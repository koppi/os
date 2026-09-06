/**
 * @file virtio_gpu.c
 * @brief virtio-gpu 2D driver: modern (1.0) virtio-pci transport, one control
 *        queue, and a scanout resource whose backing pages are the framebuffer
 *        shadow so presenting a frame is a transfer + flush.
 *
 * Polled throughout, in the style of [e1000.c](e1000.c) / [hda.c](hda.c): no
 * MSI-X, no device interrupt is unmasked, every command waits on the used ring.
 * All DMA-visible memory is either the identity-mapped kernel heap (the vring,
 * the mem-entry list) or an identity-mapped `.bss` scratch (requests /
 * responses) -- a kernel *stack* is `vmm_map`'d and its VA is not its PA.
 */
#include <virtio_gpu.h>

#include <pci.h>
#include <io.h>
#include <log.h>
#include <cmdline.h>
#include <paging.h>
#include <kheap.h>
#include <memory.h>
#include <lib/string.h>
#include <video.h>

/* ------------------------------------------------------------------ *
 *  virtio-pci (modern) register map                                   *
 * ------------------------------------------------------------------ */
#define PCI_CAP_ID_VNDR          0x09

#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG    3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4

/* Offsets into the common-config structure (virtio 1.0, 4.1.4.3). */
#define CC_DEVICE_FEATURE_SELECT 0x00
#define CC_DEVICE_FEATURE        0x04
#define CC_DRIVER_FEATURE_SELECT 0x08
#define CC_DRIVER_FEATURE        0x0C
#define CC_MSIX_CONFIG           0x10
#define CC_NUM_QUEUES            0x12
#define CC_DEVICE_STATUS         0x14
#define CC_CONFIG_GENERATION     0x15
#define CC_QUEUE_SELECT          0x16
#define CC_QUEUE_SIZE            0x18
#define CC_QUEUE_MSIX_VECTOR     0x1A
#define CC_QUEUE_ENABLE          0x1C
#define CC_QUEUE_NOTIFY_OFF      0x1E
#define CC_QUEUE_DESC            0x20   /* 64-bit */
#define CC_QUEUE_DRIVER          0x28   /* 64-bit */
#define CC_QUEUE_DEVICE          0x30   /* 64-bit */

#define ST_ACK          1
#define ST_DRIVER       2
#define ST_DRIVER_OK    4
#define ST_FEATURES_OK  8

#define VIRTIO_F_VERSION_1  32         /* feature bit */

/* ------------------------------------------------------------------ *
 *  Split virtqueue                                                    *
 * ------------------------------------------------------------------ */
#define VQ_SZ 16                        /* control queue depth (>= 2, pow2) */

#define VRING_DESC_F_NEXT       1
#define VRING_DESC_F_WRITE      2
#define VRING_AVAIL_F_NO_INTERRUPT 1

#define PCI_CMD_INTX_DISABLE   (1u << 10)  /* mask legacy INTx from the device */

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VQ_SZ];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[VQ_SZ];
} __attribute__((packed));

/* ------------------------------------------------------------------ *
 *  virtio-gpu 2D protocol (subset)                                    *
 * ------------------------------------------------------------------ */
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO      0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D    0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF        0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT           0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH        0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D   0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106

#define VIRTIO_GPU_RESP_OK_NODATA            0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO      0x1101

#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM     2    /* LE bytes: B G R X == shadow */

#define VIRTIO_GPU_MAX_SCANOUTS 16

struct vg_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} __attribute__((packed));

struct vg_rect { uint32_t x, y, width, height; } __attribute__((packed));

struct vg_disp_one {
    struct vg_rect r;
    uint32_t enabled;
    uint32_t flags;
} __attribute__((packed));

struct vg_resp_disp_info {
    struct vg_ctrl_hdr hdr;
    struct vg_disp_one pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute__((packed));

struct vg_res_create_2d {
    struct vg_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __attribute__((packed));

struct vg_res_unref {
    struct vg_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed));

struct vg_set_scanout {
    struct vg_ctrl_hdr hdr;
    struct vg_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
} __attribute__((packed));

struct vg_res_attach_backing {
    struct vg_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} __attribute__((packed));

struct vg_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute__((packed));

struct vg_transfer_2d {
    struct vg_ctrl_hdr hdr;
    struct vg_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed));

