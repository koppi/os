/**
 * @file uhci.h
 * @brief Intel UHCI (USB 1.1) host-controller driver — the transport layer
 *        under @ref usb.c.
 *
 * All schedule structures (frame list, queue heads, transfer descriptors) and
 * the transfer data buffers live in the identity-mapped low 4 MiB, so their
 * virtual address equals their physical address, which is what the controller
 * needs.
 */
#pragma once

#include <types.h>
#include <usb.h>

/** @name UHCI I/O register offsets from the controller's base I/O port */
///@{
#define UHCI_USBCMD     0x00  /**< Command (word). */
#define UHCI_USBSTS     0x02  /**< Status (word). */
#define UHCI_USBINTR    0x04  /**< Interrupt enable (word). */
#define UHCI_FRNUM      0x06  /**< Frame number (word). */
#define UHCI_FRBASEADD  0x08  /**< Frame-list base address (dword, 4 KiB aligned). */
#define UHCI_SOFMOD     0x0C  /**< Start-of-frame modify (byte). */
#define UHCI_PORTSC1    0x10  /**< Root port 1 status/control (word). */
#define UHCI_PORTSC2    0x12  /**< Root port 2 status/control (word). */
///@}

/** @name USBCMD bits */
///@{
#define UHCI_CMD_RS       0x0001  /**< Run/Stop. */
#define UHCI_CMD_HCRESET  0x0002  /**< Host controller reset. */
#define UHCI_CMD_GRESET   0x0004  /**< Global reset. */
#define UHCI_CMD_MAXP     0x0080  /**< Max packet 64 (vs 32). */
#define UHCI_CMD_CF       0x0040  /**< Configure flag. */
///@}

/** @name PORTSC bits */
///@{
#define UHCI_PORT_CCS    0x0001  /**< Current connect status. */
#define UHCI_PORT_CSC    0x0002  /**< Connect status change (write 1 to clear). */
#define UHCI_PORT_PE     0x0004  /**< Port enabled. */
#define UHCI_PORT_PEC    0x0008  /**< Port enable change (write 1 to clear). */
#define UHCI_PORT_LSDA   0x0100  /**< Low-speed device attached. */
#define UHCI_PORT_PR     0x0200  /**< Port reset. */
///@}

/** @name Transfer-descriptor PID values */
///@{
#define UHCI_PID_SETUP  0x2D
#define UHCI_PID_IN     0x69
#define UHCI_PID_OUT    0xE1
///@}

/**
 * @brief Find the UHCI controller on the PCI bus, reset it and start its
 *        schedule.
 * @return 1 on success, 0 if no UHCI controller is present.
 */
int uhci_init(void);

/** @return Number of root ports (2). */
int uhci_port_count(void);

/**
 * @brief Reset and enable root port @p port.
 * @param port   0-based port index.
 * @param speed  Out: the attached device's speed.
 * @return 1 if a device is connected and the port is enabled, else 0.
 */
int uhci_port_reset(int port, usb_speed_t *speed);

/**
 * @brief Run one control transfer through endpoint 0.
 * @see usb_control
 */
int uhci_control(usb_device_t *dev, const usb_setup_t *setup, void *data, int len);

/**
 * @brief Claim an interrupt-IN endpoint so @ref uhci_int_poll can service it.
 * @param dev      Owning device.
 * @param endpoint Endpoint number (without the direction bit).
 * @param maxlen   wMaxPacketSize of the endpoint.
 * @return A slot handle (>= 0), or -1 if no slot is free.
 */
int uhci_int_claim(usb_device_t *dev, uint8_t endpoint, int maxlen);

/**
 * @brief Poll a claimed interrupt endpoint for a new report.
 * @param slot Handle from @ref uhci_int_claim.
 * @param buf  Destination for the report.
 * @param len  Size of @p buf.
 * @return Bytes received (0 if nothing new), or negative on a transfer error.
 */
int uhci_int_poll(int slot, void *buf, int len);
