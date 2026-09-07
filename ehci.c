/**
 * @file ehci.c
 * @brief Polled EHCI (USB 2.0) driver for USB HID boot keyboards / mice.
 *
 * Scope, on purpose: BIOS→OS handoff, controller reset, the async schedule for
 * control transfers, the periodic schedule for interrupt-IN, one or two levels
 * of hub enumeration (the Intel 6-series PCH sits a Rate-Matching Hub between
 * the EHCI root ports and the physical connectors, so a full-speed keyboard is
 * reached through a transaction translator), the HID boot protocol, and hot
 * plug. No MSI, no isochronous, no mass storage.
 *
 * All schedule structures and transfer buffers are 32-byte-aligned statics in
 * .bss, which is identity-mapped, so &x is the physical address the controller
 * needs. The register block (BAR0) is mapped 1:1 cache-disabled and shared into
 * every address space, matching ahci.c / xhci.c.
 */
#include <ehci.h>

#include <pci.h>
#include <usb.h>
#include <usb_hid.h>
#include <keyboard.h>

#include <mm.h>
#include <paging.h>
#include <pit.h>
#include <cmdline.h>
#include <lib/string.h>
#include <log.h>

/* ------------------------------------------------------------------ *
 *  Capability / operational registers                                 *
 * ------------------------------------------------------------------ */
#define CAP_CAPLENGTH   0x00   /* u8  */
#define CAP_HCSPARAMS   0x04
#define CAP_HCCPARAMS   0x08

#define OP_USBCMD       0x00
#define OP_USBSTS       0x04
#define OP_USBINTR      0x08
#define OP_FRINDEX      0x0C
#define OP_CTRLDSSEG    0x10
#define OP_PERIODICLB   0x14
#define OP_ASYNCLB      0x18
#define OP_CONFIGFLAG   0x40
#define OP_PORTSC(n)    (0x44 + (n) * 4)

#define CMD_RS          (1u << 0)
#define CMD_HCRESET     (1u << 1)
#define CMD_PSE         (1u << 4)   /* periodic schedule enable */
#define CMD_ASE         (1u << 5)   /* async schedule enable    */
#define CMD_ITC_8       (8u << 16)  /* interrupt threshold: 8 micro-frames */

#define STS_HCHALTED    (1u << 12)

#define PORT_CCS        (1u << 0)
#define PORT_CSC        (1u << 1)   /* RW1C */
#define PORT_PED        (1u << 2)
#define PORT_PEC        (1u << 3)   /* RW1C */
#define PORT_OCC        (1u << 5)   /* RW1C */
#define PORT_RESET      (1u << 8)
#define PORT_LINESTS(v) (((v) >> 10) & 3)   /* 1 = K/low-speed device present */
#define PORT_PP         (1u << 12)
#define PORT_OWNER      (1u << 13)
#define PORT_RW1C       (PORT_CSC | PORT_PEC | PORT_OCC)

/* ------------------------------------------------------------------ *
 *  Hardware descriptor formats (32-bit addressing)                    *
 * ------------------------------------------------------------------ */
typedef struct __attribute__((aligned(32))) {
    volatile uint32_t next;
    volatile uint32_t alt_next;
    volatile uint32_t token;
    volatile uint32_t buf[5];
} ehci_qtd_t;

typedef struct __attribute__((aligned(32))) {
    volatile uint32_t horiz;      /* horizontal link + Typ + T          */
    volatile uint32_t epchar;     /* dword 1: addr/ep/eps/dtc/H/maxpkt   */
    volatile uint32_t epcap;      /* dword 2: s-mask/c-mask/hub/port/mult*/
    volatile uint32_t cur_qtd;
    /* transfer overlay */
    volatile uint32_t ov_next;
    volatile uint32_t ov_alt;
    volatile uint32_t ov_token;
    volatile uint32_t ov_buf[5];
} ehci_qh_t;

#define PTR_TERM        1u
#define PTR_QH          2u          /* Typ = 01b (queue head) */

#define QTD_PING        (1u << 0)
#define QTD_SPLIT       (1u << 1)
#define QTD_MISSED      (1u << 2)
#define QTD_XACTERR     (1u << 3)
#define QTD_BABBLE      (1u << 4)
#define QTD_BUFERR      (1u << 5)
#define QTD_HALTED      (1u << 6)
#define QTD_ACTIVE      (1u << 7)
#define QTD_ERRMASK     (QTD_HALTED | QTD_XACTERR | QTD_BABBLE | QTD_BUFERR)