struct vg_res_flush {
    struct vg_ctrl_hdr hdr;
    struct vg_rect r;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed));

/* ------------------------------------------------------------------ *
 *  State                                                              *
 * ------------------------------------------------------------------ */

/* Largest mode we will follow the host into: the bigger of this and the mode
 * GRUB handed over (never shrink below the panel the machine booted at). The
 * shadow is grown to it once so a resize never reallocates -- 1920x1200 x4 is
 * 9 MiB / three page tables. */
#define VG_CAP_W 1920
#define VG_CAP_H 1200

static uint32_t vg_max_w = VG_CAP_W;
static uint32_t vg_max_h = VG_CAP_H;

static volatile uint8_t  *cc;            /* common config (identity MMIO)    */
static volatile uint8_t  *isr;           /* ISR status (mapped, unused)      */
static volatile uint16_t *vq_notify;     /* control-queue notify doorbell    */

static struct vring_desc  *vq_desc;
static struct vring_avail *vq_avail;
static struct vring_used  *vq_used;
static uint16_t            vq_last_used;

/* Request / response staging. Small, and in .bss so VA == PA for the device. */
static uint8_t g_req[256]  __attribute__((aligned(16)));
static uint8_t g_resp[512] __attribute__((aligned(16)));

static int      g_active;
static uint32_t g_res_id = 1;            /* current scanout resource id      */

int virtio_gpu_active(void) { return g_active; }

/* ------------------------------------------------------------------ *
 *  Small MMIO helpers                                                 *
 * ------------------------------------------------------------------ */
static inline uint8_t  cc_r8 (uint32_t o) { return *(volatile uint8_t  *)(cc + o); }
static inline uint16_t cc_r16(uint32_t o) { return *(volatile uint16_t *)(cc + o); }
static inline uint32_t cc_r32(uint32_t o) { return *(volatile uint32_t *)(cc + o); }
static inline void cc_w8 (uint32_t o, uint8_t  v) { *(volatile uint8_t  *)(cc + o) = v; }
static inline void cc_w16(uint32_t o, uint16_t v) { *(volatile uint16_t *)(cc + o) = v; }
static inline void cc_w32(uint32_t o, uint32_t v) { *(volatile uint32_t *)(cc + o) = v; }
static inline void cc_w64(uint32_t o, uint64_t v) {
    cc_w32(o, (uint32_t) v);
    cc_w32(o + 4, (uint32_t) (v >> 32));
}

/** @brief Identity-map @p len bytes of MMIO starting at @p pa (cache-disabled). */
static void map_mmio(uint32_t pa, uint32_t len) {
    uint32_t start = pa & ~(PAGE_SIZE - 1u);
    uint32_t end   = (pa + len + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
    for (uint32_t p = start; p < end; p += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), p, p, PAGE_PRESENT | PAGE_RW | PAGE_PCD);
}

/* ------------------------------------------------------------------ *
 *  Control queue                                                      *
 * ------------------------------------------------------------------ */

/** One request/response buffer pair. */
struct vg_iov { uint32_t buf; uint32_t len; };

/**
 * @brief Post a descriptor chain (@p nout device-readable, then @p nin
 *        device-writable), ring the doorbell and wait for the used ring.
 * @return 0 on completion, -1 on timeout.
 */
