/**
 * @file keyboard.c
 * @brief PS/2 keyboard driver.
 *
 * The IRQ handler (@ref keyboard_read_key) decodes scancode set 1 into ASCII
 * and pushes it onto a small ring buffer; consumers drain it with
 * @ref keyboard_get_lastkey / @ref keyboard_invalidate_lastkey. The ring avoids
 * losing keys whose make+break pair arrives between two polls.
 */
#include <keyboard.h>
#include <io.h>
#include <idt.h>
#include <pit.h>
#include <printf.h>
#include <sound.h>

enum KBD_PORTS {
	KBD_CHECK = 0x64,   /* status (read) / command (write) */
	KBD_IN = 0x60,      /* data */
};

#define KBD_ST_OBF 0x01    /* output buffer full - data for us to read */
#define KBD_ST_IBF 0x02    /* input buffer full - controller still busy */

/** @brief Spin (bounded) until the controller can accept a byte. */
static int kbd_wait_write(void) {
	for (int i = 0; i < 100000; i++)
		if (!(inportb(KBD_CHECK) & KBD_ST_IBF))
			return 1;
	return 0;
}

/** @brief Spin (bounded) until the controller has a byte for us. */
static int kbd_wait_read(void) {
	for (int i = 0; i < 100000; i++)
		if (inportb(KBD_CHECK) & KBD_ST_OBF)
			return 1;
	return 0;
}

/** @brief Drain any bytes sitting in the controller output buffer. */
static void kbd_flush(void) {
	for (int i = 0; i < 32 && (inportb(KBD_CHECK) & KBD_ST_OBF); i++)
		(void) inportb(KBD_IN);
}

static void kbd_cmd(uint8_t c) { kbd_wait_write(); outportb(KBD_CHECK, c); }

static uint8_t kbd_cmd_read(uint8_t c) {
	kbd_cmd(c);
	return kbd_wait_read() ? inportb(KBD_IN) : 0xFF;
}

static void kbd_cmd_write(uint8_t c, uint8_t d) {
	kbd_cmd(c);
	kbd_wait_write();
	outportb(KBD_IN, d);
}

static const uint8_t keyboard_map[] =
{
    0,
   27, // Escape
  '1', '2', '3', '4', '5', '6', '7', '8',  '9', '0', '-',  '=', '\b',
 '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o',  'p', '[', ']',
 '\n', // Enter
    0, // Ctrl
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\',
  'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, // Shift
    0, // Print Scrn
    0, // Alt
  ' ', // Spacebar
};

static const uint8_t shifted_keyboard_map[] =
{
    0,
   27, // Escape
  '!', '@', '#', '$', '%', '^', '&', '*',  '(', ')', '_',  '+', '\b',
 '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O',  'P', '{', '}',
 '\n', // Enter
    0, // Ctrl
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',   0, '|',
  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0, // Shift
    0, // Print Scrn
    0, // Alt
  ' ', // Spacebar
};

/* Scancode set 1 make codes. A break (release) code is the make code | 0x80. */
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36

/* Multimedia keys arrive as a 0xE0 prefix byte followed by these make codes
 * (ThinkPad EC / QEMU PS/2 both send set-1 extended scancodes). */
#define SC_E0_MUTE     0x20
#define SC_E0_VOLDOWN  0x2E
#define SC_E0_VOLUP    0x30

/*
 * Decoded keystrokes are pushed here by the keyboard IRQ and drained by the
 * console. Without it a key that arrives (make + break) between two polls is
 * lost: the release IRQ would run before the consumer looked at the make.
 *
 * Single producer (the IRQ), single consumer. The size is a power of two so the
 * indices wrap with a mask; a full buffer drops the newest key.
 */
#define KBD_BUF_SIZE 128
#define KBD_BUF_MASK (KBD_BUF_SIZE - 1)

static volatile char kbd_buf[KBD_BUF_SIZE];
static volatile uint32_t kbd_head = 0; // next write slot  (IRQ only)
static volatile uint32_t kbd_tail = 0; // next read slot   (consumer only)

/* bit 0: left shift held, bit 1: right shift held */
static volatile uint8_t shift_state = 0;

/* Set for one scancode after the 0xE0 prefix byte (extended-key marker). */
static volatile uint8_t kbd_e0 = 0;

/** asm IRQ stub (keyboard_asm) that calls @ref keyboard_read_key. */
extern void keyboard_int();

/** @brief Push one decoded character onto the ring (drops it if full). */
static void kbd_buf_push(char c) {
    uint32_t next = (kbd_head + 1) & KBD_BUF_MASK;
    if(next == kbd_tail)
        return; // buffer full, drop this key
    kbd_buf[kbd_head] = c;
    kbd_head = next;
}

/**
 * @brief Bring up the 8042 controller, reset the keyboard and install IRQ 1.
 *
 * The old init just poked 0xAE and trusted the firmware to have configured the
 * controller. A UEFI boot leaves the i8042 in an unpredictable state (the
 * firmware's own PS/2 driver may have disabled translation or left a device
 * mid-command), so do the full sequence: disable both ports, flush, write a
 * known config byte (IRQ1+IRQ12 on, translation on so we keep decoding
 * scancode set 1), self-test, then enable and reset the keyboard.
 *
 * Runs with interrupts still masked during early boot, so the reset/enable
 * ACKs are drained by kbd_flush() rather than the IRQ handler.
 */
