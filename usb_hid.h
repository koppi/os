/**
 * @file usb_hid.h
 * @brief USB HID class driver (boot protocol only) for real USB keyboards and
 *        mice.
 *
 * No report-descriptor parsing: the driver forces the HID boot protocol, so a
 * keyboard sends the fixed 8-byte report [mods, resv, key0..key5] and a mouse
 * sends [buttons, dx, dy, (wheel)]. Keyboard keys are mapped to ASCII and
 * pushed into the shared keyboard ring; mouse deltas update @c mouse_info.
 */
#pragma once

#include <usb.h>

/**
 * @brief Claim a HID interrupt-IN interface discovered during enumeration.
 * @param dev       Owning device.
 * @param iface     Interface number.
 * @param protocol  HID boot protocol (1 = keyboard, 2 = mouse, 0 = neither).
 * @param ep_addr   Interrupt-IN endpoint address.
 * @param maxlen    Endpoint wMaxPacketSize.
 */
void usb_hid_attach(usb_device_t *dev, uint8_t iface, uint8_t protocol,
                    uint8_t ep_addr, uint16_t maxlen);

/**
 * @brief Drop every HID interface owned by device @p addr and free its
 *        interrupt slots. Called when the device is unplugged.
 */
void usb_hid_detach(uint8_t addr);

/** @brief Poll every attached HID endpoint once and dispatch any new report. */
void usb_hid_poll(void);

/**
 * @brief Translate a HID boot keyboard report and push newly-pressed keys.
 * @param rpt  The 8-byte boot report [mods, resv, key0..key5].
 * @param len  Report length.
 * @param prev Caller-owned 8-byte buffer holding the previous report (updated).
 *
 * Host-controller-agnostic: the xHCI driver feeds reports here too.
 */
void usb_hid_report_keyboard(const uint8_t *rpt, int len, uint8_t prev[8]);

/** @brief Apply a HID boot mouse report [buttons, dx, dy, (wheel)] to mouse_info. */
void usb_hid_report_mouse(const uint8_t *rpt, int len);
