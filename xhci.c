/**
 * @file xhci.c
 * @brief Polled xHCI driver for USB HID boot keyboards / mice.
 *
 * Scope on purpose: controller bring-up, root-port reset, device enumeration
 * (Enable Slot / Address Device / Configure Endpoint), synchronous control
 * transfers and interrupt-IN polling for the HID boot protocol. No MSI, no
 * streams, no mass storage. Structures live in identity-mapped .bss so their
 * address is their physical address; the register block is mapped 1:1
 * cache-disabled and shared into every address space (the USB thread is a
 * kernel thread, but keeping it uniform with AHCI costs nothing).
 */
#include <xhci.h>

#include <pci.h>
#include <usb.h>
#include <usb_hid.h>
#include <bcm5974.h>

#include <mm.h>
#include <paging.h>
#include <pit.h>
#include <cmdline.h>
#include <lib/string.h>
#include <log.h>

/* ------------------------------------------------------------------ *
 *  Register offsets                                                   *
 * ------------------------------------------------------------------ */
#define CAP_CAPLENGTH   0x00
#define CAP_HCSPARAMS1  0x04
#define CAP_HCSPARAMS2  0x08
#define CAP_HCCPARAMS1  0x10
#define CAP_DBOFF       0x14
#define CAP_RTSOFF      0x18

#define OP_USBCMD       0x00
#define OP_USBSTS       0x04
#define OP_PAGESIZE     0x08
#define OP_CRCR         0x18
#define OP_DCBAAP       0x30
#define OP_CONFIG       0x38
#define OP_PORTSC(p)    (0x400 + (p) * 0x10)

#define USBCMD_RS       (1u << 0)
#define USBCMD_HCRST    (1u << 1)
#define USBSTS_HCH      (1u << 0)
#define USBSTS_CNR      (1u << 11)

#define PORTSC_CCS      (1u << 0)
#define PORTSC_PED      (1u << 1)
#define PORTSC_PR       (1u << 4)
#define PORTSC_PP       (1u << 9)
#define PORTSC_CSC      (1u << 17)
#define PORTSC_PRC      (1u << 21)
#define PORTSC_SPEED(v) (((v) >> 10) & 0xF)
/* Writing PORTSC: preserve the RW bits, don't accidentally clear RW1C status. */
#define PORTSC_RW1C     (PORTSC_CSC | PORTSC_PRC | (1u<<18) | (1u<<20) | (1u<<22))

/* Interrupter 0 register set, relative to the runtime base `rt`. */
#define IR0_IMAN    (0x20 + 0x00)
#define IR0_ERSTSZ  (0x20 + 0x08)
#define IR0_ERSTBA  (0x20 + 0x10)
#define IR0_ERDP    (0x20 + 0x18)

static void rtw(uint32_t off, uint32_t v);   /* fwd */

/* TRB types */
#define TRB_NORMAL        1
#define TRB_SETUP         2
#define TRB_DATA          3
#define TRB_STATUS        4
#define TRB_LINK          6
#define TRB_ENABLE_SLOT   9
#define TRB_ADDRESS_DEV   11
#define TRB_CONFIG_EP     12
#define TRB_EVAL_CTX      13
#define TRB_XFER_EVENT    32
#define TRB_CMD_COMPLETE  33
#define TRB_PORT_STATUS   34

#define TRB_TYPE(t)   ((t) << 10)
#define TRB_GET_TYPE(c) (((c) >> 10) & 0x3F)
#define TRB_CYCLE     (1u << 0)
#define TRB_IOC       (1u << 5)
#define TRB_IDT       (1u << 6)
#define TRB_ISP       (1u << 2)
#define TRB_CHAIN     (1u << 4)

#define CC_SUCCESS    1

typedef struct { uint32_t d[4]; } trb_t;

#define RING_SZ 16   /* TRBs per ring (last is a Link back to entry 0) */

/* ------------------------------------------------------------------ *
 *  Static DMA structures (identity-mapped .bss)                       *
 * ------------------------------------------------------------------ */
#define MAX_SLOTS 4
#define MAX_INT_EP 2      /* HID interfaces per device: e.g. an Apple topcase */
                          /* reports a boot keyboard AND a BCM5974 trackpad.  */

static uint64_t dcbaa[64]      __attribute__((aligned(64)));
static uint64_t scratch_arr[64] __attribute__((aligned(64)));  /* scratchpad ptrs */
static trb_t    cmd_ring[RING_SZ] __attribute__((aligned(64)));
static trb_t    evt_ring[RING_SZ] __attribute__((aligned(64)));
static struct { uint64_t base; uint32_t size; uint32_t rsv; }
                erst[1]       __attribute__((aligned(64)));

