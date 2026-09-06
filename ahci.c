/**
 * @file ahci.c
 * @brief AHCI 1.x SATA driver: polled, a single command in flight per port,
 *        512-byte sector read/write behind the @ref device_t block interface.
 *
 * Deliberately small: no NCQ, no interrupts, no port-multiplier, no ATAPI.
 * Enough to mount the FAT volume on a laptop's M.2 SATA SSD. DMA structures
 * live in identity-mapped .bss so their virtual address is their physical
 * address; the HBA MMIO (ABAR / BAR5) is mapped 1:1 cache-disabled.
 */
#include <ahci.h>

#include <pci.h>
#include <device.h>
#include <fat.h>
#include <io.h>
#include <paging.h>
#include <lib/string.h>
#include <log.h>
#include <pit.h>

/* ------------------------------------------------------------------ *
 *  HBA / port register layout (AHCI spec 1.3.1)                       *
 * ------------------------------------------------------------------ */
typedef volatile struct {
    uint32_t clb, clbu, fb, fbu, is, ie, cmd, rsv0;
    uint32_t tfd, sig, ssts, sctl, serr, sact, ci, sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} hba_port_t;

typedef volatile struct {
    uint32_t cap, ghc, is, pi, vs, ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32];
} hba_mem_t;

typedef struct {
    uint16_t flags;          /* cfl:5 a:1 w:1 p:1 r:1 b:1 c:1 rsv:1 pmp:4 */
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba, ctbau;
    uint32_t rsv[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct {
    uint32_t dba, dbau, rsv0;
    uint32_t dbc;            /* bit0..21 = byte count - 1, bit31 = interrupt */
} __attribute__((packed)) hba_prdt_t;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    hba_prdt_t prdt[1];
} __attribute__((packed)) hba_cmd_tbl_t;

typedef struct {
    uint8_t  type;           /* 0x27 = Register FIS - host to device */
    uint8_t  pmp_c;          /* bit7 = command */
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0, lba1, lba2, device;
    uint8_t  lba3, lba4, lba5, featureh;
    uint8_t  countl, counth, icc, control;
    uint8_t  rsv[4];
} __attribute__((packed)) fis_h2d_t;

#define GHC_AE      (1u << 31)
#define GHC_HR      (1u << 0)
#define CAP2_BOH    (1u << 0)
#define BOHC_BOS    (1u << 0)
#define BOHC_OOS    (1u << 1)
#define BOHC_BB     (1u << 4)

#define PxCMD_ST    (1u << 0)
#define PxCMD_FRE   (1u << 4)
#define PxCMD_FR    (1u << 14)
#define PxCMD_CR    (1u << 15)

#define PxIS_TFES   (1u << 30)

#define ATA_CMD_IDENTIFY   0xEC
#define ATA_CMD_READ_DMA_EX  0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_FLUSH_EXT    0xEA

#define SATA_SIG_ATA  0x00000101

#define MAX_PORTS 2   /* first two ports with a disk -> hda, hdb */

/* ------------------------------------------------------------------ *
 *  Per-port DMA structures (identity-mapped .bss, phys == virt)       *
 * ------------------------------------------------------------------ */
static hba_cmd_header_t cmd_list[MAX_PORTS][32] __attribute__((aligned(1024)));
static uint8_t          rx_fis  [MAX_PORTS][256] __attribute__((aligned(256)));
static hba_cmd_tbl_t    cmd_tbl [MAX_PORTS]      __attribute__((aligned(128)));
static uint8_t          dma_buf [MAX_PORTS][512] __attribute__((aligned(64)));

static uint8_t          sect_scratch[MAX_PORTS][512];
static device_t         dev_info[MAX_PORTS];

static hba_mem_t *hba;
static struct pci_device *ahci_pci;
static int n_disks;

/* Which physical HBA port each hd{a,b} maps to, and its slot in the arrays. */
static struct { int hw_port; } disk[MAX_PORTS];
static int cur_disk;   /* selected by the device read/write shims */

/* ------------------------------------------------------------------ *
 *  Low level                                                          *
 * ------------------------------------------------------------------ */
static void stop_port(hba_port_t *p) {
    p->cmd &= ~PxCMD_ST;
    p->cmd &= ~PxCMD_FRE;
    for (int i = 0; i < 500000; i++)
        if (!(p->cmd & (PxCMD_FR | PxCMD_CR)))
            break;
}

static void start_port(hba_port_t *p) {
    for (int i = 0; i < 500000 && (p->cmd & PxCMD_CR); i++)
        ;
    p->cmd |= PxCMD_FRE;
    p->cmd |= PxCMD_ST;
}

/** @brief Run one ATA command on port slot 0. @return 0 on success. */
static int port_exec(int idx, uint8_t cmd, uint64_t lba, int write, uint32_t bytes) {
    hba_port_t *p = &hba->ports[disk[idx].hw_port];
    int slot = idx;   /* one dedicated slot per disk keeps the arrays simple */

    p->is = (uint32_t) -1;                 /* clear port interrupt status */
    p->serr = (uint32_t) -1;

    hba_cmd_header_t *hdr = &cmd_list[slot][0];
    memset(hdr, 0, sizeof(*hdr));
    hdr->flags = (uint16_t) (sizeof(fis_h2d_t) / 4) | (write ? (1 << 6) : 0);
    hdr->prdtl = 1;
    hdr->ctba  = (uint32_t) &cmd_tbl[slot];
    hdr->ctbau = 0;

    hba_cmd_tbl_t *t = &cmd_tbl[slot];
    memset(t, 0, sizeof(*t));
    t->prdt[0].dba = (uint32_t) &dma_buf[slot][0];
    t->prdt[0].dbc = (bytes ? bytes - 1 : 0);   /* byte count - 1 */

    fis_h2d_t *f = (fis_h2d_t *) t->cfis;
    f->type   = 0x27;
    f->pmp_c  = 0x80;                       /* this is a command */
    f->command = cmd;
    f->device  = 0x40;                      /* LBA mode */
    f->lba0 = lba & 0xFF;
    f->lba1 = (lba >> 8) & 0xFF;
    f->lba2 = (lba >> 16) & 0xFF;
    f->lba3 = (lba >> 24) & 0xFF;
    f->lba4 = (lba >> 32) & 0xFF;
    f->lba5 = (lba >> 40) & 0xFF;
    if (cmd == ATA_CMD_READ_DMA_EX || cmd == ATA_CMD_WRITE_DMA_EX) {
        f->countl = 1;                      /* one sector */
        f->counth = 0;
    }

    /* Wait for BSY / DRQ to clear before issuing. */
    for (int i = 0; i < 1000000; i++)
        if (!(p->tfd & 0x88))
            break;

    p->ci = 1u << slot;

    for (int i = 0; i < 5000000; i++) {
        if (!(p->ci & (1u << slot)))
            break;
        if (p->is & PxIS_TFES)
            return -1;
        __builtin_ia32_pause();
    }
    if (p->ci & (1u << slot))
        return -1;                          /* timed out */
    if (p->is & PxIS_TFES)
        return -1;
    return 0;
}

/* ------------------------------------------------------------------ *
 *  device_t shims                                                     *
 * ------------------------------------------------------------------ */
static char *ahci_read_sector(int lba) {
    int d = cur_disk;
    if (port_exec(d, ATA_CMD_READ_DMA_EX, (uint32_t) lba, 0, 512) == 0)
        memcpy(sect_scratch[d], dma_buf[d], 512);
    else
        memset(sect_scratch[d], 0, 512);
    return (char *) sect_scratch[d];
}

static int ahci_write_sector(int lba) {
    int d = cur_disk;
    memcpy(dma_buf[d], sect_scratch[d], 512);
    if (port_exec(d, ATA_CMD_WRITE_DMA_EX, (uint32_t) lba, 1, 512) != 0)
        return 0;
    port_exec(d, ATA_CMD_FLUSH_EXT, 0, 0, 0);
    return 1;
}

/* The device layer calls read/write with no device handle, so bounce through
 * a per-disk shim that first selects cur_disk. */
static char *rd0(int lba) { cur_disk = 0; return ahci_read_sector(lba); }
static int   wr0(int lba) { cur_disk = 0; return ahci_write_sector(lba); }
static char *rd1(int lba) { cur_disk = 1; return ahci_read_sector(lba); }
static int   wr1(int lba) { cur_disk = 1; return ahci_write_sector(lba); }

/* ------------------------------------------------------------------ *
 *  Bring-up                                                           *
 * ------------------------------------------------------------------ */
void ahci_probe(struct pci_device *d) {
    if (!ahci_pci)
        ahci_pci = d;
}

static int port_has_disk(hba_port_t *p) {
    uint32_t ssts = p->ssts;
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    return det == 3 && ipm == 1 && p->sig == SATA_SIG_ATA;
}

void ahci_init(void) {
    if (!ahci_pci) {
        const pci_device_t *d = pci_get_by_class(0x01, 0x06);
        if (!d) return;
        ahci_pci = (struct pci_device *) d;
    }
    pci_device_t *d = (pci_device_t *) ahci_pci;

    uint32_t abar = d->bar[5].addr;
    if (!abar) {
        klogf(LOG_WARNING, "ahci: controller has no ABAR\n");
        return;
    }
    pci_enable(d, PCI_CMD_MEM | PCI_CMD_MASTER);

    uint32_t span = d->bar[5].size ? d->bar[5].size : 0x2000;
    for (uint32_t off = 0; off < span; off += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), abar + off, abar + off,
                     PAGE_PRESENT | PAGE_RW | PAGE_PCD | PAGE_PWT);
    hba = (hba_mem_t *) abar;

    /* The block read/write path runs syscall-side on the calling process's
     * CR3 (a spawned `ls`/`cat` doing open()), so the ABAR must be visible
     * from every address space, not just kern_dir. */
    vmm_share_kernel_range(abar, span);

    /* BIOS/OS handoff, then put the HBA in AHCI mode. */
    if (hba->cap2 & CAP2_BOH) {
        hba->bohc |= BOHC_OOS;
        for (int i = 0; i < 500000 && (hba->bohc & BOHC_BOS); i++)
            ;
        pit_busywait_ms(25);
    }
    hba->ghc |= GHC_AE;

    uint32_t pi = hba->pi;
    klogf(LOG_INFO, "ahci: ABAR 0x%x  cap 0x%x  ports 0x%x\n",
          abar, (unsigned) hba->cap, (unsigned) pi);

    for (int hw = 0; hw < 32 && n_disks < MAX_PORTS; hw++) {
        if (!(pi & (1u << hw)))
            continue;
        hba_port_t *p = &hba->ports[hw];
        if (!port_has_disk(p))
            continue;

        int idx = n_disks;
        stop_port(p);

        memset(cmd_list[idx], 0, sizeof(cmd_list[idx]));
        memset(rx_fis[idx], 0, sizeof(rx_fis[idx]));
        p->clb  = (uint32_t) &cmd_list[idx][0];
        p->clbu = 0;
        p->fb   = (uint32_t) &rx_fis[idx][0];
        p->fbu  = 0;
        p->serr = (uint32_t) -1;
        p->is   = (uint32_t) -1;

        start_port(p);
        disk[idx].hw_port = hw;

        /* IDENTIFY, mostly to confirm the port answers. */
        if (port_exec(idx, ATA_CMD_IDENTIFY, 0, 0, 512) != 0) {
            klogf(LOG_WARNING, "ahci: port %d IDENTIFY failed\n", hw);
            stop_port(p);
            continue;
        }
        uint16_t *id = (uint16_t *) dma_buf[idx];
        uint32_t sectors = id[60] | ((uint32_t) id[61] << 16);

        device_t *dev = &dev_info[idx];
        memset(dev, 0, sizeof(*dev));
        int did = 0;
        while (did < 8 && get_dev_by_id(did))
            did++;
        dev->id = did;
        dev->type = 1;
        strcpy(dev->mount, "hd");
        dev->mount[2] = 'a' + idx;
        dev->mount[3] = 0;
        dev->read  = idx == 0 ? rd0 : rd1;
        dev->write = idx == 0 ? wr0 : wr1;
        fat_init(&dev->fs);
        device_register(dev);
        n_disks++;

        klogf(LOG_INFO, "ahci: port %d -> %s (%u MiB)\n",
              hw, dev->mount, sectors / 2048);
    }

    if (!n_disks)
        klogf(LOG_INFO, "ahci: no SATA disks\n");
}

int ahci_disk_count(void) { return n_disks; }
