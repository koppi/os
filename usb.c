/**
 * @file usb.c
 * @brief USB core: standard control-request helpers and root-port enumeration.
 *
 * Enumeration is single-device-per-port and synchronous: reset the port, read
 * the device descriptor at address 0, assign an address, read the full
 * configuration, select it, then walk the interface/endpoint descriptors and
 * offer each HID interface to @ref usb_hid.c.
 */
#include <usb.h>
#include <uhci.h>
#include <xhci.h>
#include <usb_hid.h>
#include <usb_hub.h>
#include <log.h>
#include <lib/string.h>
#include <io.h>

#define MAX_DEVICES 8
#define MAX_HUB_DEPTH 3

static usb_device_t devices[MAX_DEVICES];

static usb_device_t *alloc_device(void);

int usb_control(usb_device_t *dev, const usb_setup_t *setup, void *data, int len) {
    return uhci_control(dev, setup, data, len);
}

int usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t index,
                       void *buf, int len) {
    usb_setup_t s = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest      = USB_REQ_GET_DESCRIPTOR,
        .wValue        = (uint16_t)((type << 8) | index),
        .wIndex        = 0,
        .wLength       = (uint16_t)len,
    };
    return usb_control(dev, &s, buf, len);
}

int usb_set_address(usb_device_t *dev, uint8_t addr) {
    usb_setup_t s = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest      = USB_REQ_SET_ADDRESS,
        .wValue        = addr,
        .wIndex        = 0,
        .wLength       = 0,
    };
    int r = usb_control(dev, &s, 0, 0);
    if(r >= 0)
        dev->address = addr;
    return r;
}

int usb_set_configuration(usb_device_t *dev, uint8_t cfg) {
    usb_setup_t s = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest      = USB_REQ_SET_CONFIGURATION,
        .wValue        = cfg,
        .wIndex        = 0,
        .wLength       = 0,
    };
    return usb_control(dev, &s, 0, 0);
}

/** @brief Walk a configuration blob and hand HID interfaces to the HID driver. */
static void parse_config(usb_device_t *dev, uint8_t *cfg, int total) {
    int off = 0;
    usb_iface_desc_t *iface = 0;
    while(off + 2 <= total) {
        uint8_t blen = cfg[off];
        uint8_t btype = cfg[off + 1];
        if(blen < 2 || off + blen > total)
            break;

        if(btype == USB_DT_INTERFACE) {
            iface = (usb_iface_desc_t *)(cfg + off);
        } else if(btype == USB_DT_ENDPOINT && iface) {
            usb_endpoint_desc_t *ep = (usb_endpoint_desc_t *)(cfg + off);
            int is_int = (ep->bmAttributes & 0x03) == 0x03;
            int is_in  = (ep->bEndpointAddress & 0x80) != 0;
            if(iface->bInterfaceClass == USB_CLASS_HID && is_int && is_in) {
                usb_hid_attach(dev, iface->bInterfaceNumber,
                               iface->bInterfaceProtocol,
                               ep->bEndpointAddress, ep->wMaxPacketSize);
            }
        }
        off += blen;
    }
}

/* Configuration blob staging. Enumeration is synchronous and single-threaded,
 * so one shared buffer is enough and it keeps recursion (hub behind hub) off
 * the kernel-thread stack. */
static uint8_t cfgbuf[256];