/* Device context = 32 contexts, Input context = 1 control + 32 -> 33, at 64B. */
static uint8_t  dev_ctx  [MAX_SLOTS][32 * 64] __attribute__((aligned(64)));
static uint8_t  in_ctx   [MAX_SLOTS][33 * 64] __attribute__((aligned(64)));
static trb_t    ep0_ring [MAX_SLOTS][RING_SZ] __attribute__((aligned(64)));
static trb_t    int_ring [MAX_SLOTS][MAX_INT_EP][RING_SZ] __attribute__((aligned(64)));
static uint8_t  xfer_buf [MAX_SLOTS][256] __attribute__((aligned(64)));
/* One HID report staging buffer per interrupt endpoint. 512 bytes covers the
 * largest BCM5974 multi-touch package (type-3 header + 16 finger blocks). */
static uint8_t  hid_buf  [MAX_SLOTS][MAX_INT_EP][512] __attribute__((aligned(64)));

/* ------------------------------------------------------------------ *
 *  Controller / device state                                          *
 * ------------------------------------------------------------------ */
static volatile uint8_t *mmio;
static volatile uint8_t *op;
static volatile uint8_t *rt;
static volatile uint32_t *db;
static struct pci_device *xhci_pci;
static int have_hc;
static int ctx_size;     /* 32 or 64 */
static int max_ports;

static uint32_t cmd_enq, cmd_cycle = 1;
static uint32_t evt_deq, evt_cycle = 1;

#define MAX_IEP_PER_DEV 2  /* keyboard + trackpad */

typedef struct {
    int      in_use;
    int      slot_id;
    int      port;              /* 1-based root port */
    int      n_iep;             /* # of configured interrupt IN endpoints    */
    int      iep_dci[MAX_IEP_PER_DEV];   /* DCI of the interrupt endpoint         */
    int      iep_iface[MAX_IEP_PER_DEV]; /* owning interface number               */
    int      iep_proto[MAX_IEP_PER_DEV]; /* 1 = keyboard, 2 = mouse, 3 = BCM5974  */
    int      iep_len[MAX_IEP_PER_DEV];   /* bytes requested per transfer          */
    uint32_t ep0_enq, ep0_cycle;
    uint32_t iep_enq[MAX_IEP_PER_DEV], iep_cycle[MAX_IEP_PER_DEV];
    uint8_t  hid_prev[8];       /* previous keyboard report (edge detection) */
} xdev_t;
static xdev_t xdev[MAX_SLOTS];

static uint32_t rd(uint32_t off)          { return *(volatile uint32_t *)(mmio + off); }
static uint32_t opr(uint32_t off)         { return *(volatile uint32_t *)(op + off); }
static void     opw(uint32_t off, uint32_t v) { *(volatile uint32_t *)(op + off) = v; }
static void     rtw(uint32_t off, uint32_t v) { *(volatile uint32_t *)(rt + off) = v; }

/** @brief Advance ERDP to the current dequeue position and clear EHB. */
static void erdp_update(void) {
    rtw(IR0_ERDP,     (uint32_t)&evt_ring[evt_deq] | (1u << 3));
    rtw(IR0_ERDP + 4, 0);
}

/* Input context = Input Control Context + Slot Context + EP contexts.
 * The device/slot contexts start after the 32-or-64-byte control context. */
static uint32_t *in_ctrl(uint8_t *ctx)       { return (uint32_t *)ctx; }
static uint8_t  *in_dev(uint8_t *ctx)        { return ctx + ctx_size; }

/* ------------------------------------------------------------------ *
 *  Ring helpers                                                       *
 * ------------------------------------------------------------------ */
static void ring_init(trb_t *ring, uint32_t phys_link_target) {
    memset(ring, 0, sizeof(trb_t) * RING_SZ);
    ring[RING_SZ - 1].d[0] = phys_link_target;
    ring[RING_SZ - 1].d[1] = 0;
    ring[RING_SZ - 1].d[2] = 0;
    ring[RING_SZ - 1].d[3] = TRB_TYPE(TRB_LINK) | (1u << 1) /* Toggle Cycle */;
}

/** @brief Append @p t to @p ring at *@p enq (updating enq / cycle). */
static void ring_push(trb_t *ring, uint32_t *enq, uint32_t *cycle, trb_t t) {
    t.d[3] = (t.d[3] & ~TRB_CYCLE) | (*cycle ? TRB_CYCLE : 0);
    ring[*enq] = t;
    (*enq)++;
    if (*enq == RING_SZ - 1) {
        /* Set the Link TRB's cycle then wrap. */
        ring[RING_SZ - 1].d[3] = (ring[RING_SZ - 1].d[3] & ~TRB_CYCLE) |
                                 (*cycle ? TRB_CYCLE : 0);
        *enq = 0;
        *cycle ^= 1;
    }
}

/* ------------------------------------------------------------------ *
 *  Event ring: synchronous drain for a matching completion            *
 * ------------------------------------------------------------------ */
