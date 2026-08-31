/**
 * @file uhci.c
 * @brief Intel UHCI (USB 1.1) host-controller driver.
 *
 * Schedule layout: every entry of the 1024-slot frame list points at the first
 * of @c NUM_INT_SLOTS interrupt queue heads, head-linked in turn to the control
 * queue head and a terminating entry. Control transfers run by pointing the
 * control QH's element link at a chain of transfer descriptors and polling
 * until it retires; each claimed interrupt endpoint gets one @c qh_int slot and
 * a single persistent TD that is re-armed after each poll.
 *
 * All descriptors and buffers are 16-byte-aligned statics in .bss, which is
 * identity-mapped, so &x can be handed to the controller as a physical address.
 */
#include <uhci.h>
#include <pci.h>
#include <io.h>
#include <log.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ *
 *  Hardware descriptor formats                                        *
 * ------------------------------------------------------------------ */

/** UHCI transfer descriptor (first 16 bytes are hardware-defined). */
typedef struct __attribute__((aligned(16))) {
    volatile uint32_t link;    /**< Link pointer + T/Q/Vf flags. */
    volatile uint32_t status;  /**< Actual length + status + error control. */
    volatile uint32_t token;   /**< PID, address, endpoint, toggle, max length. */
    volatile uint32_t buffer;  /**< Physical data buffer address. */
    uint32_t pad[4];           /**< Software padding to 32 bytes. */
} uhci_td_t;

/** UHCI queue head (first 8 bytes are hardware-defined). */
typedef struct __attribute__((aligned(16))) {
    volatile uint32_t head;     /**< Horizontal link to the next QH. */
    volatile uint32_t element;  /**< Vertical link to the first TD. */
    uint32_t pad[2];
} uhci_qh_t;

#define TD_T        0x00000001u  /**< Link terminate. */
#define TD_Q        0x00000002u  /**< Link points to a QH. */
#define TD_Vf       0x00000004u  /**< Depth-first execution. */

#define TD_STS_ACTIVE   (1u << 23)
#define TD_STS_STALLED  (1u << 22)
#define TD_STS_ERRMASK  (0x7Eu << 16)
#define TD_IOS          (1u << 25)  /**< Isochronous (unused). */
#define TD_LS           (1u << 26)  /**< Low-speed transfer. */
#define TD_CERR3        (3u << 27)  /**< 3 error retries. */
#define TD_SPD          (1u << 29)  /**< Short-packet detect. */
#define TD_ACTLEN(s)    (((s) & 0x7FF))
#define TD_MAXLEN_NONE  0x7FF

#define TOKEN(pid, addr, ep, toggle, len) \
    ((uint32_t)(pid) | ((uint32_t)(addr) << 8) | ((uint32_t)(ep) << 15) | \
     ((uint32_t)((toggle) & 1) << 19) | \
     ((uint32_t)((((len) == 0) ? TD_MAXLEN_NONE : ((len) - 1)) & 0x7FF) << 21))

/* ------------------------------------------------------------------ *
 *  Static schedule / buffers                                          *
 * ------------------------------------------------------------------ */

#define NUM_INT_SLOTS 4

static volatile uint32_t frame_list[1024] __attribute__((aligned(4096))); /**< The 1024-entry frame list. */
static uhci_qh_t qh_int[NUM_INT_SLOTS];  /**< Periodic (interrupt) queue heads. */
static uhci_qh_t qh_ctrl;                /**< The shared control queue head. */

/* Control transfer staging (SETUP + up to 8 data TDs + STATUS; 16-byte
 * aligned so the controller can walk them). */
static uhci_td_t ctrl_td[16];
static uint8_t   ctrl_setup_buf[8]  __attribute__((aligned(16)));
static uint8_t   ctrl_data_buf[512] __attribute__((aligned(16)));

/* One persistent TD and report buffer per interrupt slot. */
static uhci_td_t int_td[NUM_INT_SLOTS];
static uint8_t   int_buf[NUM_INT_SLOTS][64] __attribute__((aligned(16)));

/** Per-interrupt-slot software state. */
static struct {
    int      used;
    uint8_t  addr;
    uint8_t  endpoint;
    int      maxlen;
    int      toggle;
    int      ls;
} int_slot[NUM_INT_SLOTS];

static uint16_t io_base;   /**< Controller I/O base port. */
static int      have_hc;   /**< Non-zero once initialised. */

