/**
 * @file uart.c
 * @brief 16550 UART (COM1) driver and the @ref chardev_t wrapper that makes it
 *        the kernel console / log sink during boot.
 */
#include <types.h>

#include <uart.h>

#include <io.h>
#include <idt.h>
#include <keyboard.h>
#include <lib/string.h>


static uint8_t uart_initialized = 0;
static const uint16_t UART_PORT = 0x3f8;
static const uint16_t UART_PORT_CONTROL = 0x3f8 + 5;

static int uart_read(struct chardev_struct *dev, char *buf, size_t nbyte);
static int uart_write(chardev_t *dev, const char *buf, size_t nbyte);

#define RECVBUF_LEN 64
static char recvbuf[RECVBUF_LEN];
static int rbpos = 0;

chardev_t uartdev = { uart_read, uart_write };

extern void uart_int();

#define BAUD_RATE (9600)
#define BASE_BAUD_RATE (115200)
#define DLAB_FLAG (0x80)
#define MODE_8N1 (3)

/* Bound on every "wait for the transmitter" spin. A modern ThinkPad (T470s and
 * up) has no wired-out COM1, but the PCH still decodes 0x3F8 and the LSR reads
 * back as something other than 0xFF with the THR-empty bit (0x20) stuck low --
 * so the old "if LSR == 0xFF, no port" probe trusted it and the very first
 * klogf() spun here forever, a black screen before anything could paint. */
#define UART_TX_SPIN 200000

/**
 * @brief Probe for a real 16550 with a loopback test, then, if present,
 *        program it for 9600 8N1 with FIFOs on.
 *
 * The loopback test (MCR bit 4) is the reliable "is a UART actually here"
 * check: a dead/absent port does not echo the byte back. Only then is
 * @ref uart_initialized set and klogf() output actually written.
 */
