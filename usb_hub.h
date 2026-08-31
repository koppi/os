/**
 * @file usb_hub.h
 * @brief USB hub class driver.
 *
 * After a hub is enumerated by @ref usb.c it is handed here: the driver reads
 * the hub descriptor, powers every downstream port, and for each port that has
 * something connected it drives a port reset and recursively enumerates the
 * device via @ref usb_enumerate. Topology is scanned once at boot; the hub's
 * status-change interrupt endpoint is not used.
 */
#pragma once

#include <usb.h>

/**
 * @brief Bring up a hub and enumerate everything attached to it.
 * @param hub   The already-configured hub device.
 * @param depth This hub's nesting depth (0 = attached to a root port).
 */
void usb_hub_init(usb_device_t *hub, int depth);
