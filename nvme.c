/**
 * @file nvme.c
 * @brief NVM Express 1.x block driver: polled, one admin + one I/O queue pair,
 *        a single command in flight, 512-byte sector read/write behind the
 *        @ref device_t block interface.
 *
 * Deliberately small: no MSI/MSI-X, no interrupts, no SGLs, no namespace
 * management, no multi-queue. Namespace 1 of the first controller is mounted as
 * the next free hd{a,b,...}. This is the disk path on a laptop whose M.2 slot
 * carries an NVMe SSD instead of a SATA one — the ThinkPad T470s ships that way.
 *
 * The queues and the bounce buffer live in identity-mapped .bss (phys == virt,
 * all < 4 MiB) so a command's PRP entries can carry their address directly. The
 * controller BAR is mapped 1:1, uncacheable, and shared into every address
 * space (the block read/write path runs syscall-side on the caller's CR3).
 */
#include <nvme.h>

#include <pci.h>
#include <device.h>
#include <fat.h>
#include <io.h>
#include <paging.h>
#include <pit.h>
#include <cmdline.h>
#include <log.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ *
 *  Controller register block (NVMe 1.4, section 3.1)                  *
 * ------------------------------------------------------------------ */
#define NVME_CAP   0x00      /* Controller Capabilities (64-bit)      */
#define NVME_VS    0x08      /* Version                               */
#define NVME_INTMS 0x0C      /* Interrupt Mask Set                    */
#define NVME_INTMC 0x10      /* Interrupt Mask Clear                  */
#define NVME_CC    0x14      /* Controller Configuration              */
#define NVME_CSTS  0x1C      /* Controller Status                     */
#define NVME_AQA   0x24      /* Admin Queue Attributes                */
#define NVME_ASQ   0x28      /* Admin SQ Base Address (64-bit)        */
#define NVME_ACQ   0x30      /* Admin CQ Base Address (64-bit)        */
#define NVME_DBS   0x1000    /* Doorbell registers                    */

#define CC_EN        (1u << 0)
#define CC_CSS_NVM   (0u << 4)
#define CC_MPS_4K    (0u << 7)
#define CC_AMS_RR    (0u << 11)
#define CC_IOSQES_64 (6u << 16)   /* 2^6 = 64-byte SQ entries */
#define CC_IOCQES_16 (4u << 20)   /* 2^4 = 16-byte CQ entries */

#define CSTS_RDY     (1u << 0)
#define CSTS_CFS     (1u << 1)

/* Admin command set */
#define ADM_CREATE_SQ 0x01
#define ADM_CREATE_CQ 0x05
#define ADM_IDENTIFY  0x06

/* NVM command set */
#define IO_FLUSH 0x00
#define IO_WRITE 0x01
#define IO_READ  0x02

#define QD      8          /* admin + I/O queue depth (entries)           */
#define IO_QID  1          /* the one I/O queue pair we create            */
#define MAX_NS  2          /* namespaces we will mount (hd letters)       */

/* ------------------------------------------------------------------ *
 *  DMA memory — identity-mapped .bss, page aligned (phys == virt)     *
 * ------------------------------------------------------------------ */
static uint8_t asq_mem[4096] __attribute__((aligned(4096)));  /* admin SQ */
static uint8_t acq_mem[4096] __attribute__((aligned(4096)));  /* admin CQ */
static uint8_t isq_mem[4096] __attribute__((aligned(4096)));  /* I/O SQ   */
static uint8_t icq_mem[4096] __attribute__((aligned(4096)));  /* I/O CQ   */
/* One page-aligned bounce buffer: IDENTIFY data during bring-up, then the
 * single-sector DMA target for every read/write (nvme_cmd is synchronous, so
 * the two uses never overlap). */
static uint8_t dma4k [4096] __attribute__((aligned(4096)));
static uint8_t sect_scratch[512];

/* ------------------------------------------------------------------ *
 *  State                                                             *
 * ------------------------------------------------------------------ */
struct nvme_queue {
    volatile uint32_t *sq;      /* 16 dwords / entry */
    volatile uint32_t *cq;      /*  4 dwords / entry */
    volatile uint32_t *sq_db;
    volatile uint32_t *cq_db;
    uint16_t sq_tail, cq_head;
    uint8_t  phase;             /* CQ phase tag we are waiting for */
    uint16_t cid;
};