static int vq_cmd(const struct vg_iov *iov, int nout, int nin) {
    int n = nout + nin;
    for (int i = 0; i < n; i++) {
        vq_desc[i].addr  = iov[i].buf;
        vq_desc[i].len   = iov[i].len;
        vq_desc[i].flags = (uint16_t) ((i + 1 < n ? VRING_DESC_F_NEXT : 0) |
                                       (i >= nout ? VRING_DESC_F_WRITE : 0));
        vq_desc[i].next  = (uint16_t) (i + 1);
    }

    vq_avail->ring[vq_avail->idx % VQ_SZ] = 0;   /* chain head is descriptor 0 */
    asm volatile("sfence" ::: "memory");
    vq_avail->idx++;
    asm volatile("sfence" ::: "memory");

    *vq_notify = 0;                              /* notify queue 0 */

    for (uint32_t spin = 0; spin < 1000000u; spin++) {
        if (vq_used->idx != vq_last_used) {
            vq_last_used = vq_used->idx;
            return 0;
        }
        inportb(0x80);
    }
    klogf(LOG_ERR, "virtio-gpu: control-queue timeout (cmd 0x%x)\n",
          ((struct vg_ctrl_hdr *) iov[0].buf)->type);
    return -1;
}

/** @return 0 if the response header carries an OK_* code. */
static int resp_ok(void) {
    uint32_t t = ((struct vg_ctrl_hdr *) g_resp)->type;
    return (t >= 0x1100 && t < 0x1200) ? 0 : -1;
}

/** @brief Issue the command staged in @ref g_req, response into @ref g_resp. */
static int vg_cmd(uint32_t reqlen, uint32_t resplen) {
    struct vg_iov iov[2] = {
        { (uint32_t) g_req,  reqlen  },
        { (uint32_t) g_resp, resplen },
    };
    if (vq_cmd(iov, 1, 1) != 0)
        return -1;
    return resp_ok();
}

/* ------------------------------------------------------------------ *
 *  virtio-gpu commands                                                *
 * ------------------------------------------------------------------ */

static int vg_get_display_info(uint32_t *pw, uint32_t *ph) {
    struct vg_ctrl_hdr *r = (void *) g_req;
    memset(r, 0, sizeof *r);
    r->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    struct vg_iov iov[2] = {
        { (uint32_t) g_req,  sizeof *r },
        { (uint32_t) g_resp, sizeof(struct vg_resp_disp_info) },
    };
    if (vq_cmd(iov, 1, 1) != 0)
        return -1;

    struct vg_resp_disp_info *resp = (void *) g_resp;
    if (resp->hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO || !resp->pmodes[0].enabled)
        return -1;
    *pw = resp->pmodes[0].r.width;
    *ph = resp->pmodes[0].r.height;
    return (*pw && *ph) ? 0 : -1;
}

static int vg_create_2d(uint32_t id, uint32_t w, uint32_t h) {
    struct vg_res_create_2d *r = (void *) g_req;
    memset(r, 0, sizeof *r);
    r->hdr.type   = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    r->resource_id = id;
    r->format      = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    r->width       = w;
    r->height      = h;
    return vg_cmd(sizeof *r, sizeof(struct vg_ctrl_hdr));
}

static int vg_unref(uint32_t id) {
    struct vg_res_unref *r = (void *) g_req;
    memset(r, 0, sizeof *r);
    r->hdr.type    = VIRTIO_GPU_CMD_RESOURCE_UNREF;
    r->resource_id = id;
    return vg_cmd(sizeof *r, sizeof(struct vg_ctrl_hdr));
}

static int vg_set_scanout(uint32_t sid, uint32_t rid, uint32_t w, uint32_t h) {
    struct vg_set_scanout *r = (void *) g_req;
    memset(r, 0, sizeof *r);
    r->hdr.type     = VIRTIO_GPU_CMD_SET_SCANOUT;
    r->r.width      = w;
    r->r.height     = h;
    r->scanout_id   = sid;
    r->resource_id  = rid;
    return vg_cmd(sizeof *r, sizeof(struct vg_ctrl_hdr));
}

static int vg_transfer(uint32_t id, int x, int y, int w, int h) {
    struct vg_transfer_2d *r = (void *) g_req;
    memset(r, 0, sizeof *r);
    r->hdr.type    = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    r->r.x = (uint32_t) x; r->r.y = (uint32_t) y;
    r->r.width = (uint32_t) w; r->r.height = (uint32_t) h;
    r->offset      = (uint64_t) ((uint32_t) y * vbemem.pitch + (uint32_t) x * 4u);
    r->resource_id = id;
    return vg_cmd(sizeof *r, sizeof(struct vg_ctrl_hdr));
}