void uart_init(void) {
  uart_initialized = 0;

  // We need to specify ratio between baud rate and the base crystal
  // oscillation rate of a 16550 UART serial chip.
  uint16_t ratio = BAUD_RATE / BASE_BAUD_RATE;

  // Disable all interrupts.
  outportb(UART_PORT + 1, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outportb(UART_PORT + 3, DLAB_FLAG); // Enable DLAB - 'Divisor Latch Access Bit'. Lets
			              // us set baud rate.
  outportb(UART_PORT + 0, ratio);
  outportb(UART_PORT + 3, MODE_8N1); // Lock divisor, 8 data bits.
  outportb(UART_PORT + 4, 0x0B);     // Interrupt enable and DTR,RTS high

  // Enable and clear the 16550 FIFOs, RX trigger level 14 bytes (FCR).
  // A receive FIFO lets the console survive a burst of pasted/scripted input
  // that arrives faster than the RX IRQ can drain it one byte at a time.
  outportb(UART_PORT + 2, 0xC7);

  // Loopback self-test: put the UART in loopback (MCR bit 4), send a pattern,
  // and require it back. A port that reads 0xFF, or one the chipset half-
  // decodes but does not clock, fails this and stays disabled.
  outportb(UART_PORT + 4, 0x1E);            // LOOP | OUT2 | OUT1 | RTS
  outportb(UART_PORT + 0, 0xAE);
  int ok = 0;
  for (int i = 0; i < 100000; i++)
    if (inportb(UART_PORT + 5) & 0x01) { ok = (inportb(UART_PORT + 0) == 0xAE); break; }

  // Restore normal operation (interrupts + DTR/RTS asserted).
  outportb(UART_PORT + 4, 0x0B);

  if (!ok)
    return;                                 // no usable COM1 -- klogf is a no-op

  uart_initialized = 1;
}

void uart_rx_ir(void) {
  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  inportb(UART_PORT + 2);
  inportb(UART_PORT + 0);

  install_ir(32 + 4, 0x80 | 0x0E, 0x8, &uart_int);

  // Enable receive interrupts.
  outportb(UART_PORT + 1, 0x01);
}

int uart_getc(void) {
    if (!uart_initialized)
        return -1;
    for (int i = 0; i < UART_TX_SPIN; i++)
        if (inportb(UART_PORT + 5) & 1)
            return inportb(UART_PORT + 0);
    return -1;   // rx stayed empty
}

/*
 * Escape-sequence state for the console input ring: 0 = idle, 1 = an ESC has
 * arrived, 2 = inside a CSI/SS3 sequence waiting for its final byte.
 *
 * A terminal sends a cursor key as ESC [ A (or ESC O A once an application has
 * turned on the alternate cursor-key mode), three bytes arriving back to back.
 * Forwarded raw they were three keystrokes to whatever was reading, which is
 * how an arrow key used to insert `[A` into a command line over a serial
 * console; the kernel's own keyboards do not have this problem because they
 * decode a cursor key straight to one control character (keyboard.h). So the
 * sequence is reassembled here and handed on as that same character, which
 * also means the shell's editor needs to know only one form of a cursor key
 * whether the keystroke came from a terminal, a PS/2 controller or USB.
 */
static int esc_state;

/** @brief CSI/SS3 final byte -> the console's character for that cursor key. */
static char csi_final_to_char(char f) {
    switch (f) {
        case 'A': return KBD_CH_UP;
        case 'B': return KBD_CH_DOWN;
        case 'C': return KBD_CH_RIGHT;
        case 'D': return KBD_CH_LEFT;
        case 'H': return KBD_CH_HOME;   /* xterm Home */
        case 'F': return KBD_CH_END;    /* xterm End  */
        default:  return 0;             /* a sequence the console has no key for */
    }
}

void uart_handler(void) {
    /* Drain the whole RX FIFO: one IRQ can cover several buffered bytes. */
    while (inportb(UART_PORT + 5) & 1) {
        char c = (char) inportb(UART_PORT + 0);

        /* Legacy line buffer (drained by uart_read); silently drop on overflow
         * - logging here would recurse through the console on an input flood.
         * This keeps the raw bytes, escape sequences included. */
        if (rbpos < RECVBUF_LEN)
            recvbuf[rbpos++] = c;

        if (esc_state == 1) {
            esc_state = 0;
            if (c == '[' || c == 'O') {     /* CSI / SS3: a cursor key follows */
                esc_state = 2;
                continue;
            }
            /*
             * A bare ESC. Two in a row is the shortcut that shuts the machine
             * down (and with it QEMU, through the isa-debug-exit device): it
             * used to be a single ESC, but that cannot coexist with a terminal
             * whose cursor keys *start* with ESC -- pressing Up would quit.
             * Press Esc twice instead. Anything else: the ESC was not the
             * start of a sequence, so drop it and handle this byte normally.
             */
            if (c == 27) {
                exit_qemu(0);
            }
        } else if (esc_state == 2) {
            /*
             * Inside a sequence. The parameter bytes of a longer one (`ESC [
             * 3 ~` for Delete, `ESC [ 1 ; 5 C` for Ctrl-Right) are 0x30-0x3F;
             * the first byte outside that range ends it. Only the sequences
             * with a key of their own reach the ring, so an unrecognised one
             * is swallowed whole rather than arriving as its own tail.
             */
            if ((unsigned char) c >= 0x30 && (unsigned char) c <= 0x3F)
                continue;               /* parameter / intermediate byte */
            esc_state = 0;
            char key = csi_final_to_char(c);
            if (key)
                keyboard_push_char(key);
            continue;
        }

        if (c == 27) {
            esc_state = 1;
            continue;
        }

        /* Feed the byte into the shared console input ring (the same hook the
         * USB keyboard uses) so the kernel console can be driven over a bare
         * serial link with no keyboard attached. A terminal sends CR for Enter
         * and DEL for Backspace; the console wants LF and BS. */
        if (c == '\r')
            c = '\n';
        else if (c == 0x7f)
            c = '\b';
        keyboard_push_char(c);
    }
}

#define LSR_THR (0x20)

uint8_t uart_tx_empty(void) {
  return inportb(UART_PORT_CONTROL) & LSR_THR;
}

void uart_putc(char c) {
    if (!uart_initialized)
        return;   // no COM1 -- do not touch the port, never spin
    for (int i = 0; i < UART_TX_SPIN && !uart_tx_empty(); i++)
        __builtin_ia32_pause();
    outportb(UART_PORT, c);
}

static int uart_read(struct chardev_struct *dev, char *buf, size_t nbyte) {
    (void)dev;

    if (!uart_initialized)
        return 0;   // no COM1 -- never block waiting for an RX IRQ that can't come

    size_t read = 0, rs;

    while (1) {
        if (rbpos > 0) {
            if (rbpos <= (int)(nbyte - read)) {
                rs = rbpos;
            } else {
                rs = nbyte - read;
            }
            memcpy(buf, recvbuf, rs);
            read += rbpos;
            buf += rbpos;
            if (rbpos - rs > 0)
                memmove(recvbuf, recvbuf + rs, rbpos - rs);
            rbpos -= rs;
        }
        
        if (read >= nbyte)
            break;
        
        /* Wait for an interrupt */
        asm volatile("hlt":::"memory");
    }

    return read;
}

static int uart_write(chardev_t *dev, const char *buf, size_t nbyte) {
    (void)dev;
    
    size_t i;
    
    for (i=0; i < nbyte; i++) {
        uart_putc(buf[i]);
    }
    
    return i;
}