#define QTD_PID_OUT     (0u << 8)
#define QTD_PID_IN      (1u << 8)
#define QTD_PID_SETUP   (2u << 8)
#define QTD_CERR3       (3u << 10)
#define QTD_IOC         (1u << 15)
#define QTD_BYTES(n)    ((uint32_t)((n) & 0x7FFF) << 16)
#define QTD_TOGGLE      (1u << 31)
#define QTD_GET_BYTES(t) (((t) >> 16) & 0x7FFF)

#define EPS_FULL        0u
#define EPS_LOW         1u
#define EPS_HIGH        2u

/* ------------------------------------------------------------------ *
 *  Static schedule / buffers (identity-mapped .bss, phys == virt)     *
 * ------------------------------------------------------------------ */
#define MAX_DEV   8    /* USB addresses 1..7                     */
#define MAX_INT   4    /* concurrently polled interrupt endpoints */

/* 1024-entry periodic frame list. Allocated from a pmm frame (4 KiB, page
 * aligned) and identity-mapped rather than a static aligned .bss array — the
 * kernel image already sits right against the 4 MiB identity-map line on a real
 * laptop (see the T470s "black screen" fix), so keep 4 KiB out of .bss. */
static volatile uint32_t *periodic_list;

static ehci_qh_t  async_qh;                 /* H-bit head of the async ring   */
static ehci_qh_t  ctrl_qh;                  /* control-transfer work QH       */
static ehci_qtd_t ctrl_qtd[3];              /* SETUP / DATA / STATUS          */
static uint8_t    setup_buf[8]   __attribute__((aligned(32)));
static uint8_t    data_buf[512]  __attribute__((aligned(32)));

static ehci_qh_t  int_qh[MAX_INT];
static ehci_qtd_t int_qtd[MAX_INT];
static uint8_t    int_buf[MAX_INT][16] __attribute__((aligned(32)));

/* ------------------------------------------------------------------ *
 *  Controller / device state                                          *
 * ------------------------------------------------------------------ */
static volatile uint8_t *mmio;
static volatile uint8_t *op;
static struct pci_device *ehci_pci;
static int      have_hc;
static int      nports;
static uint8_t  next_addr = 1;

/** One enumerated device (enough of it to build a QH). */
typedef struct {
    int      in_use;
    uint8_t  addr;
    uint8_t  speed;        /* EPS_FULL / EPS_LOW / EPS_HIGH */
    uint16_t max_packet0;
    uint8_t  tt_hub;       /* transaction-translator hub address (0 = direct HS) */
    uint8_t  tt_port;      /* TT downstream port number                          */
} edev_t;
static edev_t edev[MAX_DEV];

/** Per-interrupt-endpoint software state. */
static struct {
    int      used;
    uint8_t  ep;
    uint8_t  proto;        /* 1 = keyboard, 2 = mouse */
    int      maxlen;
    int      toggle;
    uint8_t  prev[8];      /* previous keyboard report (edge detection) */
} islot[MAX_INT];

/* ------------------------------------------------------------------ *
 *  Register helpers                                                   *
 * ------------------------------------------------------------------ */
static uint32_t cr(uint32_t o)            { return *(volatile uint32_t *)(mmio + o); }
static uint32_t opr(uint32_t o)           { return *(volatile uint32_t *)(op + o); }
static void     opw(uint32_t o, uint32_t v){ *(volatile uint32_t *)(op + o) = v; }

static void msleep(uint32_t ms) { pit_busywait_ms(ms); }

/* ------------------------------------------------------------------ *
 *  BIOS -> OS handoff (EHCI extended capability USBLEGSUP)            *
 * ------------------------------------------------------------------ */
static void bios_handoff(pci_device_t *pd, uint32_t hccparams) {
    uint8_t eecp = (hccparams >> 8) & 0xFF;
    int guard = 0;
    while (eecp >= 0x40 && guard++ < 16) {
        uint32_t cap = pci_cfg_read32(pd, eecp);
        if ((cap & 0xFF) == 0x01) {              /* USBLEGSUP */
            if (cap & (1u << 16)) {              /* HC BIOS Owned */
                pci_cfg_write32(pd, eecp, cap | (1u << 24) /* HC OS Owned */);
                for (int i = 0; i < 200; i++) {
                    if (!(pci_cfg_read32(pd, eecp) & (1u << 16)))
                        break;
                    msleep(5);
                }
            }
            /* Kill every SMI-enable source in USBLEGCTLSTS so the firmware
             * stops trapping our register writes. */
            pci_cfg_write32(pd, eecp + 4, 0);
            klogf(LOG_INFO, "ehci: BIOS handoff via EECP 0x%x\n", eecp);
            return;
        }
        eecp = (cap >> 8) & 0xFF;
    }
}