static volatile uint8_t *bar;         /* controller registers (1:1, UC)   */
static struct nvme_queue admin_q, io_q;
static pci_device_t *nvme_pci;
static uint32_t ns_nsid[MAX_NS];      /* namespace id behind hd<letter>   */
static int      n_ns;
static int      cur_ns;               /* selected by the rd/wr shims      */
static device_t dev_info[MAX_NS];

/* ------------------------------------------------------------------ *
 *  Register helpers                                                   *
 * ------------------------------------------------------------------ */
static uint32_t r32(uint32_t o)          { return *(volatile uint32_t *)(bar + o); }
static void     w32(uint32_t o, uint32_t v) { *(volatile uint32_t *)(bar + o) = v; }

/** @brief Wait for CSTS.RDY to reach @p want (0/1), or CFS / timeout. */
static int wait_ready(int want, uint32_t timeout_ms) {
    uint32_t start = pit_ms();
    for (;;) {
        uint32_t csts = r32(NVME_CSTS);
        if (csts & CSTS_CFS)
            return -1;
        if ((int)(csts & CSTS_RDY) == want)
            return 0;
        if (pit_ms() - start > timeout_ms)
            return -1;
        pit_busywait_ms(1);
    }
}

/* ------------------------------------------------------------------ *
 *  Command submission (fully polled, one in flight)                   *
 * ------------------------------------------------------------------ */
/**
 * @brief Push a 16-dword command onto @p q, ring the doorbell and spin on the
 *        completion queue until the phase tag flips.
 * @param cmd    CDW0..CDW15; CID (CDW0[31:16]) is filled in here.
 * @param result Optional out: CDW0 of the completion entry.
 * @return 0 on success, or the negated NVMe status field on error / timeout.
 */
