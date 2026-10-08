/**
 * @file gamepad.h
 * @brief USB game controllers: HID report-descriptor parsing and one
 *        normalised state per pad, shared by all three host controllers.
 *
 * A gamepad is a HID device, but not a boot-protocol one: there is no fixed
 * report layout to hard-code, the way there is for a keyboard or a mouse. Every
 * pad describes its own reports in its HID report descriptor -- which bits are
 * the left stick, where the D-pad hat sits, how many buttons there are -- and
 * the only way to read one is to parse that description first. So the pad
 * driver is two halves:
 *
 *  - @ref gamepad_attach reads the descriptor once, at enumeration, and keeps
 *    the list of fields a report is made of (a handful of bit offsets);
 *  - @ref gamepad_report then applies each interrupt report to those fields
 *    and updates the pad's @ref gamepad_state_t.
 *
 * The host-controller drivers (uhci.c via usb_hid.c, ehci.c, xhci.c) do the
 * transfers; this file does not touch hardware, which is also why its decoding
 * can be exercised with nothing but byte arrays.
 *
 * What a program gets is deliberately not a mapped controller ("A", "B",
 * "LB") but what the device itself said: buttons numbered the way its own
 * descriptor numbers them, sticks scaled to a fixed range, the hat folded into
 * four direction bits. There is no database of which pad numbers which button
 * what -- SDL needs one for exactly that -- so the mapping from buttons to
 * actions is the program's, and the console's `pad` command shows the numbers
 * to map from.
 *
 * Also understood: the Xbox 360 wired controller (vendor class 0xFF/0x5D/0x01),
 * the commonest pad there is and not a HID device at all. Its fixed 20-byte
 * report is translated into the same state, with its buttons numbered the way
 * a DualShock or Logitech F310 numbers theirs (see @ref gamepad_attach_xinput).
 */
#pragma once

#include <types.h>

/** Pads tracked at once. */
#define GAMEPAD_MAX 4

/** @name Direction bits of @ref gamepad_state_t::dpad */
///@{
#define GAMEPAD_DPAD_UP    0x01
#define GAMEPAD_DPAD_DOWN  0x02
#define GAMEPAD_DPAD_LEFT  0x04
#define GAMEPAD_DPAD_RIGHT 0x08
///@}

/**
 * @brief The state of one pad -- also the layout `getpad` (syscall #45) copies
 *        out, so it is fixed-width and has no hidden padding.
 *
 * Sticks run -32768..32767 with 0 at rest, +x to the right and +y **down** (the
 * HID and screen convention, so "up" is negative). Triggers run 0..255. A
 * stick or trigger the pad does not have reads 0.
 */
typedef struct {
    uint32_t buttons;   /**< bit n-1 set = button n pressed, in the device's own numbering */
    int16_t  lx, ly;    /**< left stick */
    int16_t  rx, ry;    /**< right stick */
    uint8_t  lt, rt;    /**< analog triggers (left, right) */
    uint8_t  dpad;      /**< GAMEPAD_DPAD_* bits: the hat, or the D-pad's own buttons */
    uint8_t  reserved;
} gamepad_state_t;

/**
 * @brief Size of the staging buffer for a report descriptor, as returned by
 *        @ref gamepad_rdesc_buf.
 *
 * A DualShock 4's runs to about 500 bytes; the 128 bytes the keyboard path
 * asks for would cut it off in the middle of an item.
 */
#define GAMEPAD_RDESC_MAX 1024

/**
 * @brief A buffer of @ref GAMEPAD_RDESC_MAX bytes for a host controller to
 *        fetch a report descriptor into.
 *
 * Shared, so it does not sit on a kernel thread's stack. Enumeration is one
 * thread doing one device at a time (the same reason usb.c has a single
 * configuration-blob buffer), so it is never in use twice at once.
 */
uint8_t *gamepad_rdesc_buf(void);

/**
 * @brief Parse a HID report descriptor and, if it describes a game
 *        controller, claim a pad slot for it.
 * @param vendor,product  The device's ids, for the log.
 * @param desc,len        The report descriptor.
 * @return The pad index (0..@ref GAMEPAD_MAX - 1), or -1 if the descriptor is
 *         not a joystick / game pad (or every slot is taken).
 *
 * A controller is a Generic Desktop application collection whose usage is
 * Joystick, Game Pad or Multi-axis Controller, with something to read in it. The
 * decision is made from the descriptor alone: a mouse or keyboard interface
 * that is not boot-protocol falls through here and is turned down.
 */
int gamepad_attach(uint16_t vendor, uint16_t product, const uint8_t *desc, int len);

/**
 * @brief Claim a pad slot for an Xbox 360 wired controller (vendor class
 *        0xFF, subclass 0x5D, protocol 0x01).
 *
 * It sends the same 20-byte report on every poll and needs no initialisation,
 * so there is no descriptor to parse. Its buttons are numbered A, B, X, Y, LB,
 * RB, then the two triggers as digital buttons 7 and 8, Back, Start, the stick
 * clicks and Guide (9..13): the order of a DualShock 4 or Logitech F310, so a
 * mapping written for one of those works unchanged on this.
 *
 * @return The pad index, or -1 if every slot is taken.
 */
int gamepad_attach_xinput(uint16_t vendor, uint16_t product);

/** @brief Number of bytes in an Xbox 360 input report. */
#define GAMEPAD_XINPUT_REPORT 20

/**
 * @brief Apply one interrupt-IN report from pad @p pad to its state.
 *
 * Called from the host controller's polling loop. A report that matches no
 * field (another report ID, or one too short to hold the field) leaves the state
 * alone.
 */
void gamepad_report(int pad, const uint8_t *rpt, int len);

/** @brief The pad has been unplugged: free its slot and release everything. */
void gamepad_detach(int pad);

/**
 * @brief Copy out the state of pad @p index.
 * @return 1 if that slot holds a connected pad (and @p out is filled), else 0.
 *
 * Safe from any CPU while the USB thread is updating the pad: a read that
 * overlaps a report is retried, so it never returns half of one.
 */
int gamepad_get(int index, gamepad_state_t *out);

/** @brief How many pads are connected. */
int gamepad_count(void);

/**
 * @brief Describe pad @p index for the console: ids, which controls the
 *        descriptor gave it, and how many buttons.
 * @return 1 if the slot is in use, 0 if not (nothing written).
 */
int gamepad_describe(int index, char *buf, int size);

/** @brief Log every report to the console as it arrives ("pad dump"). */
void gamepad_set_watch(int on);
