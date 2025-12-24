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

static uint8_t lastkey = 0;
static volatile uint8_t shift_pressed = 0;

extern void keyboard_int();

void keyboard_init() {
    install_ir(33, 0x80 | 0x0E, 0x8, &keyboard_int);
    outportb(KBD_CHECK, 0xAE);
}

void keyboard_read_key() {
    lastkey = 0;
    if(inportb(KBD_CHECK) & 1) {
        uint8_t keycode = inportb(KBD_IN);

        if (shift_pressed == 0 && (keycode == 42 || keycode == 54)) {
            shift_pressed = keycode;
        }
        // Release shifted keyboard map if shift was relieved
        if ((shift_pressed == 42 && keycode == 170) ||
            (shift_pressed == 54 && keycode == 182)) {
            shift_pressed = 0;
        }
        if (keycode > sizeof(keyboard_map)) {
            return;
        }
        if (shift_pressed > 0) {
            lastkey = shifted_keyboard_map[keycode];
        } else {
            lastkey = keyboard_map[keycode];
        }
	}
}

char keyboard_get_lastkey() {
    return lastkey;
}

void keyboard_invalidate_lastkey() {
    lastkey = 0;
}

char getchar() {
    enable_int();
    char c = 0;
    while(1) {
        c = keyboard_get_lastkey();
        if(c == 0)
            continue;
        keyboard_invalidate_lastkey();
        return c;
    }
}

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
