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

enum KBD_PORTS {
	KBD_CHECK = 0x64,
	KBD_IN = 0x60,
};

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

/** @brief Reset the ring, install IRQ 1 and enable the PS/2 first port. */
void keyboard_init() {
    kbd_head = kbd_tail = 0;
    shift_state = 0;
    install_ir(33, 0x80 | 0x0E, 0x8, &keyboard_int);
    outportb(KBD_CHECK, 0xAE);
}

/**
 * @brief IRQ-context: read one scancode, track shift, and buffer the ASCII
 *        value of make codes that map to a printable character.
 */
void keyboard_read_key() {
    if(!(inportb(KBD_CHECK) & 1))
        return;

    uint8_t code = inportb(KBD_IN);

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
 * @brief Read an echoed line into @p str (backs the scanf syscall).
 *
 * Disables preemption while reading. Accepts printable ASCII (32-122), handles
 * backspace, and terminates on newline.
 *
 * @param str  Destination buffer.
 * @param size Buffer size in bytes.
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
