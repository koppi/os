/**
 * @file usb.h
 * @brief USB subsystem: spec constants, standard descriptors, the per-device
 *        record and the core API used by the host-controller and class drivers.
 *
 * This is a deliberately small USB 1.1 stack: one UHCI host controller
 * (@ref uhci.c), synchronous control transfers, polled interrupt-IN transfers,
 * and a single HID boot-protocol class driver (@ref usb_hid.c). Enumeration is
 * driven from a kernel thread, not from an interrupt.
 */
#pragma once

#include <types.h>

/** @name Request types (bmRequestType) */
///@{
#define USB_DIR_OUT              0x00  /**< Host → device. */
#define USB_DIR_IN              0x80  /**< Device → host. */
#define USB_TYPE_STANDARD       0x00
#define USB_TYPE_CLASS         0x20
#define USB_RECIP_DEVICE       0x00
#define USB_RECIP_INTERFACE    0x01
///@}

/** @name Standard request codes (bRequest) */
///@{
#define USB_REQ_GET_STATUS         0x00
#define USB_REQ_CLEAR_FEATURE      0x01
#define USB_REQ_SET_FEATURE        0x03
#define USB_REQ_SET_ADDRESS        0x05
#define USB_REQ_GET_DESCRIPTOR     0x06
#define USB_REQ_SET_CONFIGURATION  0x09
///@}

/** @name Descriptor types (wValue high byte) */
///@{
#define USB_DT_DEVICE        1
#define USB_DT_CONFIG        2
#define USB_DT_STRING        3
#define USB_DT_INTERFACE     4
#define USB_DT_ENDPOINT      5
#define USB_DT_HID           0x21
#define USB_DT_HID_REPORT    0x22
///@}

/** @name Class codes */
///@{
#define USB_CLASS_HID       0x03
///@}

/** @name HID class requests */
///@{
#define HID_REQ_GET_REPORT   0x01
#define HID_REQ_SET_IDLE     0x0A
#define HID_REQ_SET_PROTOCOL 0x0B
#define HID_PROTO_BOOT       0
#define HID_PROTO_REPORT     1
#define HID_SUBCLASS_BOOT    1
#define HID_PROTOCOL_KEYBOARD 1
#define HID_PROTOCOL_MOUSE    2
///@}

/** Transfer speed of a device / port. */
typedef enum { USB_SPEED_FULL = 0, USB_SPEED_LOW = 1 } usb_speed_t;

/** The 8-byte SETUP packet of a control transfer. */
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

/** Standard device descriptor (18 bytes). */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} usb_device_desc_t;

/** Standard configuration descriptor (9 bytes; followed by interfaces). */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} usb_config_desc_t;

/** Standard interface descriptor (9 bytes). */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} usb_iface_desc_t;

/** Standard endpoint descriptor (7 bytes). */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;   /**< bit 7 = direction (IN if set). */
    uint8_t  bmAttributes;       /**< bits 1:0 = transfer type (3 = interrupt). */
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} usb_endpoint_desc_t;

/** An enumerated USB device. */
typedef struct usb_device {
    uint8_t     address;         /**< Assigned USB address (1..). */
    usb_speed_t speed;
    uint8_t     max_packet0;     /**< EP0 max packet size. */
    uint16_t    vendor;
    uint16_t    product;
    int         in_use;
} usb_device_t;

/**
 * @brief Perform a synchronous control transfer on endpoint 0.
 * @param dev   Target device (address 0 during set-address).
 * @param setup The 8-byte SETUP packet.
 * @param data  Data-stage buffer (may be NULL for a zero-length data stage).
 * @param len   Data-stage length in bytes.
 * @return Number of data bytes transferred, or negative on error.
 */
int usb_control(usb_device_t *dev, const usb_setup_t *setup, void *data, int len);

/** @brief GET_DESCRIPTOR helper. @return bytes read, or negative on error. */
int usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t index,
                       void *buf, int len);

/** @brief SET_ADDRESS helper. */
int usb_set_address(usb_device_t *dev, uint8_t addr);

/** @brief SET_CONFIGURATION helper. */
int usb_set_configuration(usb_device_t *dev, uint8_t cfg);

/**
 * @brief Probe the root ports, enumerate any attached device and offer its
 *        interfaces to the class drivers. Called once from the USB thread.
 */
void usb_init(void);

/** @brief Poll every claimed interrupt endpoint once (called from the thread). */
void usb_poll(void);

/** @brief Kernel-thread entry: @ref usb_init then a @ref usb_poll loop. */
void usb_thread(void);
