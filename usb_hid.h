/**
 * @file usb_hid.h
 * @brief USB HID class driver for real USB keyboards, mice and game pads.
 *
 * Game pads are not boot-protocol devices and are handled by gamepad.c, which
 * parses their report descriptors; this file only claims their interrupt
 * endpoint and passes it the reports (see @ref usb_hid_attach).
 *
 * The driver asks for the HID boot protocol, which is the fixed 8-byte
 * keyboard report [mods, resv, key0..key5] and the [buttons, dx, dy, (wheel)]
 * mouse report. Keyboard keys are mapped to ASCII and pushed into the shared
 * keyboard ring; mouse deltas update @c mouse_info.
 *
 * A keyboard is free to answer that request with its own reports instead: one
 * whose report descriptor declares report IDs has a report ID in front of
 * every report, so its modifier bitmap is one byte further on than a boot
 * report's and every key in the array shifts by two. A MacBook Air 2013's
 * topcase keyboard does exactly that. The descriptor is not parsed any
 * further than this -- whether it declares a report ID at all -- which is all
 * it takes to know where to read the modifiers.
 */
#pragma once

#include <usb.h>

/** Size of the buffer to fetch a HID report descriptor into. The longest one a
 *  keyboard sends is well under this, and a short read is not an error that the
 *  report parser above can survive. */
#define HID_REPORT_DESC_MAX 128

/**
 * @brief Claim a HID interrupt-IN interface discovered during enumeration.
 * @param dev        Owning device.
 * @param iface      Interface number.
 * @param protocol   HID boot protocol (1 = keyboard, 2 = mouse, 0 = neither).
 * @param ep_addr    Interrupt-IN endpoint address.
 * @param maxlen     Endpoint wMaxPacketSize.
 * @param rdesc_len  Length of the interface's report descriptor, from its HID
 *                   descriptor (0 if the configuration did not give one).
 *
 * An interface that is neither a boot keyboard nor a boot mouse is read for a
 * report descriptor and, if that describes a joystick or game pad, claimed as
 * one (see gamepad_attach()); anything else is left alone.
 */
void usb_hid_attach(usb_device_t *dev, uint8_t iface, uint8_t protocol,
                    uint8_t ep_addr, uint16_t maxlen, uint16_t rdesc_len);

/**
 * @brief Claim the interrupt-IN endpoint of an Xbox 360 controller's gamepad
 *        interface (see @ref USB_IS_XINPUT and gamepad_attach_xinput()).
 */
void usb_hid_attach_xinput(usb_device_t *dev, uint8_t iface, uint8_t ep_addr,
                           uint16_t maxlen);

/**
 * @brief Drop every HID interface owned by device @p addr and free its
 *        interrupt slots. Called when the device is unplugged.
 */
void usb_hid_detach(uint8_t addr);

/** @brief Poll every attached HID endpoint once and dispatch any new report. */
void usb_hid_poll(void);

/**
 * @brief Log every HID report to the console as it arrives ("hid" command).
 * @param on  1 to start dumping, 0 to stop.
 *
 * The boot-protocol assumption (report byte 0 is the modifier bitmap) is the
 * one thing this driver cannot verify for itself: it never reads the report
 * descriptor, so a keyboard that answers SET_PROTOCOL(boot) with something else
 * looks exactly like a keyboard whose modifiers are all released. Dumping the
 * reports is what tells the two apart.
 */
void usb_hid_set_watch(int on);

/** @brief Is the "hid" report dump currently on? */
int usb_hid_watching(void);

/**
 * @brief Which layout a keyboard's reports arrive in.
 *
 * @c report_id is what the descriptor says, and what the driver starts out
 * assuming; @c boot_misses counts the reports that contradict it, because a
 * keyboard may declare report IDs and still send boot reports (see
 * usb_hid_report_keyboard()).
 */
typedef struct {
    uint8_t report_id;    /**< 1 = every report carries a leading report ID */
    uint8_t boot_misses;  /**< consecutive reports that looked like boot ones */
} hid_layout_t;

/**
 * @brief Does this HID report descriptor declare report IDs?
 * @param desc  Report descriptor bytes, as returned by a HID GET_DESCRIPTOR.
 * @param len   Bytes in @p desc.
 * @return 1 if any item is a Report ID, 0 if not (or if @p desc is unusable).
 *
 * Just walks the short items looking for the Report ID global tag. Parsing a
 * keyboard's usage pages in full would be a HID parser; this is the one bit
 * of it that decides whether the driver reads the modifier bitmap at all.
 */
int usb_hid_report_uses_report_id(const uint8_t *desc, int len);

/**
 * @brief Translate a HID keyboard report and push newly-pressed keys.
 * @param rpt     The report, boot [mods, resv, key0..key5] or report-ID
 *                prefixed [id, mods, resv, key0..].
 * @param len     Report length.
 * @param prev    Caller-owned 8-byte buffer holding the previous report in boot
 *                layout (updated).
 * @param layout  Per-keyboard layout, updated from what the reports show.
 *
 * Host-controller-agnostic: the xHCI driver feeds reports here too. A layout
 * the descriptor did not predict is corrected from the reserved byte, which is
 * zero in both layouts and cannot be anything else.
 */
void usb_hid_report_keyboard(const uint8_t *rpt, int len, uint8_t prev[8],
                             hid_layout_t *layout);

/** @brief Apply a HID boot mouse report [buttons, dx, dy, (wheel)] to mouse_info. */
void usb_hid_report_mouse(const uint8_t *rpt, int len);