/* ------------------------------------------------------------------ *
 *  Async schedule: one control transfer at a time on ctrl_qh          *
 * ------------------------------------------------------------------ */
static void qtd_fill(ehci_qtd_t *t, uint32_t pid, int toggle, uint32_t addr,
                     int len, int ioc, int last) {
    memset((void *)t, 0, sizeof(*t));
    t->next     = last ? PTR_TERM : (uint32_t)(t + 1);
    t->alt_next = PTR_TERM;
    t->token    = QTD_ACTIVE | QTD_CERR3 | pid | QTD_BYTES(len) |
                  (toggle ? QTD_TOGGLE : 0) | (ioc ? QTD_IOC : 0);
    if (len) {
        t->buf[0] = addr;
        t->buf[1] = (addr + 0x1000) & ~0xFFFu;
    }
}

/** @brief Run a control transfer on @p d. @return bytes transferred, or -1. */
static int ehci_control(edev_t *d, const usb_setup_t *s, void *data, int len, int in) {
    if (len > (int)sizeof(data_buf))
        return -1;

    memcpy(setup_buf, (void *)s, 8);
    if (!in && data && len > 0)
        memcpy(data_buf, data, len);

    int n = 0;
    qtd_fill(&ctrl_qtd[n++], QTD_PID_SETUP, 0, (uint32_t)setup_buf, 8, 0, 0);
    if (len > 0)
        qtd_fill(&ctrl_qtd[n++], in ? QTD_PID_IN : QTD_PID_OUT, 1,
                 (uint32_t)data_buf, len, 0, 0);
    qtd_fill(&ctrl_qtd[n], in ? QTD_PID_OUT : QTD_PID_IN, 1, 0, 0, 1, 1);
    n++;

    uint32_t c = (d->speed != EPS_HIGH) ? (1u << 27) : 0;   /* control-ep flag */
    ctrl_qh.epchar = (d->addr & 0x7F) | (0u << 8) | ((uint32_t)d->speed << 12) |
                     (1u << 14 /* DTC */) | ((uint32_t)d->max_packet0 << 16) | c;
    ctrl_qh.epcap  = (1u << 30) /* mult = 1 */ |
                     ((uint32_t)d->tt_hub << 16) | ((uint32_t)d->tt_port << 23);
    ctrl_qh.cur_qtd = PTR_TERM;
    ctrl_qh.ov_alt  = PTR_TERM;
    ctrl_qh.ov_token = 0;                    /* inactive: reload from ov_next */
    for (int i = 0; i < 5; i++) ctrl_qh.ov_buf[i] = 0;
    ctrl_qh.ov_next = (uint32_t)&ctrl_qtd[0];

    int ok = 0;
    for (int spin = 0; spin < 500000; spin++) {
        uint32_t tok = ctrl_qtd[n - 1].token;
        if (!(tok & QTD_ACTIVE)) { ok = 1; break; }
        if (tok & QTD_HALTED) break;
        __builtin_ia32_pause();
    }
    ctrl_qh.ov_next = PTR_TERM;
    ctrl_qh.ov_token = QTD_HALTED;

    if (!ok)
        return -1;
    for (int i = 0; i < n; i++)
        if (ctrl_qtd[i].token & QTD_ERRMASK)
            return -1;

    int got = len;
    if (len > 0)
        got = len - (int)QTD_GET_BYTES(ctrl_qtd[1].token);
    if (in && data && got > 0)
        memcpy(data, data_buf, got > len ? len : got);
    return got;
}

/* --- standard-request helpers ------------------------------------- */
static int get_desc(edev_t *d, uint8_t type, uint8_t idx, void *buf, int len) {
    usb_setup_t s = {
        .bmRequestType = USB_DIR_IN,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (uint16_t)((type << 8) | idx),
        .wIndex = 0, .wLength = (uint16_t)len,
    };
    return ehci_control(d, &s, buf, len, 1);
}

static int set_address(edev_t *d, uint8_t addr) {
    usb_setup_t s = { .bmRequestType = USB_DIR_OUT, .bRequest = USB_REQ_SET_ADDRESS,
                      .wValue = addr, .wIndex = 0, .wLength = 0 };
    int r = ehci_control(d, &s, 0, 0, 0);
    if (r >= 0) d->addr = addr;
    return r;
}

