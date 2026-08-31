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

/** @brief Was HID usage @p code present in an 8-byte keyboard report? */
static int report_has_key(uint8_t *rpt, uint8_t code) {
    for(int i = 2; i < 8; i++)
        if(rpt[i] == code)
            return 1;
    return 0;
}

/**
 * @brief Diff a new keyboard report against the previous one and push newly
 *        pressed keys (mods byte bit 1/5 = shift) into the keyboard ring.
 */
static void handle_keyboard(hid_dev_t *h, uint8_t *rpt, int len) {
    if(len < 3)
        return;
    int shift = (rpt[0] & 0x22) != 0;   /* L/R shift */
    for(int i = 2; i < 8 && i < len; i++) {
        uint8_t code = rpt[i];
        if(code < 4 || code >= 128)
            continue;
        if(report_has_key(h->prev, code))
            continue;   /* still held from last report */
        char c = shift ? kbd_ascii_shift[code] : kbd_ascii[code];
        if(c)
            keyboard_push_char(c);
    }
    memcpy(h->prev, rpt, 8);
}

/** @brief Apply a boot-protocol mouse report to @c mouse_info. */
static void handle_mouse(uint8_t *rpt, int len) {
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
