/**
 * @file usb_hid.c
 * @brief USB HID boot-protocol driver: real USB keyboards and mice.
 */
#include <usb_hid.h>
#include <uhci.h>
#include <keyboard.h>
#include <mouse.h>
#include <log.h>
#include <lib/string.h>

#define MAX_HID 4

/** One attached HID interrupt endpoint. */
typedef struct {
    int      used;
    int      slot;          /**< UHCI interrupt-poll slot. */
    uint8_t  addr;          /**< Owning device address (for detach). */
    uint8_t  protocol;      /**< 1 = keyboard, 2 = mouse. */
    uint8_t  prev[8];       /**< Previous keyboard report (for edge detection). */
} hid_dev_t;

static hid_dev_t hid[MAX_HID];

/* --- HID keyboard usage (0x04..) -> ASCII, unshifted / shifted --- */
static const char kbd_ascii[128] = {
    /* 00 */ 0, 0, 0, 0,
    /* 04 */ 'a','b','c','d','e','f','g','h','i','j','k','l','m',
    /* 11 */ 'n','o','p','q','r','s','t','u','v','w','x','y','z',
    /* 1e */ '1','2','3','4','5','6','7','8','9','0',
    /* 28 */ '\n', 27, '\b', '\t', ' ', '-', '=', '[', ']', '\\', 0, ';', '\'',
    /* 35 */ '`', ',', '.', '/', 0,
    /* 3a */ 0,0,0,0,0,0,0,0,0,0,0,0,   /* F1..F12 */
    /* 46 */ 0,0,0,0,0,0,0,0,0,0,0,0,0, /* PrtSc..arrows region */
    /* 53 */ 0, '/', '*', '-', '+', '\n',
    /* 59 */ '1','2','3','4','5','6','7','8','9','0','.',
};
static const char kbd_ascii_shift[128] = {
    0, 0, 0, 0,
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '!','@','#','$','%','^','&','*','(',')',
    '\n', 27, '\b', '\t', ' ', '_', '+', '{', '}', '|', 0, ':', '"',
    '~', '<', '>', '?', 0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,
    0, '/', '*', '-', '+', '\n',
    '1','2','3','4','5','6','7','8','9','0','.',
};

/*
 * HID usage -> PS/2 scancode-set-1 make code, for the raw key stream a
 * full-screen program reads (see keyboard.h). 0 means "no equivalent"; the
 * high byte marks a key whose PS/2 form carries the 0xE0 prefix, which is how
 * the consumer tells the arrow cluster from the numeric keypad.
 *
 * Only what a game plausibly binds is filled in -- the letters, digits, the
 * arrows and the modifiers -- because this table exists for input, not for
 * text: text still comes from kbd_ascii above.
 */
#define HID_E0 0x100   /* set-1 form of this key carries the 0xE0 prefix */
static const uint16_t kbd_scan[128] = {
    /* 00 */ 0, 0, 0, 0,
    /* 04 a..z */ 0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,
    /* 11 n..z */ 0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C,
    /* 1e 1..0 */ 0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,
    /* 28 */ 0x1C,0x01,0x0E,0x0F,0x39,0x0C,0x0D,0x1A,0x1B,0x2B, 0, 0x27,0x28,
    /* 35 */ 0x29,0x33,0x34,0x35,0x3A,
    /* 3a F1..F12 */ 0x3B,0x3C,0x3D,0x3E,0x3F,0x40,0x41,0x42,0x43,0x44,0x57,0x58,
    /* 46 PrtSc, ScrLk, Pause. PrintScreen and Pause are multi-byte
       sequences on PS/2 (E0 2A E0 37 / E1 1D 45 ...), which the raw ring
       does not model, so they are dropped rather than half-decoded. */
    0, 0x46, 0,
    /* 49 Ins,Home,PgUp,Del,End,PgDn */
    HID_E0|0x52, HID_E0|0x47, HID_E0|0x49, HID_E0|0x53, HID_E0|0x4F, HID_E0|0x51,
    /* 4f right,left,down,up */
    HID_E0|0x4D, HID_E0|0x4B, HID_E0|0x50, HID_E0|0x48,
    /* 53 NumLk,/,*,-,+,enter */ 0x45, HID_E0|0x35, 0x37, 0x4A, 0x4E, HID_E0|0x1C,
    /* 59 keypad 1..0,. */ 0x4F,0x50,0x51,0x4B,0x4C,0x4D,0x47,0x48,0x49,0x52,0x53,
};

/* Report byte 0 is a modifier bitmap; bit n maps to these make codes. */
static const uint16_t kbd_mod_scan[8] = {
    0x1D,        /* left ctrl  */
    0x2A,        /* left shift */
    0x38,        /* left alt   */
    HID_E0 | 0x5B,   /* left GUI   */
    HID_E0 | 0x1D,   /* right ctrl */
    0x36,        /* right shift*/
    HID_E0 | 0x38,   /* right alt  */
    HID_E0 | 0x5C,   /* right GUI  */
};

/** @brief Feed one usage's press/release into the raw scancode stream. */
static void push_scan_usage(uint8_t usage, int release) {
    if(usage >= 128)
        return;
    uint16_t sc = kbd_scan[usage];
    if(sc)
        keyboard_push_scan((uint8_t) (sc & 0xFF), (sc & HID_E0) != 0, release);
}

/** @brief Was HID usage @p code present in an 8-byte keyboard report? */
static int report_has_key(const uint8_t *rpt, uint8_t code) {
    for(int i = 2; i < 8; i++)
        if(rpt[i] == code)
            return 1;
    return 0;
}