/* ------------------------------------------------------------------ *
 *  Register helpers                                                   *
 * ------------------------------------------------------------------ */

/** @brief Read a UHCI word register at @p off. */
static uint16_t rd16(uint16_t off) { return inportw(io_base + off); }
/** @brief Write a UHCI word register at @p off. */
static void     wr16(uint16_t off, uint16_t v) { outportw(io_base + off, v); }
/** @brief Write a UHCI dword register at @p off. */
static void     wr32(uint16_t off, uint32_t v) { outportl(io_base + off, v); }

/**
 * @brief Millisecond delay. Runs only from the USB kernel thread, so it can
 *        yield via the PIT-tick sleep().
 */
static void uhci_delay(int ms) {
    if(ms > 0)
        sleep(ms);
}

/* ------------------------------------------------------------------ *
 *  PCI discovery                                                      *
 * ------------------------------------------------------------------ */

/**
 * @brief Scan the PCI bus for a serial-bus / USB / UHCI function.
 * @param bus,dev,fn Out: its location.
 * @return 1 if found.
 */
static int find_uhci(uint8_t *bus, uint8_t *dev, uint8_t *fn) {
    for(uint32_t b = 0; b < 256; b++)
        for(uint32_t d = 0; d < 32; d++)
            for(uint32_t f = 0; f < 8; f++) {
                uint32_t id = pci_read(b, d, f, PCI_VENDOR_DEVICE);
                if(id == 0xFFFFFFFF)
                    continue;
                uint32_t cls = pci_read(b, d, f, PCI_CLASS_SUBCLASS);
                uint8_t classcode = (cls >> 24) & 0xFF;
                uint8_t subclass  = (cls >> 16) & 0xFF;
                uint8_t progif    = (cls >> 8)  & 0xFF;
                if(classcode == 0x0C && subclass == 0x03 && progif == 0x00) {
                    *bus = b; *dev = d; *fn = f;
                    return 1;
                }
            }
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Initialisation                                                     *
 * ------------------------------------------------------------------ */

int uhci_init(void) {
    uint8_t bus, dev, fn;
    if(!find_uhci(&bus, &dev, &fn)) {
        klogf(LOG_INFO, "UHCI: no controller found\n");
        return 0;
    }

    /* BAR4 holds the I/O base (bit 0 set => I/O space). */
    uint32_t bar4 = pci_read(bus, dev, fn, PCI_BAR4);
    io_base = bar4 & ~0x3u;

    /* Enable I/O space + bus mastering, disable the legacy-keyboard trap. */
    uint32_t cmd = pci_read(bus, dev, fn, 0x04);
    pci_write(bus, dev, fn, 0x04, (cmd & 0xFFFF0000u) | 0x05);
    pci_write(bus, dev, fn, 0xC0, 0x8F00);   /* PIIX3 USB_LEGKEY: clear traps */

    klogf(LOG_INFO, "UHCI: controller at %u:%u.%u, io 0x%x\n", bus, dev, fn, io_base);

    /* Global reset, then host-controller reset. */
    wr16(UHCI_USBCMD, UHCI_CMD_GRESET);
    uhci_delay(15);
    wr16(UHCI_USBCMD, 0);
    uhci_delay(5);
    wr16(UHCI_USBCMD, UHCI_CMD_HCRESET);
    for(int i = 0; i < 100 && (rd16(UHCI_USBCMD) & UHCI_CMD_HCRESET); i++)
        uhci_delay(1);

    wr16(UHCI_USBINTR, 0);           /* poll, no interrupts */
    wr16(UHCI_FRNUM, 0);
    outportb(io_base + UHCI_SOFMOD, 64);

    /* Skeleton: qh_int[0] -> qh_int[1] -> ... -> qh_ctrl -> terminate. */
    memset((void *)qh_int, 0, sizeof(qh_int));
    for(int i = 0; i < NUM_INT_SLOTS; i++) {
        qh_int[i].head = (i + 1 < NUM_INT_SLOTS)
            ? ((uint32_t)&qh_int[i + 1] | TD_Q)
            : ((uint32_t)&qh_ctrl | TD_Q);
        qh_int[i].element = TD_T;
    }
    qh_ctrl.head = TD_T;
    qh_ctrl.element = TD_T;

    for(int i = 0; i < 1024; i++)
        frame_list[i] = (uint32_t)&qh_int[0] | TD_Q;

    wr32(UHCI_FRBASEADD, (uint32_t)frame_list);
    wr16(UHCI_USBSTS, 0x3F);         /* clear status */
    wr16(UHCI_USBCMD, UHCI_CMD_RS | UHCI_CMD_CF | UHCI_CMD_MAXP);

    have_hc = 1;
    return 1;
}

int uhci_port_count(void) { return 2; }

/* ------------------------------------------------------------------ *
 *  Root ports                                                         *
 * ------------------------------------------------------------------ */

/** @brief Read root port @p port status/control register. */
static uint16_t port_rd(int port) {
    return rd16(port == 0 ? UHCI_PORTSC1 : UHCI_PORTSC2);
}
/** @brief Write root port @p port status/control register. */
static void port_wr(int port, uint16_t v) {
    wr16(port == 0 ? UHCI_PORTSC1 : UHCI_PORTSC2, v);
}

int uhci_port_reset(int port, usb_speed_t *speed) {
    if(!have_hc || port < 0 || port > 1)
        return 0;

    uint16_t sc = port_rd(port);
    if(!(sc & UHCI_PORT_CCS))
        return 0;   /* nothing attached */

    /* Drive reset for 50 ms, then release and let the port settle. */
    port_wr(port, UHCI_PORT_PR);
    uhci_delay(50);
    port_wr(port, port_rd(port) & ~UHCI_PORT_PR);
    uhci_delay(10);

    /* Enable and clear the change bits (write-1-to-clear). */
    for(int i = 0; i < 10; i++) {
        sc = port_rd(port);
        if(sc & UHCI_PORT_PE)
            break;
        port_wr(port, (sc & ~(UHCI_PORT_CSC | UHCI_PORT_PEC)) | UHCI_PORT_PE);
        uhci_delay(10);
    }
    port_wr(port, port_rd(port) | UHCI_PORT_CSC | UHCI_PORT_PEC);

    sc = port_rd(port);
    if(!(sc & UHCI_PORT_PE))
        return 0;

    *speed = (sc & UHCI_PORT_LSDA) ? USB_SPEED_LOW : USB_SPEED_FULL;
    return 1;
}

/* ------------------------------------------------------------------ *
 *  Control transfers                                                  *
 * ------------------------------------------------------------------ */

/**
 * @brief Poll until @p qh retires its TD chain (or a TD halts), timing out
 *        after roughly @p frames milliframes. The controller is running, so
 *        FRNUM advances once per frame — use it as the clock.
 */
static int wait_qh(uhci_qh_t *qh, int frames) {
    uint16_t last = rd16(UHCI_FRNUM);
    int elapsed = 0;
    while(elapsed < frames) {
        if(qh->element & TD_T)
            return 1;
        for(int t = 0; t < 16; t++) {
            if(ctrl_td[t].token != 0 &&
               !(ctrl_td[t].status & TD_STS_ACTIVE) &&
               (ctrl_td[t].status & (TD_STS_STALLED | TD_STS_ERRMASK)))
                return 0;
        }
        uint16_t now = rd16(UHCI_FRNUM);
        if(now != last) { elapsed++; last = now; }
        inportb(0x80);   /* short I/O delay */
    }
    return 0;
}

int uhci_control(usb_device_t *dev, const usb_setup_t *setup, void *data, int len) {
    if(!have_hc)
        return -1;
    if(len > (int)sizeof(ctrl_data_buf))
        return -1;

    int ls   = (dev->speed == USB_SPEED_LOW);
    int mps  = dev->max_packet0 ? dev->max_packet0 : 8;
    int addr = dev->address;
    int in   = (setup->bmRequestType & USB_DIR_IN);

    memset(ctrl_td, 0, sizeof(ctrl_td));
    memcpy(ctrl_setup_buf, (void *)setup, 8);
    if(!in && data && len > 0)
        memcpy(ctrl_data_buf, data, len);

    uint32_t base_sts = TD_CERR3 | (ls ? TD_LS : 0);
    int n = 0;

    /* SETUP stage (always DATA0). */
    ctrl_td[n].status = base_sts | TD_STS_ACTIVE;
    ctrl_td[n].token  = TOKEN(UHCI_PID_SETUP, addr, 0, 0, 8);
    ctrl_td[n].buffer = (uint32_t)ctrl_setup_buf;
    n++;

    /* DATA stage, packets of mps, toggle starting at DATA1. */
    int toggle = 1;
    int off = 0;
    while(off < len && n < 15) {
        int chunk = len - off;
        if(chunk > mps) chunk = mps;
        ctrl_td[n].status = base_sts | TD_STS_ACTIVE | (in ? TD_SPD : 0);
        ctrl_td[n].token  = TOKEN(in ? UHCI_PID_IN : UHCI_PID_OUT,
                                  addr, 0, toggle, chunk);
        ctrl_td[n].buffer = (uint32_t)(ctrl_data_buf + off);
        toggle ^= 1;
        off += chunk;
        n++;
    }

    /* STATUS stage: opposite direction, DATA1, zero length. */
    ctrl_td[n].status = base_sts | TD_STS_ACTIVE;
    ctrl_td[n].token  = TOKEN(in ? UHCI_PID_OUT : UHCI_PID_IN, addr, 0, 1, 0);
    ctrl_td[n].buffer = 0;
    n++;

    /* Link them depth-first; last one terminates. */
    for(int i = 0; i < n; i++)
        ctrl_td[i].link = (i == n - 1)
            ? TD_T
            : ((uint32_t)&ctrl_td[i + 1] | TD_Vf);

    qh_ctrl.element = (uint32_t)&ctrl_td[0];
    int ok = wait_qh(&qh_ctrl, 100);   /* ~100 frames = 100 ms */
    qh_ctrl.element = TD_T;

    if(!ok)
        return -1;

    /* Sum the actual data-stage lengths. */
    int got = 0;
    for(int i = 1; i < n - 1; i++) {
        uint32_t s = ctrl_td[i].status;
        if(s & (TD_STS_STALLED | TD_STS_ERRMASK))
            return -1;
        int al = (int)(s & 0x7FF);
        got += (al == TD_MAXLEN_NONE) ? 0 : al + 1;
    }
    if(in && data && got > 0)
        memcpy(data, ctrl_data_buf, got > len ? len : got);
    return got;
}

/* ------------------------------------------------------------------ *
 *  Interrupt endpoints                                                *
 * ------------------------------------------------------------------ */

/** @brief (Re)load interrupt-slot @p s TD and hand it to the controller. */
static void int_arm(int s) {
    int_td[s].link   = TD_T;
    int_td[s].status = TD_CERR3 | TD_SPD | TD_STS_ACTIVE |
                       (int_slot[s].ls ? TD_LS : 0);
    int_td[s].token  = TOKEN(UHCI_PID_IN, int_slot[s].addr, int_slot[s].endpoint,
                             int_slot[s].toggle, int_slot[s].maxlen);
    int_td[s].buffer = (uint32_t)int_buf[s];
    qh_int[s].element = (uint32_t)&int_td[s];
}

int uhci_int_claim(usb_device_t *dev, uint8_t endpoint, int maxlen) {
    if(!have_hc)
        return -1;
    for(int s = 0; s < NUM_INT_SLOTS; s++) {
        if(int_slot[s].used)
            continue;
        if(maxlen > (int)sizeof(int_buf[s]))
            maxlen = sizeof(int_buf[s]);
        int_slot[s].used     = 1;
        int_slot[s].addr     = dev->address;
        int_slot[s].endpoint = endpoint & 0x0F;
        int_slot[s].maxlen   = maxlen;
        int_slot[s].toggle   = 0;
        int_slot[s].ls       = (dev->speed == USB_SPEED_LOW);
        memset(&int_td[s], 0, sizeof(int_td[s]));
        int_arm(s);
        return s;
    }
    return -1;
}

int uhci_int_poll(int slot, void *buf, int len) {
    if(slot < 0 || slot >= NUM_INT_SLOTS || !int_slot[slot].used)
        return -1;

    uint32_t s = int_td[slot].status;
    if(s & TD_STS_ACTIVE)
        return 0;   /* no report yet (NAKs keep it active) */

    if(s & (TD_STS_STALLED | TD_STS_ERRMASK)) {
        /* Recover: clear toggle and re-arm. */
        int_slot[slot].toggle = 0;
        int_arm(slot);
        return -1;
    }

    int al = (int)(s & 0x7FF);
    int got = (al == TD_MAXLEN_NONE) ? 0 : al + 1;
    if(got > len) got = len;
    if(got > 0)
        memcpy(buf, int_buf[slot], got);

    int_slot[slot].toggle ^= 1;
    int_arm(slot);
    return got;
}