static int set_config(edev_t *d, uint8_t cfg) {
    usb_setup_t s = { .bmRequestType = USB_DIR_OUT, .bRequest = USB_REQ_SET_CONFIGURATION,
                      .wValue = cfg, .wIndex = 0, .wLength = 0 };
    return ehci_control(d, &s, 0, 0, 0);
}

/* ------------------------------------------------------------------ *
 *  Periodic schedule: one persistent qTD per claimed interrupt EP     *
 * ------------------------------------------------------------------ */
static void int_arm(int s) {
    ehci_qtd_t *t = &int_qtd[s];
    memset((void *)t, 0, sizeof(*t));
    t->next = t->alt_next = PTR_TERM;
    t->token = QTD_ACTIVE | QTD_CERR3 | QTD_PID_IN | QTD_IOC |
               QTD_BYTES(islot[s].maxlen) | (islot[s].toggle ? QTD_TOGGLE : 0);
    t->buf[0] = (uint32_t)int_buf[s];
    t->buf[1] = ((uint32_t)int_buf[s] + 0x1000) & ~0xFFFu;

    int_qh[s].ov_alt   = PTR_TERM;
    int_qh[s].ov_token = 0;
    for (int i = 0; i < 5; i++) int_qh[s].ov_buf[i] = 0;
    int_qh[s].ov_next  = (uint32_t)t;
}

static int int_claim(edev_t *d, uint8_t ep_addr, int maxlen, int proto) {
    int s = -1;
    for (int i = 0; i < MAX_INT; i++)
        if (!islot[i].used) { s = i; break; }
    if (s < 0)
        return -1;
    if (maxlen > (int)sizeof(int_buf[s]))
        maxlen = sizeof(int_buf[s]);

    islot[s].used   = 1;
    islot[s].ep     = ep_addr & 0x0F;
    islot[s].proto  = proto;
    islot[s].maxlen = maxlen;
    islot[s].toggle = 0;
    memset(islot[s].prev, 0, sizeof(islot[s].prev));

    /* Interrupt-IN QH. Micro-frame 0 for a HS device; for a FS/LS device behind
     * a TT the start-split goes in uframe 0 and the complete-splits are polled
     * across uframes 2..4. */
    uint32_t smask = 0x01;
    uint32_t cmask = (d->speed != EPS_HIGH) ? (0x1Cu << 8) : 0;
    int_qh[s].epchar = (d->addr & 0x7F) | ((uint32_t)islot[s].ep << 8) |
                       ((uint32_t)d->speed << 12) | (1u << 14 /* DTC */) |
                       ((uint32_t)maxlen << 16);
    int_qh[s].epcap  = smask | cmask | (1u << 30) |
                       ((uint32_t)d->tt_hub << 16) | ((uint32_t)d->tt_port << 23);
    int_qh[s].cur_qtd = PTR_TERM;
    int_arm(s);

    klogf(LOG_INFO, "ehci: HID %s on dev %u ep 0x%x (int slot %d)\n",
          proto == 1 ? "keyboard" : "mouse", d->addr, ep_addr, s);
    return s;
}

static void int_service(int s) {
    if (!islot[s].used)
        return;
    uint32_t tok = int_qtd[s].token;
    if (tok & QTD_ACTIVE)
        return;                       /* NAKs keep it active: no report yet */

    if (tok & QTD_ERRMASK) {
        islot[s].toggle = 0;
        int_arm(s);
        return;
    }

    int got = islot[s].maxlen - (int)QTD_GET_BYTES(tok);
    if (got > 0) {
        if (islot[s].proto == 1)
            usb_hid_report_keyboard(int_buf[s], got, islot[s].prev);
        else
            usb_hid_report_mouse(int_buf[s], got);
    }
    islot[s].toggle ^= 1;
    int_arm(s);
}

/* ------------------------------------------------------------------ *
 *  Enumeration                                                        *
 * ------------------------------------------------------------------ */
#define DT_HUB          0x29

static void hid_boot(edev_t *d, int iface) {
    usb_setup_t s;
    s = (usb_setup_t){ .bmRequestType = 0x21, .bRequest = HID_REQ_SET_PROTOCOL,
                       .wValue = HID_PROTO_BOOT, .wIndex = (uint16_t)iface, .wLength = 0 };
    ehci_control(d, &s, 0, 0, 0);
    s = (usb_setup_t){ .bmRequestType = 0x21, .bRequest = HID_REQ_SET_IDLE,
                       .wValue = 0, .wIndex = (uint16_t)iface, .wLength = 0 };
    ehci_control(d, &s, 0, 0, 0);
}