int usb_enumerate(usb_speed_t speed, int depth, const char *where) {
    klogf(LOG_INFO, "USB: %s: device attached (%s speed)\n",
          where, speed == USB_SPEED_LOW ? "low" : "full");

    /* Address 0, minimum EP0 packet size until we know better. */
    usb_device_t probe = { .address = 0, .speed = speed, .max_packet0 = 8 };

    usb_device_desc_t dd;
    memset(&dd, 0, sizeof(dd));
    if(usb_get_descriptor(&probe, USB_DT_DEVICE, 0, &dd, 8) < 8) {
        klogf(LOG_ERR, "USB: %s: no response to GET_DESCRIPTOR\n", where);
        return 0;
    }
    probe.max_packet0 = dd.bMaxPacketSize0 ? dd.bMaxPacketSize0 : 8;

    usb_device_t *dev = alloc_device();
    if(!dev) {
        klogf(LOG_ERR, "USB: device table full\n");
        return 0;
    }
    *dev = probe;
    dev->in_use = 1;

    /* Address == slot index + 1, so a released slot's address is reused. */
    uint8_t addr = (uint8_t)(dev - devices) + 1;
    if(usb_set_address(dev, addr) < 0) {
        klogf(LOG_ERR, "USB: SET_ADDRESS failed for %s\n", where);
        dev->in_use = 0;
        return 0;
    }

    if(usb_get_descriptor(dev, USB_DT_DEVICE, 0, &dd, sizeof(dd)) < (int)sizeof(dd)) {
        klogf(LOG_ERR, "USB: full device descriptor read failed\n");
        dev->in_use = 0;
        return 0;
    }
    dev->vendor  = dd.idVendor;
    dev->product = dd.idProduct;
    klogf(LOG_INFO, "USB: dev %u = %x:%x, class %u, %u config(s)\n",
          addr, dd.idVendor, dd.idProduct, dd.bDeviceClass, dd.bNumConfigurations);

    usb_config_desc_t cd;
    if(usb_get_descriptor(dev, USB_DT_CONFIG, 0, &cd, sizeof(cd)) < (int)sizeof(cd)) {
        klogf(LOG_ERR, "USB: config descriptor read failed\n");
        dev->in_use = 0;
        return 0;
    }
    int total = cd.wTotalLength;
    if(total > (int)sizeof(cfgbuf))
        total = sizeof(cfgbuf);
    if(usb_get_descriptor(dev, USB_DT_CONFIG, 0, cfgbuf, total) < total) {
        klogf(LOG_ERR, "USB: config blob read failed\n");
        dev->in_use = 0;
        return 0;
    }

    if(usb_set_configuration(dev, cd.bConfigurationValue) < 0) {
        klogf(LOG_ERR, "USB: SET_CONFIGURATION failed\n");
        dev->in_use = 0;
        return 0;
    }

    if(dd.bDeviceClass == USB_CLASS_HUB) {
        if(depth < MAX_HUB_DEPTH)
            usb_hub_init(dev, depth);
        else
            klogf(LOG_ERR, "USB: hub nesting too deep, ignoring\n");
        return addr;
    }

    parse_config(dev, cfgbuf, total);
    return addr;
}

void usb_release_device(uint8_t addr) {
    if(addr < 1 || addr > MAX_DEVICES)
        return;
    usb_device_t *dev = &devices[addr - 1];
    if(!dev->in_use)
        return;

    usb_hub_removed(addr);    /* if a hub, release everything behind it first */
    usb_hid_detach(addr);     /* drop any HID interfaces it owned */
    klogf(LOG_INFO, "USB: dev %u released\n", addr);
    memset(dev, 0, sizeof(*dev));
}

/** @brief Reset root @p port and, if a device is present, enumerate it. */
static void enumerate_root_port(int port) {
    usb_speed_t speed;
    if(!uhci_port_reset(port, &speed))
        return;

    char where[16] = "root port 0";
    where[10] = (char)('1' + port);
    usb_enumerate(speed, 0, where);
}

/* --- tiny device-table helpers (kept out of the header) --- */

/** @return A zeroed free device slot (not yet marked in use), or NULL. */
static usb_device_t *alloc_device(void) {
    for(int i = 0; i < MAX_DEVICES; i++) {
        if(!devices[i].in_use) {
            memset(&devices[i], 0, sizeof(devices[i]));
            return &devices[i];
        }
    }
    return 0;
}

void usb_init(void) {
    /* xHCI first: it is the only controller on a recent machine. */
    xhci_init();

    if(!uhci_init())
        return;
    for(int p = 0; p < uhci_port_count(); p++)
        enumerate_root_port(p);
}

void usb_poll(void) {
    xhci_poll();
    usb_hid_poll();
    usb_hub_poll();
}

void usb_thread(void) {
    usb_init();
    while(1) {
        usb_poll();
        sleep(2);
    }
}
