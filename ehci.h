/**
 * @file ehci.h
 * @brief EHCI (USB 2.0) host-controller driver — polled, HID boot devices only.
 *
 * The ThinkPad X220 (and every Sandy/Ivy Bridge ThinkPad) is **EHCI**, not
 * xHCI: the chipset has no USB 3.0. Its internal keyboard / TrackPoint are PS/2,
 * so this driver — like @ref xhci.c on the newer machines — exists for
 * *external* USB keyboards and mice. It does the BIOS→OS handoff, resets the
 * controller, drives the async schedule for control transfers and the periodic
 * schedule for interrupt-IN, walks one or two levels of hub (the 6-series PCH
 * puts a Rate-Matching Hub between the root ports and the connectors), forces
 * the HID boot protocol and feeds reports into the same input hooks the PS/2
 * and UHCI drivers use. No mass storage, no interrupts, no isochronous.
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief PCI bind hook (class 0C:03, prog-IF 0x20): record a controller for
 *         ehci_init(). */
void ehci_probe(struct pci_device *d);

/** @brief Bring up the first usable EHCI controller and enumerate whatever is
 *         attached. Safe to call with no controller present. Called from the
 *         USB thread alongside xhci_init() / uhci_init(). */
int ehci_init(void);

/** @brief One service pass from the USB thread: poll every claimed interrupt
 *         endpoint, dispatch HID reports and re-arm; periodically re-scan the
 *         ports for hot-plug. */
void ehci_poll(void);

/** @return non-zero if an EHCI controller was brought up. */
int ehci_present(void);
