/**
 * @file bcm5974.c
 * @brief Apple BCM5974 (Wellspring) multi-touch trackpad driver.
 *
 * The BCM5974 is used in MacBook Air/Pro (2012-2015 era) as the internal
 * trackpad controller. It speaks a custom binary format over USB interrupt
 * endpoint 0x83 (not HID), and requires a mode switch control transfer to
 * enable "wellspring mode" (multi-touch).
 *
 * For MacBook Air 6,2 (2013, wellspring 8): TYPE3 trackpad format
 *   Endpoint: 0x83 (interrupt IN)
 *   Header: 38 bytes (19 * uint16_t)
 *   Button: at offset 46 bytes (23 * uint16_t) - integrated button
 *   Finger block: 28 bytes (14 * uint16_t) per finger
 *   Max fingers: 16
 *   X range: -4620..5140, Y range: -150..6600
 *
 * Finger structure (little-endian uint16_t):
 *   origin, abs_x, abs_y, rel_x, rel_y, tool_major, tool_minor,
 *   orientation, touch_major, touch_minor, unused[2], pressure, multi
 */
#include <bcm5974.h>
#include <mouse.h>
#include <log.h>
#include <lib/string.h>
#include <usb.h>

/* BCM5974 Wellspring 8 (MacBookAir6,2) TYPE3 constants */
#define BCM5974_TYPE3_HEADER    38      /* 19 * 2 bytes */
#define BCM5974_TYPE3_BUTTON    46      /* 23 * 2 bytes */
#define BCM5974_TYPE3_FSIZE     28      /* 14 * 2 bytes per finger */
#define BCM5974_MAX_FINGERS     16

#define BCM5974_X_MIN (-4620)
#define BCM5974_X_MAX (5140)
#define BCM5974_Y_MIN (-150)
#define BCM5974_Y_MAX (6600)

static int bcm5974_finger_down = 0;
static int bcm5974_last_x = 0;
static int bcm5974_last_y = 0;
static int bcm5974_button_state = 0;

/** @brief Convert little-endian uint16_t to signed int. */
static inline int le16_to_int(const uint8_t *p) {
    return (int16_t)(p[0] | (p[1] << 8));
}

/** @brief Apply absolute trackpad coordinates to relative mouse deltas. */
static void bcm5974_apply_motion(int x, int y, int button_state) {
    if (!bcm5974_finger_down) {
        bcm5974_last_x = x;
        bcm5974_last_y = y;
        bcm5974_finger_down = 1;
        bcm5974_button_state = button_state;
        return;
    }

    int dx = x - bcm5974_last_x;
    int dy = bcm5974_last_y - y;  /* Y is inverted (trackpad origin at top) */

    /* Scale from trackpad resolution to screen pixels.
     * Trackpad range ~10000 units, screen ~1000-2000 pixels.
     * Use divisor of ~8-10 for comfortable speed. */
    dx /= 8;
    dy /= 8;

    if (dx != 0 || dy != 0) {
        mouse_info.x += dx;
        mouse_info.y += dy;
        mouse_check_bounds();
    }

    bcm5974_last_x = x;
    bcm5974_last_y = y;

    /* Button handling: trackpad is a ClickPad - integrated button in data */
    if (button_state & 0x01) {
        mouse_info.curr_button = LEFT_CLICK;
    } else {
        mouse_info.curr_button = 0;
    }
    bcm5974_button_state = button_state;
}

/** @brief Parse BCM5974 TYPE3 trackpad data. */
void bcm5974_parse_report(const uint8_t *data, int len) {
    if (len < BCM5974_TYPE3_HEADER + BCM5974_TYPE3_FSIZE)
        return;

    /* Button state at offset 46 (integrated button for TYPE3) */
    int button_state = le16_to_int(data + BCM5974_TYPE3_BUTTON) & 0x01;

    /* Finger data starts right after header (delta = 0 for TYPE3) */
    const uint8_t *finger_base = data + BCM5974_TYPE3_HEADER;
    int max_fingers = (len - BCM5974_TYPE3_HEADER) / BCM5974_TYPE3_FSIZE;
    if (max_fingers > BCM5974_MAX_FINGERS)
        max_fingers = BCM5974_MAX_FINGERS;

    int finger_found = 0;
    for (int i = 0; i < max_fingers; i++) {
        const uint8_t *f = finger_base + i * BCM5974_TYPE3_FSIZE;
        int touch_major = le16_to_int(f + 16);  /* touch_major at offset 16 */
        if (touch_major == 0)
            continue;

        int abs_x = le16_to_int(f + 2);  /* abs_x at offset 2 */
        int abs_y = le16_to_int(f + 4);  /* abs_y at offset 4 */

        /* Invert Y: trackpad origin is top-left, screen origin is bottom-left */
        abs_y = BCM5974_Y_MIN + BCM5974_Y_MAX - abs_y;

        bcm5974_apply_motion(abs_x, abs_y, button_state);
        finger_found = 1;
        break;  /* Use first valid finger */
    }

    if (!finger_found && bcm5974_finger_down) {
        bcm5974_finger_down = 0;
        mouse_info.curr_button = 0;
    }
}

/** @brief Initialize BCM5974 trackpad parsing state. */
void bcm5974_init(void) {
    bcm5974_finger_down = 0;
    bcm5974_last_x = 0;
    bcm5974_last_y = 0;
    bcm5974_button_state = 0;
    klogf(LOG_INFO, "bcm5974: initialized for wellspring 8 (TYPE3)\n");
}