static int vg_flush(uint32_t id, int x, int y, int w, int h) {
    struct vg_res_flush *r = (void *) g_req;
    memset(r, 0, sizeof *r);
    r->hdr.type    = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    r->r.x = (uint32_t) x; r->r.y = (uint32_t) y;
    r->r.width = (uint32_t) w; r->r.height = (uint32_t) h;
    r->resource_id = id;
    return vg_cmd(sizeof *r, sizeof(struct vg_ctrl_hdr));
}

/** @brief Attach the first @p w*@p h*4 bytes of the shadow as @p id's backing. */
static int vg_attach_backing(uint32_t id, uint32_t w, uint32_t h) {
    uint32_t bytes = w * 4u * h;
    uint32_t n     = (bytes + PAGE_SIZE - 1u) / PAGE_SIZE;

    struct vg_res_attach_backing *r = (void *) g_req;
    memset(r, 0, sizeof *r);
    r->hdr.type    = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    r->resource_id = id;
    r->nr_entries  = n;

    struct vg_mem_entry *ents = kmalloc(n * sizeof *ents);
    if (!ents)
        return -1;
    uint32_t sva = (uint32_t) vbemem.buffer;
    for (uint32_t i = 0; i < n; i++) {
        ents[i].addr   = (uint32_t) get_phys_addr(get_kern_directory(),
                                                  sva + i * PAGE_SIZE);
        ents[i].length = PAGE_SIZE;
        ents[i].padding = 0;
    }

    struct vg_iov iov[3] = {
        { (uint32_t) r,      sizeof *r },
        { (uint32_t) ents,   n * (uint32_t) sizeof *ents },
        { (uint32_t) g_resp, sizeof(struct vg_ctrl_hdr) },
    };
    int rc = vq_cmd(iov, 2, 1);
    kfree(ents);
    return (rc == 0) ? resp_ok() : -1;
}

/* ------------------------------------------------------------------ *
 *  Present hook + resize                                              *
 * ------------------------------------------------------------------ */

/** @brief video.c present hook: push a dirty rect of the shadow to the host.
 *         Runs under video_lock(). */
static void virtio_gpu_present(int x, int y, int w, int h) {
    if (!g_active)
        return;
    if (vg_transfer(g_res_id, x, y, w, h) == 0)
        vg_flush(g_res_id, x, y, w, h);
}

/** @brief Switch the desktop to @p w x @p h. Runs under video_lock(). */
static void virtio_gpu_resize(uint32_t w, uint32_t h) {
    uint32_t nid = (g_res_id == 1) ? 2 : 1;

    video_set_geometry(w, h);

    if (vg_create_2d(nid, w, h) != 0)
        return;
    if (vg_attach_backing(nid, w, h) != 0) {
        vg_unref(nid);
        return;
    }
    vg_set_scanout(0, nid, w, h);
    vg_transfer(nid, 0, 0, (int) w, (int) h);
    vg_flush(nid, 0, 0, (int) w, (int) h);

    vg_unref(g_res_id);
    g_res_id = nid;
    klogf(LOG_INFO, "virtio-gpu: mode %ux%u\n", w, h);
}

void virtio_gpu_thread(void) {
    if (!g_active) {
        for (;;)
            halt();
    }
    for (;;) {
        sleep(300);

        uint32_t w = 0, h = 0;
        video_lock();
        if (vg_get_display_info(&w, &h) == 0) {
            if (w > vg_max_w) w = vg_max_w;   /* honour as much as the buffer allows */
            if (h > vg_max_h) h = vg_max_h;
            if (w >= 320 && h >= 200 &&
                (w != vbemem.xres || h != vbemem.yres))
                virtio_gpu_resize(w, h);
        }
        video_unlock();
    }
}

/* ------------------------------------------------------------------ *
 *  Bring-up                                                           *
 * ------------------------------------------------------------------ */

