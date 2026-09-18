/**
 * @file usb_hub.c
 * @brief USB hub class driver — powers downstream ports and enumerates the
 *        devices behind them, including hot-plug at run time.
 *
 * Each enumerated hub is registered in @c hubs[]. @ref usb_hub_poll walks every
 * registered hub's downstream ports on a slow cadence and compares each port's
 * live connection status against the device we last enumerated there: a new
 * connection is reset and enumerated, a lost connection is torn down via
 * @ref usb_release_device. Detection is pure GET_STATUS polling — the hub's
 * status-change interrupt endpoint is left unused, in keeping with the rest of
 * this polled stack.
 */
#include <usb_hub.h>
#include <log.h>
#include <lib/string.h>
#include <io.h>

#define MAX_HUBS        4   /**< Concurrently tracked hubs (>= MAX_HUB_DEPTH). */
#define MAX_HUB_PORTS  15   /**< Downstream ports handled per hub. */

/** How many @ref usb_hub_poll calls to skip between port re-scans (~0.5 s). */
#define HUB_SCAN_INTERVAL 256

/** Sentinel @c port_addr value: a device is present but failed to enumerate. */
#define PORT_FAILED 0xFF

/** USB hub descriptor (bDescriptorType 0x29); trailing bitmaps ignored. */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t  bPwrOn2PwrGood;    /**< In 2 ms units. */
    uint8_t  bHubContrCurrent;
} usb_hub_desc_t;

/** A tracked hub and the address of whatever sits on each downstream port. */
typedef struct {
    usb_device_t *dev;                     /**< NULL = free registry slot. */
    int           depth;
    uint8_t       nports;
    uint8_t       port_addr[MAX_HUB_PORTS]; /**< 0 = empty, PORT_FAILED, else addr. */
} hub_t;

static hub_t hubs[MAX_HUBS];

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

/**
 * @brief GET_STATUS of downstream @p port.
 * @param hub    The hub device.
 * @param port   Downstream port number (1-based).
 * @param status Out: wPortStatus (HUB_PS_* bits).
 * @param change Out: wPortChange (HUB_PC_* bits).
 * @return 0 on success, -1 if the transfer failed (outputs untouched).
 */
static int hub_port_status(usb_device_t *hub, int port,
                           uint16_t *status, uint16_t *change) {
    uint8_t buf[4] = {0};
    usb_setup_t s = {
        .bmRequestType = REQ_IN_CLASS_OTHER,
        .bRequest      = USB_REQ_GET_STATUS,
        .wValue        = 0,
        .wIndex        = port,
        .wLength       = 4,
    };
    if(usb_control(hub, &s, buf, 4) < 4)
        return -1;
    *status = (uint16_t)(buf[0] | (buf[1] << 8));
    *change = (uint16_t)(buf[2] | (buf[3] << 8));
    return 0;
}

/** @brief Reset downstream @p port and enumerate the device now at address 0. */
static void hub_connect(hub_t *h, int p) {
    usb_device_t *hub = h->dev;

    sleep(100);   /* connect debounce */

    hub_set_port_feature(hub, p, HUB_PORT_RESET);
    sleep(60);
    hub_clear_port_feature(hub, p, HUB_C_PORT_RESET);
    sleep(15);

    uint16_t status = 0, change = 0;
    if(hub_port_status(hub, p, &status, &change) < 0 ||
       !(status & HUB_PS_ENABLE)) {
        klogf(LOG_ERR, "USB hub: dev %u port %d did not enable\n",
              hub->address, p);
        return;
    }

    usb_speed_t speed = (status & HUB_PS_LOWSPEED) ? USB_SPEED_LOW
                                                   : USB_SPEED_FULL;
    char where[24];
    snprintf(where, sizeof(where), "hub %u port %d", hub->address, p);

    int addr = usb_enumerate(speed, h->depth + 1, where);
    h->port_addr[p - 1] = (addr > 0) ? (uint8_t)addr : PORT_FAILED;
}

/** @brief Compare every downstream port against its last known state. */
static void hub_scan(hub_t *h) {
    for(int p = 1; p <= h->nports; p++) {
        uint16_t status = 0, change = 0;
        if(hub_port_status(h->dev, p, &status, &change) < 0)
            continue;   /* transient error — leave this port untouched */

        if(change & HUB_PC_CONNECTION)
            hub_clear_port_feature(h->dev, p, HUB_C_PORT_CONNECTION);

        int connected = (status & HUB_PS_CONNECTION) != 0;
        uint8_t known = h->port_addr[p - 1];

        /* Drop the recorded device if it lost its connection, or if the
         * connect state toggled since we last looked (fast unplug/replug). */
        if(known && (!connected || (change & HUB_PC_CONNECTION))) {
            if(known != PORT_FAILED) {
                klogf(LOG_INFO, "USB hub: dev %u port %d: device removed\n",
                      h->dev->address, p);
                usb_release_device(known);
            }
            h->port_addr[p - 1] = known = 0;
        }

        /* Enumerate whatever is now sitting on a port we consider empty. */
        if(connected && known == 0)
            hub_connect(h, p);
    }
}

void usb_hub_init(usb_device_t *hub, int depth) {
    usb_hub_desc_t hd;
    memset(&hd, 0, sizeof(hd));
    if(hub_get_descriptor(hub, &hd) < 7 || hd.bNbrPorts == 0) {
        klogf(LOG_ERR, "USB hub: bad descriptor\n");
        return;
    }

    hub_t *h = 0;
    for(int i = 0; i < MAX_HUBS; i++)
        if(!hubs[i].dev) { h = &hubs[i]; break; }
    if(!h) {
        klogf(LOG_ERR, "USB hub: registry full, dev %u not tracked\n",
              hub->address);
        return;
    }

    int nports = hd.bNbrPorts;
    if(nports > MAX_HUB_PORTS) {
        klogf(LOG_INFO, "USB hub: dev %u has %u ports, using first %d\n",
              hub->address, hd.bNbrPorts, MAX_HUB_PORTS);
        nports = MAX_HUB_PORTS;
    }

    memset(h, 0, sizeof(*h));
    h->dev    = hub;
    h->depth  = depth;
    h->nports = (uint8_t)nports;

    klogf(LOG_INFO, "USB hub: dev %u, %d downstream ports\n",
          hub->address, nports);

    /* Power every port, wait the hub's power-on-to-power-good time, then do
     * the first scan; from here on usb_hub_poll() picks up changes. */
    for(int p = 1; p <= nports; p++)
        hub_set_port_feature(hub, p, HUB_PORT_POWER);
    sleep(hd.bPwrOn2PwrGood * 2 + 60);

    hub_scan(h);
}

void usb_hub_poll(void) {
    static unsigned skip = 0;
    if(skip++ % HUB_SCAN_INTERVAL != 0)
        return;

    for(int i = 0; i < MAX_HUBS; i++)
        if(hubs[i].dev)
            hub_scan(&hubs[i]);
}

void usb_hub_removed(uint8_t addr) {
    for(int i = 0; i < MAX_HUBS; i++) {
        if(!hubs[i].dev || hubs[i].dev->address != addr)
            continue;

        for(int p = 0; p < MAX_HUB_PORTS; p++) {
            uint8_t child = hubs[i].port_addr[p];
            if(child && child != PORT_FAILED)
                usb_release_device(child);
            hubs[i].port_addr[p] = 0;
        }
        hubs[i].dev = 0;
        return;
    }
}