/**
 * @brief Spin the event ring until a TRB of @p want_type arrives (or timeout).
 * @param out  Filled with the event TRB (may be NULL).
 * @param match_ptr  If non-zero, also require d[0] (the TRB pointer) to match.
 * @return completion code, or -1 on timeout.
 */
static int evt_wait(int want_type, uint32_t match_ptr, trb_t *out) {
    for (int spin = 0; spin < 3000000; spin++) {
        trb_t *e = &evt_ring[evt_deq];
        if ((e->d[3] & TRB_CYCLE) != (evt_cycle ? TRB_CYCLE : 0)) {
            __builtin_ia32_pause();
            continue;
        }
        trb_t ev = *e;
        int type = TRB_GET_TYPE(ev.d[3]);

        /* advance dequeue */
        evt_deq++;
        if (evt_deq == RING_SZ) { evt_deq = 0; evt_cycle ^= 1; }
        erdp_update();

        if (type == want_type &&
            (match_ptr == 0 || ev.d[0] == match_ptr)) {
            if (out) *out = ev;
            return (ev.d[2] >> 24) & 0xFF;   /* completion code */
        }
        /* Not what we're waiting for: for HID polling we might want it later,
         * but during enumeration just drop other events. */
        if (type == TRB_PORT_STATUS)
            continue;
    }
    return -1;
}

/** @brief Post a command TRB, ring doorbell 0, wait for its completion event. */
static int cmd_exec(trb_t t, trb_t *result) {
    uint32_t slot_of_trb = (uint32_t)&cmd_ring[cmd_enq];
    ring_push(cmd_ring, &cmd_enq, &cmd_cycle, t);
    db[0] = 0;                                     /* command ring doorbell */
    trb_t ev;
    int cc = evt_wait(TRB_CMD_COMPLETE, slot_of_trb, &ev);
    if (result) *result = ev;
    return cc;
}

/* ------------------------------------------------------------------ *
 *  Control transfer on a device's EP0 ring                            *
 * ------------------------------------------------------------------ */
static int ctrl_xfer(int idx, const usb_setup_t *s, void *data, int len, int in) {
    xdev_t *d = &xdev[idx];
    if (data && len > 0 && !in)
        memcpy(xfer_buf[idx], (void *)data, len > 256 ? 256 : len);

    trb_t setup = {{ 0 }};
    memcpy(&setup.d[0], (void *)s, 8);
    setup.d[2] = 8;
    setup.d[3] = TRB_TYPE(TRB_SETUP) | TRB_IDT |
                 ((len > 0) ? (in ? (3u << 16) : (2u << 16)) : 0);
    ring_push(ep0_ring[idx], &d->ep0_enq, &d->ep0_cycle, setup);

    uint32_t data_ptr = 0;
    if (len > 0) {
        trb_t dt = {{ 0 }};
        dt.d[0] = (uint32_t)&xfer_buf[idx][0];
        dt.d[2] = (len > 256 ? 256 : len);
        dt.d[3] = TRB_TYPE(TRB_DATA) | (in ? (1u << 16) : 0) | TRB_ISP;
        data_ptr = (uint32_t)&ep0_ring[idx][d->ep0_enq];
        ring_push(ep0_ring[idx], &d->ep0_enq, &d->ep0_cycle, dt);
    }

    trb_t st = {{ 0 }};
    st.d[3] = TRB_TYPE(TRB_STATUS) | ((len > 0 && in) ? 0 : (1u << 16)) | TRB_IOC;
    uint32_t status_ptr = (uint32_t)&ep0_ring[idx][d->ep0_enq];
    ring_push(ep0_ring[idx], &d->ep0_enq, &d->ep0_cycle, st);

    db[d->slot_id] = 1;                       /* doorbell: slot, DCI 1 (EP0) */

    (void)data_ptr;
    trb_t ev;
    int cc = evt_wait(TRB_XFER_EVENT, status_ptr, &ev);
    if (cc < 0) cc = evt_wait(TRB_XFER_EVENT, 0, &ev);   /* accept short pkt */
    if (cc == CC_SUCCESS || cc == 13 /* short packet */) {
        if (data && len > 0 && in)
            memcpy(data, xfer_buf[idx], len > 256 ? 256 : len);
        return len;
    }
    return -1;
}

static int get_descriptor(int idx, uint8_t type, uint8_t index, void *buf, int len) {
    usb_setup_t s = {
        .bmRequestType = USB_DIR_IN,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (uint16_t)((type << 8) | index),
        .wIndex = 0, .wLength = (uint16_t)len,
    };
    return ctrl_xfer(idx, &s, buf, len, 1);
}

/* ------------------------------------------------------------------ *
 *  MacBook Air / Apple device helpers                                 *
 * ------------------------------------------------------------------ */