/** @brief Walk the PCI capability list for the modern-virtio structures. */
static int map_virtio_caps(pci_device_t *d, uint32_t *notify_mult) {
    if (!(pci_cfg_read16(d, PCI_STATUS) & (1u << 4))) {
        klogf(LOG_ERR, "virtio-gpu: no PCI capability list\n");
        return -1;
    }

    int have_common = 0, have_notify = 0;
    uint32_t shared_lo = 0xFFFFFFFF, shared_hi = 0;
    uint8_t cap = pci_cfg_read8(d, 0x34) & 0xFC;
    for (int guard = 0; cap && guard < 48; guard++) {
        uint8_t id   = pci_cfg_read8(d, cap + 0);
        uint8_t next = pci_cfg_read8(d, cap + 1);
        if (id == PCI_CAP_ID_VNDR) {
            uint8_t  type = pci_cfg_read8 (d, cap + 3);
            uint8_t  bar  = pci_cfg_read8 (d, cap + 4);
            uint32_t off  = pci_cfg_read32(d, cap + 8);
            uint32_t len  = pci_cfg_read32(d, cap + 12);
            uint32_t base = (bar < 6) ? d->bar[bar].addr : 0;

            if (base) {
                uint32_t rlen = len ? len : PAGE_SIZE;
                map_mmio(base + off, rlen);
                if (base + off < shared_lo) shared_lo = base + off;
                if (base + off + rlen > shared_hi) shared_hi = base + off + rlen;
                switch (type) {
                case VIRTIO_PCI_CAP_COMMON_CFG:
                    cc = (volatile uint8_t *) (base + off);
                    have_common = 1;
                    break;
                case VIRTIO_PCI_CAP_NOTIFY_CFG:
                    vq_notify = (volatile uint16_t *) (base + off);
                    *notify_mult = pci_cfg_read32(d, cap + 16);
                    have_notify = 1;
                    break;
                case VIRTIO_PCI_CAP_ISR_CFG:
                    isr = (volatile uint8_t *) (base + off);
                    break;
                default:
                    break;
                }
            }
        }
        cap = next & 0xFC;
    }

    if (!have_common || !have_notify) {
        klogf(LOG_ERR, "virtio-gpu: missing common/notify capability\n");
        return -1;
    }
    /* One shared_pde entry for the whole virtio register window (all the caps
     * QEMU places sit in one BAR), so a present from a user CR3 still reaches it. */
    if (shared_hi > shared_lo)
        vmm_share_kernel_range(shared_lo & ~(PAGE_SIZE - 1u),
                               shared_hi - (shared_lo & ~(PAGE_SIZE - 1u)));
    return 0;
}

