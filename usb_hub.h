/**
 * @file usb_hub.h
 * @brief USB hub class driver.
 *
 * After a hub is enumerated by @ref usb.c it is handed here: the driver reads
 * the hub descriptor, powers every downstream port and registers the hub. On
 * every USB service pass @ref usb_hub_poll re-reads each downstream port's
 * status, so devices attached or removed while the system runs are enumerated
 * and torn down on the fly (hot-plug). Detection is by polling GET_STATUS; the
 * hub's status-change interrupt endpoint is not used.
 */
#pragma once

#include <usb.h>

/**
 * @brief Bring up a hub, power its ports and register it for hot-plug polling.
 * @param hub   The already-configured hub device.
 * @param depth This hub's nesting depth (0 = attached to a root port).
 */
void usb_hub_init(usb_device_t *hub, int depth);

/**
 * @brief Re-scan every registered hub's downstream ports and enumerate or
 *        release devices whose connection state has changed. Rate-limited
 *        internally; call it freely from the USB thread.
 */
void usb_hub_poll(void);

/**
 * @brief Note that hub @p addr has been unplugged: unregister it and release
 *        every device still attached behind it. Called from
 *        @ref usb_release_device; a no-op if @p addr is not a known hub.
 */
void usb_hub_removed(uint8_t addr);