/* The internal keyboard + trackpad of a MacBook Air 6,x (2013) is a single
 * composite USB HID device ("Apple Internal Keyboard / Trackpad", widekeys
 * "wellspring 8", Linux bcm5974). Interface 0 is a boot keyboard; interface 1
 * is the BCM5974 multi-touch trackpad which needs its own endpoint and its own
 * report handler, and must NOT be switched to the HID boot protocol. */
#define APPLE_VENDOR       0x05ac
#define APPLE_TP_PID_ANSI  0x0290   /* MacBookAir6,x wellspring 8            */
#define APPLE_TP_PID_ISO   0x0291
#define APPLE_TP_PID_JIS   0x0292

static int is_apple_tp_pid(uint16_t pid) {
    return pid == APPLE_TP_PID_ANSI || pid == APPLE_TP_PID_ISO ||
           pid == APPLE_TP_PID_JIS;
}

/* xHCI PSI speed id -> USB max packet 0 for the control endpoint. */
static int ep0_mps_for_speed(int psi) {
    switch (psi) {
    case 1: return 8;     /* Full  */
    case 2: return 8;     /* Low   */
    case 3: return 64;    /* High  */
    case 4: return 512;   /* Super */
    default: return 64;
    }
}

static void reset_port(int port) {
    uint32_t v = opr(OP_PORTSC(port - 1));
    v &= ~PORTSC_RW1C;
    opw(OP_PORTSC(port - 1), v | PORTSC_PR);
    for (int i = 0; i < 200; i++) {
        pit_busywait_ms(1);
        if (opr(OP_PORTSC(port - 1)) & PORTSC_PRC)
            break;
    }
    /* ack PRC/CSC */
    v = opr(OP_PORTSC(port - 1));
    opw(OP_PORTSC(port - 1), (v & ~PORTSC_RW1C) | PORTSC_PRC | PORTSC_CSC);
    pit_busywait_ms(10);
}

/* ------------------------------------------------------------------ *
 *  Helper: configure one interrupt IN endpoint for a device slot      *
 * ------------------------------------------------------------------ */