/** @brief Negotiate VERSION_1 and arm the control queue. */
static int virtio_bringup(uint32_t notify_mult) {
    cc_w8(CC_DEVICE_STATUS, 0);                       /* reset */
    for (int i = 0; i < 100000 && cc_r8(CC_DEVICE_STATUS); i++)
        inportb(0x80);
    cc_w8(CC_DEVICE_STATUS, ST_ACK);
    cc_w8(CC_DEVICE_STATUS, ST_ACK | ST_DRIVER);

    cc_w32(CC_DEVICE_FEATURE_SELECT, 1);
    uint32_t feat_hi = cc_r32(CC_DEVICE_FEATURE);
    if (!(feat_hi & (1u << (VIRTIO_F_VERSION_1 - 32)))) {
        klogf(LOG_ERR, "virtio-gpu: device is not VIRTIO_F_VERSION_1\n");
        return -1;
    }
    cc_w32(CC_DRIVER_FEATURE_SELECT, 1);
    cc_w32(CC_DRIVER_FEATURE, 1u << (VIRTIO_F_VERSION_1 - 32));
    cc_w32(CC_DRIVER_FEATURE_SELECT, 0);
    cc_w32(CC_DRIVER_FEATURE, 0);

    cc_w8(CC_DEVICE_STATUS, ST_ACK | ST_DRIVER | ST_FEATURES_OK);
    if (!(cc_r8(CC_DEVICE_STATUS) & ST_FEATURES_OK)) {
        klogf(LOG_ERR, "virtio-gpu: FEATURES_OK rejected\n");
        return -1;
    }

    /* Control queue (0). One page from the identity-mapped heap holds all
     * three rings; VA == PA there, which is what the device needs. */
    cc_w16(CC_QUEUE_SELECT, 0);
    uint16_t qmax = cc_r16(CC_QUEUE_SIZE);
    if (qmax < VQ_SZ) {
        klogf(LOG_ERR, "virtio-gpu: control queue too small (%u)\n", qmax);
        return -1;
    }

    uint8_t *ring = kmalloc(PAGE_SIZE);
    if (!ring)
        return -1;
    memset(ring, 0, PAGE_SIZE);
    uint32_t b = ((uint32_t) ring + 15u) & ~15u;
    vq_desc  = (struct vring_desc  *) b;                 /* 256 B */
    vq_avail = (struct vring_avail *) (b + 256);         /*  36 B */
    vq_used  = (struct vring_used  *) ((b + 256 + 36 + 3) & ~3u);
    vq_last_used = 0;
    /* Polled: never ask the device to raise a used-buffer interrupt. */
    vq_avail->flags = VRING_AVAIL_F_NO_INTERRUPT;

    cc_w16(CC_QUEUE_SIZE, VQ_SZ);
    cc_w16(CC_QUEUE_MSIX_VECTOR, 0xFFFF);
    cc_w64(CC_QUEUE_DESC,   (uint32_t) vq_desc);
    cc_w64(CC_QUEUE_DRIVER, (uint32_t) vq_avail);
    cc_w64(CC_QUEUE_DEVICE, (uint32_t) vq_used);
    cc_w16(CC_QUEUE_ENABLE, 1);

    uint16_t noff = cc_r16(CC_QUEUE_NOTIFY_OFF);
    vq_notify = (volatile uint16_t *) ((uint32_t) vq_notify + noff * notify_mult);

    cc_w8(CC_DEVICE_STATUS, ST_ACK | ST_DRIVER | ST_FEATURES_OK | ST_DRIVER_OK);
    return 0;
}

void virtio_gpu_probe(pci_device_t *d) {
    if (cmdline_has("novirtiogpu"))
        return;
    if (!video_has_shadow()) {
        klogf(LOG_WARNING, "virtio-gpu: no framebuffer shadow, staying on VGA\n");
        return;
    }

    /* MMIO + bus-master for the queues; INTx masked -- the driver is polled and
     * nothing installs a handler for this line (a pending, level-triggered INTx
     * would fault the first time interrupts are enabled). */
    pci_enable(d, PCI_CMD_MEM | PCI_CMD_MASTER | PCI_CMD_INTX_DISABLE);

    uint32_t notify_mult = 0;
    if (map_virtio_caps(d, &notify_mult) != 0)
        return;
    if (virtio_bringup(notify_mult) != 0)
        return;

    /* Never mode below the resolution GRUB booted at. Enlarge the shadow so any
     * mode up to that ceiling fits without reallocation. */
    if (vbemem.xres > vg_max_w) vg_max_w = vbemem.xres;
    if (vbemem.yres > vg_max_h) vg_max_h = vbemem.yres;
    if (!video_grow_shadow(vg_max_w, vg_max_h)) {
        klogf(LOG_ERR, "virtio-gpu: cannot size the back buffer, staying on VGA\n");
        return;
    }

    uint32_t w = vbemem.xres, h = vbemem.yres;
    g_res_id = 1;
    if (vg_create_2d(g_res_id, w, h) != 0 ||
        vg_attach_backing(g_res_id, w, h) != 0 ||
        vg_set_scanout(0, g_res_id, w, h) != 0) {
        klogf(LOG_ERR, "virtio-gpu: scanout setup failed, staying on VGA\n");
        return;
    }
    vg_transfer(g_res_id, 0, 0, (int) w, (int) h);
    vg_flush(g_res_id, 0, 0, (int) w, (int) h);

    g_active = 1;
    video_present_hook = virtio_gpu_present;
    klogf(LOG_INFO, "virtio-gpu: scanout %ux%u, window-follow resize enabled\n",
          w, h);
}