static void enum_device(int speed, uint8_t tt_hub, uint8_t tt_port,
                        int depth, const char *where);

/** @brief Walk a hub's downstream ports and enumerate anything attached. */
static void hub_enumerate(edev_t *hub, int depth) {
    uint8_t hd[16];
    usb_setup_t s = { .bmRequestType = 0xA0, .bRequest = USB_REQ_GET_DESCRIPTOR,
                      .wValue = (uint16_t)(DT_HUB << 8), .wIndex = 0, .wLength = 8 };
    if (ehci_control(hub, &s, hd, 8, 1) < 4) {
        klogf(LOG_WARNING, "ehci: hub %u descriptor read failed\n", hub->addr);
        return;
    }
    int nbr = hd[2];
    int pwr_ms = hd[5] * 2;
    if (pwr_ms < 100) pwr_ms = 100;
    klogf(LOG_INFO, "ehci: hub %u: %d ports\n", hub->addr, nbr);

    for (int p = 1; p <= nbr; p++) {
        s = (usb_setup_t){ .bmRequestType = 0x23, .bRequest = USB_REQ_SET_FEATURE,
                           .wValue = HUB_PORT_POWER, .wIndex = (uint16_t)p, .wLength = 0 };
        ehci_control(hub, &s, 0, 0, 0);
    }
    msleep(pwr_ms);

    for (int p = 1; p <= nbr; p++) {
        uint8_t st[4];
        s = (usb_setup_t){ .bmRequestType = 0xA3, .bRequest = USB_REQ_GET_STATUS,
                           .wValue = 0, .wIndex = (uint16_t)p, .wLength = 4 };
        if (ehci_control(hub, &s, st, 4, 1) < 4)
            continue;
        uint16_t status = st[0] | (st[1] << 8);
        if (!(status & HUB_PS_CONNECTION))
            continue;

        s = (usb_setup_t){ .bmRequestType = 0x23, .bRequest = USB_REQ_SET_FEATURE,
                           .wValue = HUB_PORT_RESET, .wIndex = (uint16_t)p, .wLength = 0 };
        ehci_control(hub, &s, 0, 0, 0);
        msleep(60);
        s = (usb_setup_t){ .bmRequestType = 0x23, .bRequest = USB_REQ_CLEAR_FEATURE,
                           .wValue = HUB_C_PORT_RESET, .wIndex = (uint16_t)p, .wLength = 0 };
        ehci_control(hub, &s, 0, 0, 0);
        msleep(15);

        s = (usb_setup_t){ .bmRequestType = 0xA3, .bRequest = USB_REQ_GET_STATUS,
                           .wValue = 0, .wIndex = (uint16_t)p, .wLength = 4 };
        if (ehci_control(hub, &s, st, 4, 1) < 4)
            continue;
        status = st[0] | (st[1] << 8);
        if (!(status & HUB_PS_ENABLE))
            continue;

        int cspeed = (status & HUB_PS_LOWSPEED) ? EPS_LOW
                   : (status & 0x0400 /* PORT_STAT_HIGH_SPEED */) ? EPS_HIGH
                   : EPS_FULL;

        uint8_t ct_hub, ct_port;
        if (cspeed == EPS_HIGH) {
            ct_hub = 0; ct_port = 0;
        } else if (hub->speed == EPS_HIGH) {
            ct_hub = hub->addr; ct_port = (uint8_t)p;
        } else {
            ct_hub = hub->tt_hub; ct_port = hub->tt_port;
        }
        enum_device(cspeed, ct_hub, ct_port, depth + 1, "hub port");
    }
}

