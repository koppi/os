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

void uart_handler(void) {
    /* Drain the whole RX FIFO: one IRQ can cover several buffered bytes. */
    while (inportb(UART_PORT + 5) & 1) {
        char c = (char) inportb(UART_PORT + 0);

        /* Legacy line buffer (drained by uart_read); silently drop on overflow
         * - logging here would recurse through the console on an input flood. */
        if (rbpos < RECVBUF_LEN)
            recvbuf[rbpos++] = c;

        if (c == 27) {
            exit_qemu(0);
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
