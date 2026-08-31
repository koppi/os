/**
 * @file usb_hub.c
 * @brief USB hub class driver — powers downstream ports and enumerates the
 *        devices behind them.
 */
#include <usb_hub.h>
#include <log.h>
#include <lib/string.h>
#include <io.h>

/** USB hub descriptor (bDescriptorType 0x29); trailing bitmaps ignored. */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t  bPwrOn2PwrGood;    /**< In 2 ms units. */
    uint8_t  bHubContrCurrent;
} usb_hub_desc_t;

#define REQ_OUT_CLASS_OTHER  (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_OTHER)
#define REQ_IN_CLASS_OTHER   (USB_DIR_IN  | USB_TYPE_CLASS | USB_RECIP_OTHER)
#define REQ_IN_CLASS_DEVICE  (USB_DIR_IN  | USB_TYPE_CLASS | USB_RECIP_DEVICE)

/** @brief Read the hub descriptor into @p hd. @return bytes read. */
static int hub_get_descriptor(usb_device_t *hub, usb_hub_desc_t *hd) {
    usb_setup_t s = {
        .bmRequestType = REQ_IN_CLASS_DEVICE,
        .bRequest      = USB_REQ_GET_DESCRIPTOR,
        .wValue        = 0x29 << 8,
        .wIndex        = 0,
        .wLength       = sizeof(*hd),
    };
    return usb_control(hub, &s, hd, sizeof(*hd));
}

/** @brief SET_FEATURE(@p feature) on downstream @p port (1-based). */
static void hub_set_port_feature(usb_device_t *hub, int port, int feature) {
    usb_setup_t s = {
        .bmRequestType = REQ_OUT_CLASS_OTHER,
        .bRequest      = USB_REQ_SET_FEATURE,
        .wValue        = feature,
        .wIndex        = port,
        .wLength       = 0,
    };
    usb_control(hub, &s, 0, 0);
}

/** @brief CLEAR_FEATURE(@p feature) on downstream @p port (1-based). */
static void hub_clear_port_feature(usb_device_t *hub, int port, int feature) {
    usb_setup_t s = {
        .bmRequestType = REQ_OUT_CLASS_OTHER,
        .bRequest      = USB_REQ_CLEAR_FEATURE,
        .wValue        = feature,
        .wIndex        = port,
        .wLength       = 0,
    };
    usb_control(hub, &s, 0, 0);
}

/** @brief GET_STATUS of downstream @p port. @return the wPortStatus word. */
static uint16_t hub_get_port_status(usb_device_t *hub, int port) {
    uint8_t buf[4] = {0};
    usb_setup_t s = {
        .bmRequestType = REQ_IN_CLASS_OTHER,
        .bRequest      = USB_REQ_GET_STATUS,
        .wValue        = 0,
        .wIndex        = port,
        .wLength       = 4,
    };
    if(usb_control(hub, &s, buf, 4) < 2)
        return 0;
    return (uint16_t)(buf[0] | (buf[1] << 8));
}

void usb_hub_init(usb_device_t *hub, int depth) {
    usb_hub_desc_t hd;
    memset(&hd, 0, sizeof(hd));
    if(hub_get_descriptor(hub, &hd) < 7 || hd.bNbrPorts == 0) {
        klogf(LOG_ERR, "USB hub: bad descriptor\n");
        return;
    }
    klogf(LOG_INFO, "USB hub: dev %u, %u downstream ports\n",
          hub->address, hd.bNbrPorts);

    /* Power every port, then wait the hub's power-on-to-power-good time. */
    for(int p = 1; p <= hd.bNbrPorts; p++)
        hub_set_port_feature(hub, p, HUB_PORT_POWER);
    sleep(hd.bPwrOn2PwrGood * 2 + 60);

    for(int p = 1; p <= hd.bNbrPorts; p++) {
        uint16_t st = hub_get_port_status(hub, p);
        if(!(st & HUB_PS_CONNECTION))
            continue;

        hub_clear_port_feature(hub, p, HUB_C_PORT_CONNECTION);

        /* Reset the port; the device then answers at address 0. */
        hub_set_port_feature(hub, p, HUB_PORT_RESET);
        sleep(60);
        hub_clear_port_feature(hub, p, HUB_C_PORT_RESET);
        sleep(15);

        st = hub_get_port_status(hub, p);
        if(!(st & HUB_PS_ENABLE)) {
            klogf(LOG_ERR, "USB hub: port %u did not enable\n", p);
            continue;
        }

        usb_speed_t speed = (st & HUB_PS_LOWSPEED) ? USB_SPEED_LOW
                                                   : USB_SPEED_FULL;
        char where[24] = "hub d? port ?";
        where[5]  = (char)('0' + hub->address % 10);
        where[12] = (char)('0' + p % 10);
        usb_enumerate(speed, depth + 1, where);
    }
}