/**
 * @brief Diff a new keyboard report against the previous one and push newly
 *        pressed keys (mods byte bit 1/5 = shift) into the keyboard ring.
 */
void usb_hid_report_keyboard(const uint8_t *rpt, int len, uint8_t prev[8]) {
    if(len < 3)
        return;
    int shift = (rpt[0] & 0x22) != 0;   /* L/R shift */
    for(int i = 2; i < 8 && i < len; i++) {
        uint8_t code = rpt[i];
        if(code < 4 || code >= 128)
            continue;
        if(report_has_key(prev, code))
            continue;   /* still held from last report */
        char c = shift ? kbd_ascii_shift[code] : kbd_ascii[code];
        if(c)
            keyboard_push_char(c);
        push_scan_usage(code, 0);
    }

    /*
     * The raw stream also needs the two things the ASCII path above drops: a
     * key going *up*, and the modifier keys (which live in byte 0, not in the
     * usage array at all). Without this a USB keyboard could move the player
     * forward but never stop, and could not strafe or run.
     */
    for(int i = 2; i < 8; i++) {
        uint8_t code = prev[i];
        if(code >= 4 && code < 128 && !report_has_key(rpt, code))
            push_scan_usage(code, 1);
    }
    uint8_t changed = (uint8_t) (rpt[0] ^ prev[0]);
    for(int b = 0; b < 8; b++) {
        if(!(changed & (1u << b)))
            continue;
        uint16_t sc = kbd_mod_scan[b];
        keyboard_push_scan((uint8_t) (sc & 0xFF), (sc & HID_E0) != 0,
                           !(rpt[0] & (1u << b)));
    }

    memcpy(prev, (void *)rpt, 8);
}

void usb_hid_report_mouse(const uint8_t *rpt, int len) {
    if(len < 3)
        return;
    int8_t dx = (int8_t)rpt[1];
    int8_t dy = (int8_t)rpt[2];
    mouse_info.x += dx;
    mouse_info.y += dy;   /* HID dy is +down, which matches our screen y */
    mouse_check_bounds();

    uint8_t b = rpt[0];
    if(b & 0x01)      mouse_info.curr_button = LEFT_CLICK;
    else if(b & 0x02) mouse_info.curr_button = RIGHT_CLICK;
    else if(b & 0x04) mouse_info.curr_button = MIDDLE_CLICK;
    else              mouse_info.curr_button = 0;
}

static void handle_keyboard(hid_dev_t *h, uint8_t *rpt, int len) {
    usb_hid_report_keyboard(rpt, len, h->prev);
}

/** @brief Apply a boot-protocol mouse report to @c mouse_info. */
static void handle_mouse(uint8_t *rpt, int len) {
    usb_hid_report_mouse(rpt, len);
}

/** @brief SET_PROTOCOL(boot) + SET_IDLE(0) on @p iface. */
static void hid_set_boot(usb_device_t *dev, uint8_t iface) {
    usb_setup_t s;

    s = (usb_setup_t){
        .bmRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
        .bRequest = HID_REQ_SET_PROTOCOL, .wValue = HID_PROTO_BOOT,
        .wIndex = iface, .wLength = 0,
    };
    usb_control(dev, &s, 0, 0);

    s = (usb_setup_t){
        .bmRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
        .bRequest = HID_REQ_SET_IDLE, .wValue = 0,
        .wIndex = iface, .wLength = 0,
    };
    usb_control(dev, &s, 0, 0);
}

void usb_hid_attach(usb_device_t *dev, uint8_t iface, uint8_t protocol,
                    uint8_t ep_addr, uint16_t maxlen) {
    if(protocol != HID_PROTOCOL_KEYBOARD && protocol != HID_PROTOCOL_MOUSE) {
        klogf(LOG_INFO, "USB HID: iface %u protocol %u not supported\n",
              iface, protocol);
        return;
    }

    int idx = -1;
    for(int i = 0; i < MAX_HID; i++)
        if(!hid[i].used) { idx = i; break; }
    if(idx < 0)
        return;

    hid_set_boot(dev, iface);

    int slot = uhci_int_claim(dev, ep_addr, maxlen ? maxlen : 8);
    if(slot < 0) {
        klogf(LOG_ERR, "USB HID: no free interrupt slot\n");
        return;
    }

    hid[idx].used     = 1;
    hid[idx].slot     = slot;
    hid[idx].addr     = dev->address;
    hid[idx].protocol = protocol;
    memset(hid[idx].prev, 0, sizeof(hid[idx].prev));

    klogf(LOG_INFO, "USB HID: %s ready (dev %u, ep 0x%x)\n",
          protocol == HID_PROTOCOL_KEYBOARD ? "keyboard" : "mouse",
          dev->address, ep_addr);
}

void usb_hid_detach(uint8_t addr) {
    for(int i = 0; i < MAX_HID; i++) {
        if(hid[i].used && hid[i].addr == addr) {
            uhci_int_release(hid[i].slot);
            klogf(LOG_INFO, "USB HID: %s gone (dev %u)\n",
                  hid[i].protocol == HID_PROTOCOL_KEYBOARD ? "keyboard" : "mouse",
                  addr);
            memset(&hid[i], 0, sizeof(hid[i]));
        }
    }
}

void usb_hid_poll(void) {
    uint8_t rpt[16];
    for(int i = 0; i < MAX_HID; i++) {
        if(!hid[i].used)
            continue;
        int n = uhci_int_poll(hid[i].slot, rpt, sizeof(rpt));
        if(n <= 0)
            continue;
        if(hid[i].protocol == HID_PROTOCOL_KEYBOARD)
            handle_keyboard(&hid[i], rpt, n);
        else
            handle_mouse(rpt, n);
    }
}