void keyboard_init() {
    kbd_head = kbd_tail = 0;
    shift_state = 0;

    kbd_cmd(0xAD);            /* disable first (keyboard) port  */
    kbd_cmd(0xA7);            /* disable second (mouse) port    */
    kbd_flush();

    uint8_t cfg = kbd_cmd_read(0x20);
    if (cfg == 0xFF)          /* no controller responded */
        cfg = 0;
    cfg |=  (1 << 0) | (1 << 1) | (1 << 6);   /* IRQ1, IRQ12, translation */
    cfg &= ~((1 << 4) | (1 << 5));            /* both port clocks enabled */
    kbd_cmd_write(0x60, cfg);

    kbd_cmd(0xAA);                            /* controller self-test */
    if (kbd_wait_read() && inportb(KBD_IN) == 0x55)
        kbd_cmd_write(0x60, cfg);             /* self-test can wipe the config */

    kbd_cmd(0xAE);                            /* enable keyboard port */
    kbd_cmd(0xA8);                            /* enable mouse port (mouse_init finishes) */

    kbd_wait_write(); outportb(KBD_IN, 0xFF); /* reset keyboard (-> FA AA)     */
    kbd_flush();
    kbd_wait_write(); outportb(KBD_IN, 0xF4); /* enable scanning (-> FA)       */
    kbd_flush();

    install_ir(33, 0x80 | 0x0E, 0x8, &keyboard_int);
}

/**
 * @brief IRQ-context: read one scancode, track shift, and buffer the ASCII
 *        value of make codes that map to a printable character.
 */
void keyboard_read_key() {
    if(!(inportb(KBD_CHECK) & 1))
        return;

    uint8_t code = inportb(KBD_IN);

    if(code == 0xE0) {          /* extended-key prefix: the next byte is the key */
        kbd_e0 = 1;
        return;
    }

    if(kbd_e0) {
        kbd_e0 = 0;
        if(!(code & 0x80)) {   /* extended make code (break codes ignored) */
            switch(code) {
                case SC_E0_VOLUP:   sound_volume_up();   break;
                case SC_E0_VOLDOWN: sound_volume_down(); break;
                case SC_E0_MUTE:    sound_mute_toggle(); break;
                default: break;   /* arrows, nav cluster, ... not decoded yet */
            }
        }
        return;
    }

    if(code & 0x80) {
        // Break (release) code.
        uint8_t make = code & 0x7F;
        if(make == SC_LSHIFT)
            shift_state &= ~1u;
        else if(make == SC_RSHIFT)
            shift_state &= ~2u;
        return;
    }

    // Make (press) code.
    if(code == SC_LSHIFT) {
        shift_state |= 1u;
        return;
    }
    if(code == SC_RSHIFT) {
        shift_state |= 2u;
        return;
    }
    if(code >= sizeof(keyboard_map))
        return;

    char c = shift_state ? shifted_keyboard_map[code] : keyboard_map[code];
    if(c)
        kbd_buf_push(c);
}

/**
 * @brief Inject a decoded character into the keyboard ring.
 *
 * Used by the USB HID keyboard driver, which does its own HID-usage → ASCII
 * translation and feeds the result here so console input is source-agnostic.
 */
void keyboard_push_char(char c) {
    if(c)
        kbd_buf_push(c);
}

/** @brief Peek the oldest buffered keystroke without consuming it (0 if none). */
char keyboard_get_lastkey() {
    if(kbd_head == kbd_tail)
        return 0;
    return kbd_buf[kbd_tail];
}

/** @brief Consume the keystroke last returned by @ref keyboard_get_lastkey. */
void keyboard_invalidate_lastkey() {
    if(kbd_head != kbd_tail)
        kbd_tail = (kbd_tail + 1) & KBD_BUF_MASK;
}

/** @brief Spin until a key is buffered, then return and consume it. */
char getchar() {
    enable_int();
    while(1) {
        char c = keyboard_get_lastkey();
        if(c == 0)
            continue;
        keyboard_invalidate_lastkey();
        return c;
    }
}

/**
 * @brief Block until one key is buffered, then return and consume it.
 *
 * The quiet counterpart of @ref getchar for a userspace line editor (backs the
 * getkey syscall): it neither logs nor toggles the interrupt flag — a syscall
 * runs through a trap gate with interrupts already enabled — so it can be
 * called once per keystroke without flooding the console.
 */
char keyboard_getkey(void) {
    char c;
    while((c = keyboard_get_lastkey()) == 0)
        __asm__ volatile("pause");
    keyboard_invalidate_lastkey();
    return c;
}

/**
 * @brief Read an echoed line of at most @p size-1 bytes into @p str (backs the
 *        scanf syscall).
 *
 * Disables preemption while reading. Accepts printable ASCII (32-122), handles
 * backspace, and terminates on newline.
 */
void gets(char *str, size_t size) {
    int count = 0;
    char c;

    enable_int();
    sched_state(0);
    while(1) {
        c = keyboard_get_lastkey();
        if(c == 0)
            continue;
        keyboard_invalidate_lastkey();
        if(c == 4 && count == 0) {   /* Ctrl-D on an empty line: end-of-input */
            str[0] = 4;
            str[1] = '\0';
            break;
        }
        if(((int) c >= 32) && ((int) c <= 122) && count < (int)size - 1)
            str[count++] = c;
        else if(c == '\b')
            if(count > 0)
                count--;
        printf("%c", c);
        if(c == '\n') {
            str[count] = '\0';
            break;
        }
    }
    sched_state(1);
}