static int configure_iep(int idx, int slot_id, int psi, int port,
                         int iface, int proto, int ep_addr, int ep_mps, int ep_ival) {
    xdev_t *d = &xdev[idx];
    if (d->n_iep >= MAX_IEP_PER_DEV)
        return -1;

    int dci = ((ep_addr & 0x0F) * 2) + 1;   /* IN endpoint */
    int iep_idx = d->n_iep;

    ring_init(int_ring[idx][iep_idx], (uint32_t)&int_ring[idx][iep_idx][0]);
    d->iep_enq[iep_idx] = 0; d->iep_cycle[iep_idx] = 1;

    memset(in_ctx[idx], 0, sizeof(in_ctx[idx]));
    in_ctrl(in_ctx[idx])[1] = 0x1 | (1u << dci);   /* A0 (slot) + A<dci> */
    uint32_t *sc = (uint32_t *)in_dev(in_ctx[idx]);
    sc[0] = (dci << 27) | (psi << 20);
    sc[1] = (port << 16);

    uint32_t *ie = (uint32_t *)(in_dev(in_ctx[idx]) + dci * ctx_size);
    int interval = ep_ival ? ep_ival : 8;
    if (interval > 15) interval = 15;
    ie[0] = (uint32_t)interval << 16;
    ie[1] = (7u << 3) | (3u << 1) | (ep_mps << 16);
    ie[2] = (uint32_t)&int_ring[idx][iep_idx][0] | 1;
    ie[3] = 0;
    ie[4] = ep_mps;

    trb_t ce = {{ (uint32_t)&in_ctx[idx][0], 0, 0,
                  TRB_TYPE(TRB_CONFIG_EP) | (slot_id << 24) }};
    trb_t r;
    if (cmd_exec(ce, &r) != CC_SUCCESS) {
        klogf(LOG_WARNING, "xhci: Configure Endpoint failed (iface %d)\n", iface);
        return -1;
    }

    /* For boot protocol devices (keyboard/mouse), set protocol + idle.
     * For BCM5974 trackpad (proto == 3), do NOT set boot protocol. */
    if (proto == 1 || proto == 2) {
        usb_setup_t s = (usb_setup_t){ .bmRequestType = 0x21, .bRequest = HID_REQ_SET_PROTOCOL,
                                       .wValue = HID_PROTO_BOOT, .wIndex = iface, .wLength = 0 };
        ctrl_xfer(idx, &s, 0, 0, 0);
        s = (usb_setup_t){ .bmRequestType = 0x21, .bRequest = HID_REQ_SET_IDLE,
                           .wValue = 0, .wIndex = iface, .wLength = 0 };
        ctrl_xfer(idx, &s, 0, 0, 0);
    }

    d->iep_dci[iep_idx] = dci;
    d->iep_iface[iep_idx] = iface;
    d->iep_proto[iep_idx] = proto;
    d->iep_len[iep_idx] = ep_mps;
    d->n_iep++;

    /* Arm the first interrupt-IN transfer */
    trb_t nt = {{ 0 }};
    nt.d[0] = (uint32_t)&hid_buf[idx][iep_idx][0];
    nt.d[2] = ep_mps;
    nt.d[3] = TRB_TYPE(TRB_NORMAL) | TRB_IOC | TRB_ISP;
    ring_push(int_ring[idx][iep_idx], &d->iep_enq[iep_idx], &d->iep_cycle[iep_idx], nt);

    klogf(LOG_INFO, "xhci: slot %d iface %d proto %d ep 0x%x configured\n",
          slot_id, iface, proto, ep_addr);
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Enumeration                                                        *
 * ------------------------------------------------------------------ */
static int enumerate_port(int port) {
    int psi = PORTSC_SPEED(opr(OP_PORTSC(port - 1)));

    /* 1. Enable Slot */
    trb_t r;
    trb_t es = {{ 0, 0, 0, TRB_TYPE(TRB_ENABLE_SLOT) }};
    if (cmd_exec(es, &r) != CC_SUCCESS)
        return -1;
    int slot_id = (r.d[3] >> 24) & 0xFF;
    if (slot_id < 1 || slot_id >= MAX_SLOTS)
        return -1;

    int idx = slot_id;
    xdev_t *d = &xdev[idx];
    memset(d, 0, sizeof(*d));
    d->in_use = 1;
    d->slot_id = slot_id;
    d->port = port;
    d->ep0_cycle = 1;

    memset(dev_ctx[idx], 0, sizeof(dev_ctx[idx]));
    dcbaa[slot_id] = (uint32_t)&dev_ctx[idx][0];

    /* 2. Input context: add slot + EP0 */
    memset(in_ctx[idx], 0, sizeof(in_ctx[idx]));
    in_ctrl(in_ctx[idx])[1] = 0x3;                 /* A0 (slot) | A1 (EP0) */

    uint32_t *sc = (uint32_t *)in_dev(in_ctx[idx]);
    sc[0] = (1u << 27) | (psi << 20);
    sc[1] = (port << 16);

    ring_init(ep0_ring[idx], (uint32_t)&ep0_ring[idx][0]);
    uint32_t *e0 = (uint32_t *)(in_dev(in_ctx[idx]) + 1 * ctx_size);
    int mps = ep0_mps_for_speed(psi);
    e0[0] = 0;
    e0[1] = (4u << 3) | (3u << 1) | (mps << 16);
    e0[2] = (uint32_t)&ep0_ring[idx][0] | 1;
    e0[3] = 0;
    e0[4] = 8;

    /* 3. Address Device */
    trb_t ad = {{ (uint32_t)&in_ctx[idx][0], 0, 0,
                  TRB_TYPE(TRB_ADDRESS_DEV) | (slot_id << 24) }};
    if (cmd_exec(ad, &r) != CC_SUCCESS) {
        klogf(LOG_WARNING, "xhci: Address Device failed on port %d\n", port);
        d->in_use = 0;
        return -1;
    }

    /* 4. Device descriptor */
    usb_device_desc_t dd;
    memset(&dd, 0, sizeof(dd));
    if (get_descriptor(idx, USB_DT_DEVICE, 0, &dd, 8) < 0 ||
        get_descriptor(idx, USB_DT_DEVICE, 0, &dd, 18) < 0) {
        klogf(LOG_WARNING, "xhci: no device descriptor on port %d\n", port);
        d->in_use = 0;
        return -1;
    }

    /* 5. Configuration descriptor */
    uint8_t cfg[256];
    if (get_descriptor(idx, USB_DT_CONFIG, 0, cfg, 9) < 0)
        { d->in_use = 0; return -1; }
    int total = cfg[2] | (cfg[3] << 8);
    if (total > (int)sizeof(cfg)) total = sizeof(cfg);
    if (get_descriptor(idx, USB_DT_CONFIG, 0, cfg, total) < 0)
        { d->in_use = 0; return -1; }

    /* 6. SET_CONFIGURATION */
    usb_setup_t s = { .bmRequestType = USB_DIR_OUT,
                      .bRequest = USB_REQ_SET_CONFIGURATION,
                      .wValue = cfg[5], .wIndex = 0, .wLength = 0 };
    ctrl_xfer(idx, &s, 0, 0, 0);

    int is_apple_trackpad = (dd.idVendor == APPLE_VENDOR && is_apple_tp_pid(dd.idProduct));

    klogf(LOG_INFO, "xhci: port %d dev %04x:%04x %s\n", port,
          dd.idVendor, dd.idProduct,
          is_apple_trackpad ? "(Apple Internal Keyboard/Trackpad)" : "");

    /* 7. Walk interfaces to find HID endpoints */
    int off = 0;
    int current_iface = -1;
    int current_proto = 0;
    int found_keyboard = 0;
    int found_trackpad = 0;

    while (off + 2 <= total) {
        int blen = cfg[off], btype = cfg[off + 1];
        if (blen < 2) break;

        if (btype == USB_DT_INTERFACE) {
            current_iface = cfg[off + 2];
            uint8_t iclass = cfg[off + 5];
            uint8_t isubclass = cfg[off + 6];
            uint8_t iproto = cfg[off + 7];

            if (iclass == USB_CLASS_HID) {
                if (isubclass == 1 && (iproto == 1 || iproto == 2)) {
                    /* Boot protocol keyboard or mouse */
                    current_proto = iproto;
                } else if (is_apple_trackpad && !found_trackpad) {
                    /* Apple BCM5974 trackpad: subclass 0, protocol 0, not boot */
                    current_proto = 3;  /* Custom proto for BCM5974 */
                } else {
                    current_proto = 0;
                }
            } else {
                current_proto = 0;
            }
        } else if (btype == USB_DT_ENDPOINT && current_iface >= 0 && current_proto > 0) {
            int attr = cfg[off + 3], addr = cfg[off + 2];
            if ((attr & 3) == 3 && (addr & 0x80)) {
                int ep_mps = cfg[off + 4] | (cfg[off + 5] << 8);
                int ep_ival = cfg[off + 6];

                if (current_proto == 1 && !found_keyboard) {
                    configure_iep(idx, slot_id, psi, port, current_iface, 1,
                                  addr, ep_mps, ep_ival);
                    found_keyboard = 1;
                } else if (current_proto == 2) {
                    configure_iep(idx, slot_id, psi, port, current_iface, 2,
                                  addr, ep_mps, ep_ival);
                } else if (current_proto == 3 && !found_trackpad) {
                    /* For BCM5974 trackpad (wellspring 8), the endpoint is 0x83
                     * regardless of what the descriptor says. The trackpad uses
                     * a custom binary format, not HID boot protocol. */
                    int trackpad_ep = 0x83;
                    int trackpad_mps = 64;  /* TYPE3 report size ~100 bytes, use 64 */
                    configure_iep(idx, slot_id, psi, port, current_iface, 3,
                                  trackpad_ep, trackpad_mps, 4);
                    /* Switch to wellspring mode (multi-touch) via control transfer */
                    usb_setup_t s = {
                        .bmRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                        .bRequest = 0x09,  /* BCM5974 mode switch request */
                        .wValue = 0x01,    /* Enable wellspring mode */
                        .wIndex = current_iface,
                        .wLength = 0,
                    };
                    ctrl_xfer(idx, &s, 0, 0, 0);
                    bcm5974_init();  /* Initialize parsing state */
                    found_trackpad = 1;
                }
            }
        }
        off += blen;
    }

    return idx;
}

/* ------------------------------------------------------------------ *
 *  Public bring-up                                                    *
 * ------------------------------------------------------------------ */
void xhci_probe(struct pci_device *d) {
    if (!xhci_pci)
        xhci_pci = d;
}

int xhci_present(void) { return have_hc; }

int xhci_init(void) {
    if (cmdline_has("noxhci") || cmdline_has("nousb")) {
        klogf(LOG_INFO, "xhci: disabled on the command line\n");
        return 0;
    }
    if (!xhci_pci) {
        const pci_device_t *d = pci_get_by_class(0x0C, 0x03);
        if (!d || d->progif != 0x30) {
            klogf(LOG_INFO, "xhci: no controller\n");
            return 0;
        }
        xhci_pci = (struct pci_device *) d;
    }
    pci_device_t *pd = (pci_device_t *) xhci_pci;

    uint32_t span = pd->bar[0].size ? pd->bar[0].size : 0x10000;
    uint32_t bar  = pd->bar[0].addr;
    uint32_t bar_hi = pd->bar[0].is64 ? pci_cfg_read32(pd, 0x14) : 0;

    /* A UEFI firmware can park a 64-bit BAR above 4 GiB, where this 32-bit
     * kernel cannot map it. Re-home it into a free slice of the PCI hole. */
    if (bar_hi != 0 || bar == 0) {
        uint32_t nb = pci_mmio_hole(span);
        if (!nb) { klogf(LOG_WARNING, "xhci: no 32-bit MMIO hole\n"); return 0; }
        pci_cfg_write32(pd, 0x10, nb);
        if (pd->bar[0].is64)
            pci_cfg_write32(pd, 0x14, 0);
        /* Reflect the move in the device table so a later re-homer (nvme) does
         * not pick the same slice out of pci_mmio_hole(). */
        pd->bar[0].addr = nb;
        pd->bar[0].is64 = 0;
        bar = nb;
        klogf(LOG_INFO, "xhci: BAR re-homed to 0x%x\n", nb);
    }
    if (!bar) { klogf(LOG_WARNING, "xhci: no MMIO BAR\n"); return 0; }

    pci_enable(pd, PCI_CMD_MEM | PCI_CMD_MASTER);

    for (uint32_t o = 0; o < span; o += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), bar + o, bar + o,
                     PAGE_PRESENT | PAGE_RW | PAGE_PCD | PAGE_PWT);
    vmm_share_kernel_range(bar, span);
    mmio = (volatile uint8_t *) bar;

    /* Intel: route the shared ports from the EHCI companion to xHCI. */
    if (pd->vendor == 0x8086) {
        uint32_t ports;
        ports = pci_cfg_read32(pd, 0xDC);           /* USB3PRM */
        pci_cfg_write32(pd, 0xD8, ports);           /* USB3_PSSEN */
        ports = pci_cfg_read32(pd, 0xD4);           /* XUSB2PRM */
        pci_cfg_write32(pd, 0xD0, ports);           /* XUSB2PR */
    }

    uint8_t caplen = mmio[CAP_CAPLENGTH];
    op = mmio + caplen;
    rt = mmio + (rd(CAP_RTSOFF) & ~0x1Fu);
    db = (volatile uint32_t *)(mmio + (rd(CAP_DBOFF) & ~0x3u));
    uint32_t hcs1 = rd(CAP_HCSPARAMS1);
    uint32_t hcc1 = rd(CAP_HCCPARAMS1);
    max_ports = (hcs1 >> 24) & 0xFF;
    ctx_size  = (hcc1 & (1u << 2)) ? 64 : 32;
    int max_slots = hcs1 & 0xFF;
    if (max_slots > MAX_SLOTS - 1) max_slots = MAX_SLOTS - 1;

    /* Wait for CNR, then reset. */
    for (int i = 0; i < 1000 && (opr(OP_USBSTS) & USBSTS_CNR); i++)
        pit_busywait_ms(1);
    opw(OP_USBCMD, opr(OP_USBCMD) & ~USBCMD_RS);
    for (int i = 0; i < 1000 && !(opr(OP_USBSTS) & USBSTS_HCH); i++)
        pit_busywait_ms(1);
    opw(OP_USBCMD, USBCMD_HCRST);
    for (int i = 0; i < 2000 && (opr(OP_USBCMD) & USBCMD_HCRST); i++)
        pit_busywait_ms(1);
    for (int i = 0; i < 2000 && (opr(OP_USBSTS) & USBSTS_CNR); i++)
        pit_busywait_ms(1);

    /* Scratchpad buffers: the controller's own save/restore area. Real Intel
     * xHCI needs a handful; QEMU needs none. The pages are pure DMA (the CPU
     * never touches them) so pmm frames are fine -- only the pointer array
     * has to be identity-mapped. */
    uint32_t hcs2 = rd(CAP_HCSPARAMS2);
    int nscratch = ((hcs2 >> 27) & 0x1F) | (((hcs2 >> 21) & 0x1F) << 5);
    memset(scratch_arr, 0, sizeof(scratch_arr));
    if (nscratch > 64) nscratch = 64;
    for (int i = 0; i < nscratch; i++) {
        void *pg = pmm_malloc();
        if (!pg) { nscratch = i; break; }
        /* pmm frames past the low 4 MiB are not in the kernel's identity map,
         * so the zeroing memset would #PF. Map it 1:1 first (the controller
         * only ever DMAs to it after this, by physical address). */
        vmm_map_phys(get_kern_directory(), (uint32_t) pg, (uint32_t) pg,
                     PAGE_PRESENT | PAGE_RW);
        memset(pg, 0, PAGE_SIZE);
        scratch_arr[i] = (uint32_t) pg;
    }

    /* DCBAA + slots */
    memset(dcbaa, 0, sizeof(dcbaa));
    if (nscratch > 0)
        dcbaa[0] = (uint32_t)&scratch_arr[0];
    opw(OP_CONFIG, max_slots);
    opw(OP_DCBAAP, (uint32_t)&dcbaa[0]);
    opw(OP_DCBAAP + 4, 0);

    /* Command ring */
    cmd_enq = 0; cmd_cycle = 1;
    ring_init(cmd_ring, (uint32_t)&cmd_ring[0]);
    opw(OP_CRCR, (uint32_t)&cmd_ring[0] | 1);   /* RCS = 1 */
    opw(OP_CRCR + 4, 0);

    /* Event ring (single ERST segment) */
    evt_deq = 0; evt_cycle = 1;
    memset(evt_ring, 0, sizeof(evt_ring));
    erst[0].base = (uint32_t)&evt_ring[0];
    erst[0].size = RING_SZ;
    erst[0].rsv  = 0;
    rtw(IR0_ERSTSZ, 1);
    rtw(IR0_ERSTBA, (uint32_t)&erst[0]);
    rtw(IR0_ERSTBA + 4, 0);
    erdp_update();
    rtw(IR0_IMAN, 0x2);   /* IE = 1 (we still poll; harmless) */

    /* Run */
    opw(OP_USBCMD, USBCMD_RS);
    for (int i = 0; i < 1000 && (opr(OP_USBSTS) & USBSTS_HCH); i++)
        pit_busywait_ms(1);

    have_hc = 1;
    klogf(LOG_INFO, "xhci: up  ports %d  slots %d  ctx %dB  op+0x%x\n",
          max_ports, max_slots, ctx_size, caplen);

    /* Power + enumerate every populated port. */
    for (int p = 1; p <= max_ports; p++) {
        uint32_t v = opr(OP_PORTSC(p - 1));
        if (!(v & PORTSC_PP)) {
            opw(OP_PORTSC(p - 1), (v & ~PORTSC_RW1C) | PORTSC_PP);
            pit_busywait_ms(20);
            v = opr(OP_PORTSC(p - 1));
        }
        if (!(v & PORTSC_CCS))
            continue;
        reset_port(p);
        if (opr(OP_PORTSC(p - 1)) & PORTSC_PED)
            enumerate_port(p);
    }
    return 1;
}