/** @brief Bring up the single device now answering at address 0. */
static void enum_device(int speed, uint8_t tt_hub, uint8_t tt_port,
                        int depth, const char *where) {
    if (depth > 2) {
        klogf(LOG_WARNING, "ehci: hub nesting too deep, ignoring\n");
        return;
    }

    edev_t probe = { .in_use = 1, .addr = 0, .speed = (uint8_t)speed,
                     .max_packet0 = 8, .tt_hub = tt_hub, .tt_port = tt_port };

    uint8_t dd[18];
    memset(dd, 0, sizeof(dd));
    if (get_desc(&probe, USB_DT_DEVICE, 0, dd, 8) < 8) {
        klogf(LOG_ERR, "ehci: %s: no GET_DESCRIPTOR response\n", where);
        return;
    }
    probe.max_packet0 = dd[7] ? dd[7] : 8;

    int slot = -1;
    for (int i = 1; i < MAX_DEV; i++)
        if (!edev[i].in_use) { slot = i; break; }
    if (slot < 0 || next_addr >= MAX_DEV) {
        klogf(LOG_ERR, "ehci: device table full\n");
        return;
    }

    edev_t *d = &edev[slot];
    *d = probe;
    if (set_address(d, next_addr) < 0) {
        klogf(LOG_ERR, "ehci: %s: SET_ADDRESS failed\n", where);
        d->in_use = 0;
        return;
    }
    next_addr++;
    msleep(5);

    if (get_desc(d, USB_DT_DEVICE, 0, dd, 18) < 18) {
        klogf(LOG_ERR, "ehci: full device descriptor read failed\n");
        d->in_use = 0;
        return;
    }
    uint16_t vid = dd[8] | (dd[9] << 8);
    uint16_t pid = dd[10] | (dd[11] << 8);
    uint8_t  dclass = dd[4];

    uint8_t cfg[256];
    if (get_desc(d, USB_DT_CONFIG, 0, cfg, 9) < 9) {
        d->in_use = 0;
        return;
    }
    int total = cfg[2] | (cfg[3] << 8);
    if (total > (int)sizeof(cfg)) total = sizeof(cfg);
    if (get_desc(d, USB_DT_CONFIG, 0, cfg, total) < total) {
        d->in_use = 0;
        return;
    }
    if (set_config(d, cfg[5]) < 0) {
        klogf(LOG_ERR, "ehci: SET_CONFIGURATION failed\n");
        d->in_use = 0;
        return;
    }

    klogf(LOG_INFO, "ehci: dev %u = %04x:%04x class %u (%s)\n",
          d->addr, vid, pid, dclass, where);

    if (dclass == USB_CLASS_HUB) {
        hub_enumerate(d, depth);
        return;
    }

    /* Find a HID boot keyboard/mouse interface and its interrupt-IN endpoint. */
    int off = 0, iface = -1, proto = 0;
    while (off + 2 <= total) {
        int blen = cfg[off], btype = cfg[off + 1];
        if (blen < 2 || off + blen > total)
            break;
        if (btype == USB_DT_INTERFACE) {
            if (cfg[off + 5] == USB_CLASS_HID && cfg[off + 6] == 1 /* boot */) {
                iface = cfg[off + 2];
                proto = cfg[off + 7];
            } else {
                iface = -1;
            }
        } else if (btype == USB_DT_ENDPOINT && iface >= 0) {
            int attr = cfg[off + 3], eaddr = cfg[off + 2];
            if ((attr & 3) == 3 && (eaddr & 0x80)) {
                int emps = cfg[off + 4] | (cfg[off + 5] << 8);
                if (proto == 1 || proto == 2) {
                    hid_boot(d, iface);
                    int_claim(d, (uint8_t)eaddr, emps ? emps : 8, proto);
                }
                return;
            }
        }
        off += blen;
    }
}

/* ------------------------------------------------------------------ *
 *  Root ports                                                         *
 * ------------------------------------------------------------------ */
static void port_reset_enum(int p) {
    uint32_t v = opr(OP_PORTSC(p));
    if (!(v & PORT_CCS) || (v & PORT_OWNER))
        return;

    /* Low-speed device on a root port: hand it to a companion controller
     * (uhci.c) rather than driving split transactions from the root. */
    if (PORT_LINESTS(v) == 1) {
        opw(OP_PORTSC(p), (v & ~PORT_RW1C) | PORT_OWNER);
        return;
    }

    v = opr(OP_PORTSC(p)) & ~PORT_RW1C;
    v &= ~PORT_PED;
    opw(OP_PORTSC(p), v | PORT_RESET);
    msleep(50);
    v = opr(OP_PORTSC(p)) & ~PORT_RW1C;
    opw(OP_PORTSC(p), v & ~PORT_RESET);

    for (int i = 0; i < 20; i++) {
        if (!(opr(OP_PORTSC(p)) & PORT_RESET))
            break;
        msleep(1);
    }
    msleep(10);

    v = opr(OP_PORTSC(p));
    opw(OP_PORTSC(p), (v & ~PORT_RW1C) | (v & PORT_RW1C));   /* ack changes */

    if (!(v & PORT_PED)) {
        /* Enable never latched: a full-speed device on this root port. Release
         * it to a companion if one owns the pins. */
        opw(OP_PORTSC(p), (opr(OP_PORTSC(p)) & ~PORT_RW1C) | PORT_OWNER);
        klogf(LOG_INFO, "ehci: port %d: not high-speed, released\n", p + 1);
        return;
    }

    klogf(LOG_INFO, "ehci: port %d: high-speed device\n", p + 1);
    enum_device(EPS_HIGH, 0, 0, 0, "root port");
}

