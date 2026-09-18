/**
 * @file xhci.h
 * @brief xHCI (USB 3.x) host-controller driver — polled, HID boot devices only.
 *
 * Every recent laptop is xHCI-only (no UHCI/EHCI), the ThinkPad X250 included.
 * Its internal keyboard/TrackPoint are PS/2, so this exists for *external* USB
 * keyboards and mice: it enumerates attached devices, forces the HID boot
 * protocol and feeds reports into the same input hooks the PS/2 and UHCI
 * drivers use. No mass storage (the internal disk is AHCI), no interrupts.
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief PCI bind hook: record the xHCI controller for xhci_init(). */
void xhci_probe(struct pci_device *d);

/** @brief Bring up the controller and enumerate attached devices. Safe to
 *         call with no controller present. */
int xhci_init(void);

/** @brief One service pass from the USB thread: drain the event ring, dispatch
 *         HID reports, re-arm interrupt endpoints, handle hot-plug. */
void xhci_poll(void);

/** @return non-zero if an xHCI controller was brought up. */
int xhci_present(void);