#include <bcm5974.h>

/* ------------------------------------------------------------------ *
 *  Poll                                                               *
 * ------------------------------------------------------------------ */
void xhci_poll(void) {
    if (!have_hc)
        return;

    /* Drain whatever is in the event ring. */
    for (int guard = 0; guard < 64; guard++) {
        trb_t *e = &evt_ring[evt_deq];
        if ((e->d[3] & TRB_CYCLE) != (evt_cycle ? TRB_CYCLE : 0))
            break;
        trb_t ev = *e;
        int type = TRB_GET_TYPE(ev.d[3]);

        evt_deq++;
        if (evt_deq == RING_SZ) { evt_deq = 0; evt_cycle ^= 1; }
        erdp_update();

        if (type == TRB_XFER_EVENT) {
            int slot = (ev.d[3] >> 24) & 0xFF;
            if (slot < 1 || slot >= MAX_SLOTS) continue;
            xdev_t *d = &xdev[slot];
            if (!d->in_use || d->n_iep == 0) continue;

            int cc = (ev.d[2] >> 24) & 0xFF;
            int residue = ev.d[2] & 0xFFFFFF;

            /* Find which interrupt endpoint this event belongs to.
             * The event TRB's d[0] points to the TRB that completed. */
            uint32_t trb_ptr = ev.d[0];
            int iep_idx = -1;
            for (int i = 0; i < d->n_iep; i++) {
                /* Check if the TRB pointer falls within this endpoint's ring */
                uint32_t ring_base = (uint32_t)&int_ring[slot][i][0];
                uint32_t ring_end = ring_base + sizeof(trb_t) * RING_SZ;
                if (trb_ptr >= ring_base && trb_ptr < ring_end) {
                    iep_idx = i;
                    break;
                }
            }
            if (iep_idx < 0)
                iep_idx = 0;  /* fallback */

            int n = d->iep_len[iep_idx] - residue;
            if ((cc == CC_SUCCESS || cc == 13) && n > 0) {
                uint8_t *buf = hid_buf[slot][iep_idx];
                int proto = d->iep_proto[iep_idx];

                if (proto == 1) {
                    usb_hid_report_keyboard(buf, n, d->hid_prev);
                } else if (proto == 2) {
                    usb_hid_report_mouse(buf, n);
                } else if (proto == 3) {
                    bcm5974_parse_report(buf, n);
                }
            }

            /* Re-arm this specific endpoint. */
            trb_t nt = {{ 0 }};
            nt.d[0] = (uint32_t)&hid_buf[slot][iep_idx][0];
            nt.d[2] = d->iep_len[iep_idx];
            nt.d[3] = TRB_TYPE(TRB_NORMAL) | TRB_IOC | TRB_ISP;
            ring_push(int_ring[slot][iep_idx], &d->iep_enq[iep_idx], &d->iep_cycle[iep_idx], nt);

            /* Ring doorbell for THIS endpoint so the controller processes the new TRB. */
            db[slot] = d->iep_dci[iep_idx];
        }
    }

    /* Cheap hot-plug: a newly-connected port with nothing enumerated. */
    static int scan;
    if (++scan >= 20) {
        scan = 0;
        for (int p = 1; p <= max_ports; p++) {
            uint32_t v = opr(OP_PORTSC(p - 1));
            if (!(v & PORTSC_CSC))
                continue;
            opw(OP_PORTSC(p - 1), (v & ~PORTSC_RW1C) | PORTSC_CSC);
            int already = 0;
            for (int i = 1; i < MAX_SLOTS; i++)
                if (xdev[i].in_use && xdev[i].port == p) already = 1;
            if ((v & PORTSC_CCS) && !already) {
                reset_port(p);
                if (opr(OP_PORTSC(p - 1)) & PORTSC_PED)
                    enumerate_port(p);
            }
        }
    }
}