static int nvme_cmd(struct nvme_queue *q, const uint32_t cmd[16], uint32_t *result) {
    volatile uint32_t *slot = q->sq + (uint32_t)q->sq_tail * 16;
    uint16_t cid = q->cid++;
    slot[0] = (cmd[0] & 0x0000FFFFu) | ((uint32_t)cid << 16);
    for (int i = 1; i < 16; i++)
        slot[i] = cmd[i];

    q->sq_tail = (q->sq_tail + 1) % QD;
    asm volatile("" ::: "memory");
    *q->sq_db = q->sq_tail;

    volatile uint32_t *ce = q->cq + (uint32_t)q->cq_head * 4;
    uint32_t start = pit_ms(), dw3;
    for (;;) {
        dw3 = ce[3];
        if (((dw3 >> 16) & 1u) == q->phase)
            break;
        if (pit_ms() - start > 5000) {
            klogf(LOG_WARNING, "nvme: command 0x%x timed out\n", cmd[0] & 0xFF);
            return -1;
        }
        __builtin_ia32_pause();
    }

    if (result)
        *result = ce[0];

    uint32_t status = (dw3 >> 17) & 0x7FFu;

    q->cq_head = (q->cq_head + 1) % QD;
    if (q->cq_head == 0)
        q->phase ^= 1;
    asm volatile("" ::: "memory");
    *q->cq_db = q->cq_head;

    if (status) {
        klogf(LOG_WARNING, "nvme: command 0x%x status 0x%x\n",
              cmd[0] & 0xFF, status);
        return -(int)status;
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 *  device_t shims — one 512-byte sector, RMW convention               *
 * ------------------------------------------------------------------ */
static char *nvme_read_sector(int lba) {
    uint32_t cmd[16] = { 0 };
    cmd[0]  = IO_READ;
    cmd[1]  = ns_nsid[cur_ns];
    cmd[6]  = (uint32_t)(uintptr_t)dma4k;    /* PRP1 (512B, page-aligned) */
    cmd[10] = (uint32_t)lba;                  /* SLBA low  */
    cmd[11] = 0;                              /* SLBA high */
    cmd[12] = 0;                              /* NLB = 0 -> one block      */
    if (nvme_cmd(&io_q, cmd, 0) != 0)
        memset(dma4k, 0, 512);
    memcpy(sect_scratch, dma4k, 512);
    return (char *)sect_scratch;
}

static int nvme_write_sector(int lba) {
    memcpy(dma4k, sect_scratch, 512);
    uint32_t cmd[16] = { 0 };
    cmd[0]  = IO_WRITE;
    cmd[1]  = ns_nsid[cur_ns];
    cmd[6]  = (uint32_t)(uintptr_t)dma4k;
    cmd[10] = (uint32_t)lba;
    cmd[12] = 0;
    if (nvme_cmd(&io_q, cmd, 0) != 0)
        return 0;

    uint32_t fl[16] = { 0 };
    fl[0] = IO_FLUSH;
    fl[1] = ns_nsid[cur_ns];
    nvme_cmd(&io_q, fl, 0);
    return 1;
}

static char *rd0(int lba) { cur_ns = 0; return nvme_read_sector(lba); }
static int   wr0(int lba) { cur_ns = 0; return nvme_write_sector(lba); }
static char *rd1(int lba) { cur_ns = 1; return nvme_read_sector(lba); }
static int   wr1(int lba) { cur_ns = 1; return nvme_write_sector(lba); }

/* ------------------------------------------------------------------ *
 *  PCI bind hook                                                      *
 * ------------------------------------------------------------------ */
void nvme_probe(struct pci_device *d) {
    if (!nvme_pci)
        nvme_pci = (pci_device_t *)d;
}

/* ------------------------------------------------------------------ *
 *  Bring-up                                                           *
 * ------------------------------------------------------------------ */
/** @brief Next free hd{a,b,...} letter, scanning what is already registered
 *         (the RAM disk takes id 0; IDE / AHCI may have taken hda..). */
static char next_hd_letter(void) {
    char c = 'a';
    for (int i = 0; i < 8; i++) {
        device_t *e = get_dev_by_id(i);
        if (e && e->mount[0] == 'h' && e->mount[1] == 'd' && e->mount[2] >= c)
            c = e->mount[2] + 1;
    }
    return c;
}

static void mount_namespace(uint32_t nsid, uint64_t nsze, uint32_t bs) {
    if (n_ns >= MAX_NS)
        return;

    int idx = n_ns;
    int did = 0;
    while (did < 8 && get_dev_by_id(did))
        did++;
    if (did >= 8) {
        klogf(LOG_WARNING, "nvme: device table full\n");
        return;
    }

    ns_nsid[idx] = nsid;

    device_t *dev = &dev_info[idx];
    memset(dev, 0, sizeof(*dev));
    dev->id   = did;
    dev->type = 1;
    strcpy(dev->mount, "hd");
    dev->mount[2] = next_hd_letter();
    dev->mount[3] = 0;
    dev->read  = idx == 0 ? rd0 : rd1;
    dev->write = idx == 0 ? wr0 : wr1;
    fat_init(&dev->fs);
    device_register(dev);
    n_ns++;

    klogf(LOG_INFO, "nvme: nsid %u -> %s (%u MiB, %u-byte blocks)\n",
          nsid, dev->mount,
          (unsigned)((nsze * bs) / (1024u * 1024u)), bs);
}

void nvme_init(void) {
    if (cmdline_has("nonvme")) {
        klogf(LOG_INFO, "nvme: disabled on the command line\n");
        return;
    }
    if (!nvme_pci) {
        const pci_device_t *d = pci_get_by_class(0x01, 0x08);
        if (!d)
            return;
        nvme_pci = (pci_device_t *)d;
    }
    pci_device_t *d = nvme_pci;

    /* BAR0 is the 64-bit controller register block. A UEFI firmware can park it
     * above 4 GiB where this 32-bit kernel cannot map it — re-home it into a
     * free slice of the low PCI hole (same trick as xhci.c). */
    uint32_t span = d->bar[0].size ? d->bar[0].size : 0x2000;
    if (span < 0x2000)
        span = 0x2000;                    /* need the doorbells past 0x1000 */
    uint32_t base = d->bar[0].addr;
    uint32_t base_hi = d->bar[0].is64 ? pci_cfg_read32(d, 0x14) : 0;
    if (base_hi != 0 || base == 0) {
        uint32_t nb = pci_mmio_hole(span);
        if (!nb) {
            klogf(LOG_WARNING, "nvme: no 32-bit MMIO hole for the BAR\n");
            return;
        }
        pci_cfg_write32(d, 0x10, nb);
        if (d->bar[0].is64)
            pci_cfg_write32(d, 0x14, 0);
        /* Reflect the move in the device table so a later re-homer (xhci) does
         * not pick the same slice out of pci_mmio_hole(). */
        d->bar[0].addr = nb;
        d->bar[0].is64 = 0;
        base = nb;
        klogf(LOG_INFO, "nvme: BAR re-homed to 0x%x\n", nb);
    }

    /* Bus master for the queue DMA, memory decode for the BAR, and INTx#
     * disable (bit 10) — this driver is fully polled and never installs an
     * interrupt handler for the controller's IRQ line. */
    pci_enable(d, PCI_CMD_MEM | PCI_CMD_MASTER | (1u << 10));

    for (uint32_t o = 0; o < span; o += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), base + o, base + o,
                     PAGE_PRESENT | PAGE_RW | PAGE_PCD | PAGE_PWT);
    vmm_share_kernel_range(base, span);
    bar = (volatile uint8_t *)base;

    uint32_t cap_lo = r32(NVME_CAP), cap_hi = r32(NVME_CAP + 4);
    uint32_t dstrd  = cap_hi & 0xF;                 /* CAP.DSTRD  */
    uint32_t mqes   = (cap_lo & 0xFFFF) + 1;        /* CAP.MQES (1-based) */
    uint32_t to_ms  = ((cap_lo >> 24) & 0xFF) * 500u + 1000u;
    uint32_t mpsmin = (cap_hi >> 16) & 0xF;         /* CAP.MPSMIN */
    uint32_t css    = (cap_hi >> 5)  & 0xFF;        /* CAP.CSS    */
    uint32_t stride = 4u << dstrd;

    klogf(LOG_INFO, "nvme: BAR 0x%x  ver 0x%x  cap 0x%x:%x  mqes %u  dstrd %u\n",
          base, r32(NVME_VS), cap_hi, cap_lo, mqes, dstrd);

    if (mpsmin != 0) {
        klogf(LOG_WARNING, "nvme: MPSMIN %u (>4KiB pages) unsupported\n", mpsmin);
        return;
    }
    if (!(css & 1)) {
        klogf(LOG_WARNING, "nvme: NVM command set not supported (css 0x%x)\n", css);
        return;
    }

    uint32_t qd = QD;
    if (qd > mqes)
        qd = mqes;

    /* Disable the controller before touching the admin-queue registers. */
    uint32_t cc = r32(NVME_CC);
    if (cc & CC_EN) {
        w32(NVME_CC, cc & ~CC_EN);
        if (wait_ready(0, to_ms) < 0) {
            klogf(LOG_WARNING, "nvme: controller stuck enabled\n");
            return;
        }
    }
    w32(NVME_INTMS, 0xFFFFFFFF);          /* poll only — mask every vector */

    /* Admin queue pair. */
    memset(asq_mem, 0, sizeof(asq_mem));
    memset(acq_mem, 0, sizeof(acq_mem));
    admin_q.sq = (volatile uint32_t *)asq_mem;
    admin_q.cq = (volatile uint32_t *)acq_mem;
    admin_q.sq_db = (volatile uint32_t *)(bar + NVME_DBS + 0 * stride);
    admin_q.cq_db = (volatile uint32_t *)(bar + NVME_DBS + 1 * stride);
    admin_q.sq_tail = admin_q.cq_head = 0;
    admin_q.phase = 1;
    admin_q.cid = 0;

    w32(NVME_AQA, ((qd - 1) << 16) | (qd - 1));
    w32(NVME_ASQ, (uint32_t)(uintptr_t)asq_mem);
    w32(NVME_ASQ + 4, 0);
    w32(NVME_ACQ, (uint32_t)(uintptr_t)acq_mem);
    w32(NVME_ACQ + 4, 0);

    w32(NVME_CC, CC_CSS_NVM | CC_MPS_4K | CC_AMS_RR |
                 CC_IOSQES_64 | CC_IOCQES_16 | CC_EN);
    if (wait_ready(1, to_ms) < 0) {
        klogf(LOG_WARNING, "nvme: controller did not become ready\n");
        return;
    }

    /* IDENTIFY controller (CNS=1) — for the log line, and to learn NN. */
    uint32_t cmd[16] = { 0 };
    cmd[0]  = ADM_IDENTIFY;
    cmd[6]  = (uint32_t)(uintptr_t)dma4k;
    cmd[10] = 1;                          /* CNS = 1: controller */
    memset(dma4k, 0, sizeof(dma4k));
    if (nvme_cmd(&admin_q, cmd, 0) != 0) {
        klogf(LOG_WARNING, "nvme: IDENTIFY controller failed\n");
        return;
    }
    {
        char mn[41];
        memcpy(mn, dma4k + 24, 40);
        mn[40] = 0;
        for (int i = 39; i >= 0 && (mn[i] == ' ' || mn[i] == 0); i--)
            mn[i] = 0;
        uint32_t nn = *(uint32_t *)(dma4k + 516);    /* NN: number of namespaces */
        klogf(LOG_INFO, "nvme: model '%s', %u namespace(s)\n", mn, nn);
    }

    /* Create the I/O completion queue (qid 1), then the submission queue that
     * feeds it. Both physically contiguous, interrupts disabled. */
    memset(icq_mem, 0, sizeof(icq_mem));
    memset(isq_mem, 0, sizeof(isq_mem));
    io_q.sq = (volatile uint32_t *)isq_mem;
    io_q.cq = (volatile uint32_t *)icq_mem;
    io_q.sq_db = (volatile uint32_t *)(bar + NVME_DBS + (2 * IO_QID) * stride);
    io_q.cq_db = (volatile uint32_t *)(bar + NVME_DBS + (2 * IO_QID + 1) * stride);
    io_q.sq_tail = io_q.cq_head = 0;
    io_q.phase = 1;
    io_q.cid = 0;

    memset(cmd, 0, sizeof(cmd));
    cmd[0]  = ADM_CREATE_CQ;
    cmd[6]  = (uint32_t)(uintptr_t)icq_mem;          /* PRP1 */
    cmd[10] = ((qd - 1) << 16) | IO_QID;             /* QSIZE | QID */
    cmd[11] = 1;                                     /* PC=1, IEN=0, IV=0 */
    if (nvme_cmd(&admin_q, cmd, 0) != 0) {
        klogf(LOG_WARNING, "nvme: CREATE I/O CQ failed\n");
        return;
    }

    memset(cmd, 0, sizeof(cmd));
    cmd[0]  = ADM_CREATE_SQ;
    cmd[6]  = (uint32_t)(uintptr_t)isq_mem;          /* PRP1 */
    cmd[10] = ((qd - 1) << 16) | IO_QID;             /* QSIZE | QID */
    cmd[11] = ((uint32_t)IO_QID << 16) | 1;          /* CQID | PC=1 */
    if (nvme_cmd(&admin_q, cmd, 0) != 0) {
        klogf(LOG_WARNING, "nvme: CREATE I/O SQ failed\n");
        return;
    }

    /* Active Namespace ID list (CNS=2): NSIDs in increasing order, 0-terminated.
     * Probing this instead of 1..NN keeps us from issuing an IDENTIFY against a
     * hole (NN is the max NSID, not the active count). We only ever mount the
     * first MAX_NS, so copy just those out before dma4k is reused. Fall back to
     * a bare {1} on a pre-1.1 controller that rejects CNS=2. */
    uint32_t nsid_list[MAX_NS] = { 0 };
    int n_list = 0;
    memset(cmd, 0, sizeof(cmd));
    cmd[0]  = ADM_IDENTIFY;
    cmd[6]  = (uint32_t)(uintptr_t)dma4k;
    cmd[10] = 2;                                     /* CNS = 2 */
    memset(dma4k, 0, sizeof(dma4k));
    if (nvme_cmd(&admin_q, cmd, 0) == 0) {
        const uint32_t *list = (const uint32_t *)dma4k;
        for (int i = 0; i < 1024 && list[i] && n_list < MAX_NS; i++)
            nsid_list[n_list++] = list[i];
    } else {
        nsid_list[0] = 1;
        n_list = 1;
    }

    for (int i = 0; i < n_list && n_ns < MAX_NS; i++) {
        uint32_t nsid = nsid_list[i];

        memset(cmd, 0, sizeof(cmd));
        cmd[0]  = ADM_IDENTIFY;
        cmd[1]  = nsid;
        cmd[6]  = (uint32_t)(uintptr_t)dma4k;
        cmd[10] = 0;                                 /* CNS = 0: namespace */
        memset(dma4k, 0, sizeof(dma4k));
        if (nvme_cmd(&admin_q, cmd, 0) != 0)
            continue;

        uint64_t nsze = *(uint64_t *)(dma4k + 0);
        if (nsze == 0)
            continue;                                /* inactive namespace */

        uint8_t  flbas = dma4k[26] & 0x0F;
        uint32_t lbaf  = *(uint32_t *)(dma4k + 128 + flbas * 4);
        uint32_t lbads = (lbaf >> 16) & 0xFF;
        uint32_t bs    = lbads ? (1u << lbads) : 512;

        if (bs != 512) {
            klogf(LOG_INFO, "nvme: nsid %u has %u-byte blocks, not mounting\n",
                  nsid, bs);
            continue;
        }
        mount_namespace(nsid, nsze, bs);
    }

    if (!n_ns)
        klogf(LOG_INFO, "nvme: no mountable namespaces\n");
}

int nvme_disk_count(void) { return n_ns; }