/* ------------------------------------------------------------------ *
 *  Bring-up                                                           *
 * ------------------------------------------------------------------ */
void ehci_probe(struct pci_device *d) {
    if (!ehci_pci)
        ehci_pci = d;
}

int ehci_present(void) { return have_hc; }

/** @brief First EHCI function on the bus (class 0C:03, prog-IF 0x20). */
static pci_device_t *find_ehci(void) {
    if (ehci_pci)
        return (pci_device_t *)ehci_pci;
    for (int i = 0; i < pci_count(); i++) {
        const pci_device_t *d = pci_dev(i);
        if (d && d->class == 0x0C && d->subclass == 0x03 && d->progif == 0x20)
            return (pci_device_t *)d;
    }
    return NULL;
}

int ehci_init(void) {
    if (cmdline_has("noehci") || cmdline_has("nousb")) {
        klogf(LOG_INFO, "ehci: disabled on the command line\n");
        return 0;
    }
    /* Opt-in for now. Taking the controller from the firmware (the USBLEGSUP
     * handoff below) can, on a real ThinkPad, knock the BIOS "USB legacy" SMM
     * out from under the 8042 and kill the already-working PS/2 keyboard. The
     * X220's internal keyboard / TrackPoint are PS/2, so external USB HID is a
     * bonus, not load-bearing -- enable it with `ehci` on the boot line. */
    if (!cmdline_has("ehci")) {
        klogf(LOG_INFO, "ehci: not enabled (add `ehci` to the boot line for "
                        "external USB keyboards / mice)\n");
        return 0;
    }

    pci_device_t *pd = find_ehci();
    if (!pd) {
        klogf(LOG_INFO, "ehci: no controller\n");
        return 0;
    }

    uint32_t span = pd->bar[0].size ? pd->bar[0].size : 0x1000;
    uint32_t bar  = pd->bar[0].addr;
    uint32_t bar_hi = pd->bar[0].is64 ? pci_cfg_read32(pd, 0x14) : 0;

    /* A UEFI firmware can park a 64-bit BAR above 4 GiB; re-home it into the low
     * PCI hole, exactly as xhci.c / nvme.c do. */
    if (bar_hi != 0 || bar == 0) {
        uint32_t nb = pci_mmio_hole(span);
        if (!nb) { klogf(LOG_WARNING, "ehci: no 32-bit MMIO hole\n"); return 0; }
        pci_cfg_write32(pd, 0x10, nb);
        if (pd->bar[0].is64)
            pci_cfg_write32(pd, 0x14, 0);
        pd->bar[0].addr = nb;
        pd->bar[0].is64 = 0;
        bar = nb;
        klogf(LOG_INFO, "ehci: BAR re-homed to 0x%x\n", nb);
    }
    if (!bar) { klogf(LOG_WARNING, "ehci: no MMIO BAR\n"); return 0; }

    pci_enable(pd, PCI_CMD_MEM | PCI_CMD_MASTER);

    for (uint32_t o = 0; o < span; o += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), bar + o, bar + o,
                     PAGE_PRESENT | PAGE_RW | PAGE_PCD | PAGE_PWT);
    vmm_share_kernel_range(bar, span);
    mmio = (volatile uint8_t *)bar;

    uint8_t caplen = mmio[CAP_CAPLENGTH];
    op = mmio + caplen;
    uint32_t hcs = cr(CAP_HCSPARAMS);
    uint32_t hcc = cr(CAP_HCCPARAMS);
    nports = hcs & 0x0F;
    if (nports < 1 || nports > 15) nports = 1;

    bios_handoff(pd, hcc);
    /* The handoff can knock the firmware's PS/2-keyboard SMM out from under the
     * 8042; put translation + IRQ 1 back. */
    keyboard_reinit();

    /* Stop, then reset. */
    opw(OP_USBCMD, opr(OP_USBCMD) & ~CMD_RS);
    for (int i = 0; i < 100 && !(opr(OP_USBSTS) & STS_HCHALTED); i++)
        msleep(1);
    opw(OP_USBCMD, CMD_HCRESET);
    for (int i = 0; i < 100 && (opr(OP_USBCMD) & CMD_HCRESET); i++)
        msleep(1);
    if (opr(OP_USBCMD) & CMD_HCRESET) {
        klogf(LOG_WARNING, "ehci: controller reset timed out\n");
        return 0;
    }

    opw(OP_CTRLDSSEG, 0);
    opw(OP_USBINTR, 0);

    /* Async ring: async_qh (H-bit, does nothing) -> ctrl_qh -> async_qh. */
    memset((void *)&async_qh, 0, sizeof(async_qh));
    memset((void *)&ctrl_qh, 0, sizeof(ctrl_qh));
    async_qh.horiz    = (uint32_t)&ctrl_qh | PTR_QH;
    async_qh.epchar   = (1u << 15) /* H */;
    async_qh.epcap    = (1u << 30);
    async_qh.cur_qtd  = PTR_TERM;
    async_qh.ov_next  = PTR_TERM;
    async_qh.ov_alt   = PTR_TERM;
    async_qh.ov_token = QTD_HALTED;
    ctrl_qh.horiz     = (uint32_t)&async_qh | PTR_QH;
    ctrl_qh.epcap     = (1u << 30);
    ctrl_qh.cur_qtd   = PTR_TERM;
    ctrl_qh.ov_next   = PTR_TERM;
    ctrl_qh.ov_alt    = PTR_TERM;
    ctrl_qh.ov_token  = QTD_HALTED;
    opw(OP_ASYNCLB, (uint32_t)&async_qh);

    /* Periodic list: every frame points at the chain of interrupt QHs. An
     * unused QH has S-mask 0, so the controller never schedules it. */
    memset((void *)int_qh, 0, sizeof(int_qh));
    memset(islot, 0, sizeof(islot));
    for (int i = 0; i < MAX_INT; i++) {
        int_qh[i].horiz    = (i + 1 < MAX_INT)
                             ? ((uint32_t)&int_qh[i + 1] | PTR_QH) : PTR_TERM;
        int_qh[i].epcap    = (1u << 30);
        int_qh[i].cur_qtd  = PTR_TERM;
        int_qh[i].ov_next  = PTR_TERM;
        int_qh[i].ov_alt   = PTR_TERM;
        int_qh[i].ov_token = QTD_HALTED;
    }
    if (!periodic_list) {
        void *pl = pmm_malloc();
        if (!pl) { klogf(LOG_WARNING, "ehci: no frame for the periodic list\n"); return 0; }
        vmm_map_phys(get_kern_directory(), (uint32_t)pl, (uint32_t)pl,
                     PAGE_PRESENT | PAGE_RW);
        periodic_list = (volatile uint32_t *)pl;
    }
    for (int i = 0; i < 1024; i++)
        periodic_list[i] = (uint32_t)&int_qh[0] | PTR_QH;
    opw(OP_PERIODICLB, (uint32_t)periodic_list);

    /* Run with both schedules enabled. */
    opw(OP_USBCMD, CMD_RS | CMD_ASE | CMD_PSE | CMD_ITC_8);
    for (int i = 0; i < 100 && (opr(OP_USBSTS) & STS_HCHALTED); i++)
        msleep(1);
    opw(OP_CONFIGFLAG, 1);        /* route all ports to the EHCI */
    msleep(5);

    have_hc = 1;
    klogf(LOG_INFO, "ehci: up  ports %d  caplen %u  hcc 0x%x\n", nports, caplen, hcc);

    for (int p = 0; p < nports; p++) {
        uint32_t v = opr(OP_PORTSC(p));
        if (!(v & PORT_PP)) {
            opw(OP_PORTSC(p), (v & ~PORT_RW1C) | PORT_PP);
            msleep(20);
            v = opr(OP_PORTSC(p));
        }
        if (v & PORT_CCS)
            port_reset_enum(p);
    }

    keyboard_reinit();   /* once more, after all the port resets */
    return 1;
}

/* ------------------------------------------------------------------ *
 *  Poll                                                               *
 * ------------------------------------------------------------------ */
void ehci_poll(void) {
    if (!have_hc)
        return;

    for (int s = 0; s < MAX_INT; s++)
        int_service(s);

    /* Cheap hot-plug: a port whose connect-status changed and that has nothing
     * enumerated behind it. */
    static int scan;
    if (++scan >= 20) {
        scan = 0;
        for (int p = 0; p < nports; p++) {
            uint32_t v = opr(OP_PORTSC(p));
            if (!(v & PORT_CSC))
                continue;
            opw(OP_PORTSC(p), (v & ~PORT_RW1C) | PORT_CSC);
            if (v & PORT_CCS)
                port_reset_enum(p);
        }
    }
}
