/**
 * @file initrd.c
 * @brief Boot RAM disk — relocate the Multiboot module to high memory, map it,
 *        and serve it to the VFS as the block device @c rd.
 *
 * GRUB loads @c /boot/initrd.img (a small FAT16 image) directly behind the
 * kernel, right on top of where the page-table window, the kernel heap and the
 * ELF-load staging area are about to be placed. @ref initrd_relocate copies it
 * out to @ref INITRD_RELOC_BASE — above the highest address any user process
 * can map (image at 8 MiB + a 64 MiB heap ceiling) — while paging is still off.
 *
 * @ref initrd_map identity-maps that block into the kernel directory, and its
 * page-directory slots are advertised via @ref initrd_pde_lo / @ref
 * initrd_pde_hi so @c create_address_space (vmm.c) clones them into every
 * process, letting a @c run / @c open syscall reach the disk on the caller's
 * own CR3. The device hooks then just memcpy through a shared 512-byte scratch
 * sector, the same read-then-consume contract as ata.c / floppy.c.
 */
#include <linux/initrd.h>

#include <linux/align.h>
#include <linux/device.h>
#include <linux/fat.h>
#include <linux/string.h>
#include <linux/log.h>
#include <asm/memmap.h>

#include <asm/paging.h>

#define SECTOR_SIZE 512

/* Relocation target for the module: INITRD_RELOC_BASE (mm.h), 128 MiB. A user
 * process's address space runs from its image at 8 MiB up through a 64 MiB
 * per-process heap ceiling (proc.h PROC_HEAP_MAX), and its secondary threads'
 * slots occupy [UTHREAD_REGION_BASE, UTHREAD_REGION_END) -- which ends exactly
 * here, so all of it sits below this. Booting needs a machine with a little
 * over 136 MiB of RAM; a smaller box falls back to a real disk (initrd_phys_start
 * stays 0). */

uint32_t initrd_mod_start = 0, initrd_mod_end = 0;
uint32_t initrd_phys_start = 0, initrd_phys_end = 0;
uint32_t payload_mod_start = 0, payload_mod_end = 0;
uint32_t payload_phys_start = 0, payload_phys_end = 0;

/** @return non-zero if @p cmdline has @p word as a whole space-separated word. */
static int has_word(const char *cmdline, const char *word) {
    size_t n = strlen((char *) word);
    for (const char *p = cmdline; p && *p; ) {
        while (*p == ' ')
            p++;
        const char *e = p;
        while (*e && *e != ' ')
            e++;
        if ((size_t) (e - p) == n && strncmp((char *) p, (char *) word, n) == 0)
            return 1;
        p = e;
    }
    return 0;
}

void initrd_note_module(uint32_t start, uint32_t end, const char *cmdline) {
    if (has_word(cmdline, "payload")) {
        if (!payload_mod_start) {
            payload_mod_start = start;
            payload_mod_end   = end;
        }
    } else if (!initrd_mod_start) {
        /* The first module that is not the payload is the boot RAM disk. */
        initrd_mod_start = start;
        initrd_mod_end   = end;
    }
}

/** @return non-zero if [@p base, @p end) lies wholly inside one E820 RAM range. */
static int ram_covers(uint32_t base, uint32_t end) {
    for (int i = 0; i < e820counter; i++) {
        if (e820table[i].type != E820_TYPE_RAM)
            continue;
        uint64_t b = e820table[i].base_address;
        uint64_t e = b + e820table[i].size;
        if ((uint64_t) base >= b && (uint64_t) end <= e)
            return 1;
    }
    return 0;
}

void initrd_relocate(void) {
    if (!initrd_mod_start || initrd_mod_end <= initrd_mod_start) {
        klogf(LOG_INFO, "initrd: no boot module; root fs must come from a disk\n");
        return;
    }

    uint32_t size  = initrd_mod_end - initrd_mod_start;
    uint32_t asize = ALIGN_UP(size, PAGE_SIZE);
    uint32_t dst   = INITRD_RELOC_BASE;

    if (initrd_mod_end > dst || !ram_covers(dst, dst + asize)) {
        klogf(LOG_ERR, "initrd: %u KiB won't fit at 0x%x; ignoring module\n",
              size / 1024, dst);
        return;
    }

    /* Paging is still off here: these are physical addresses. */
    memmove((void *) dst, (void *) initrd_mod_start, size);

    initrd_phys_start = dst;
    initrd_phys_end   = dst + asize;
    klogf(LOG_INFO, "initrd: %u KiB module 0x%x -> 0x%x\n",
          size / 1024, initrd_mod_start, dst);

    /* The installer's payload, if the loader supplied one, goes right behind it:
     * one block of high memory, so the page-directory range cloned into every
     * process (initrd_pde_hi) stays a single span. Both sources lie below the
     * destination, so moving the RAM disk first cannot overwrite the payload. */
    if (payload_mod_start && payload_mod_end > payload_mod_start) {
        uint32_t psize  = payload_mod_end - payload_mod_start;
        uint32_t pasize = ALIGN_UP(psize, PAGE_SIZE);
        uint32_t pdst   = initrd_phys_end;

        if (payload_mod_end > INITRD_RELOC_BASE || !ram_covers(pdst, pdst + pasize)) {
            klogf(LOG_ERR, "initrd: %u KiB payload won't fit at 0x%x; ignoring it\n",
                  psize / 1024, pdst);
        } else {
            memmove((void *) pdst, (void *) payload_mod_start, psize);
            payload_phys_start = pdst;
            payload_phys_end   = pdst + pasize;
            klogf(LOG_INFO, "initrd: %u KiB payload module 0x%x -> 0x%x\n",
                  psize / 1024, payload_mod_start, pdst);
        }
    }
}

void initrd_map(void) {
    if (!initrd_phys_start)
        return;
    for (uint32_t va = initrd_phys_start; va < initrd_phys_end; va += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), va, va, PAGE_PRESENT | PAGE_RW);
    for (uint32_t va = payload_phys_start; va < payload_phys_end; va += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), va, va, PAGE_PRESENT | PAGE_RW);
}

int initrd_pde_lo(void) {
    return initrd_phys_start ? (int) (initrd_phys_start >> 22) : -1;
}

int initrd_pde_hi(void) {
    if (!initrd_phys_start)
        return -1;
    uint32_t end = payload_phys_end ? payload_phys_end : initrd_phys_end;
    return (int) ((end - 1) >> 22);
}

/* ------------------------------------------------------------------ *
 *  Block device                                                       *
 * ------------------------------------------------------------------ */

/** Shared scratch sector — same read-then-consume contract as ata_buf. */
static uint8_t  rd_scratch[SECTOR_SIZE];
static uint32_t rd_base;    /**< Base (identity-mapped) of the relocated image. */
static uint32_t rd_nsect;   /**< Image size in 512-byte sectors. */
static device_t rd_dev;

char *ramdisk_read(int lba) {
    if ((uint32_t) lba >= rd_nsect)
        memset(rd_scratch, 0, SECTOR_SIZE);
    else
        memcpy(rd_scratch, (void *) (rd_base + (uint32_t) lba * SECTOR_SIZE),
               SECTOR_SIZE);
    return (char *) rd_scratch;
}

int ramdisk_write(int lba) {
    if ((uint32_t) lba >= rd_nsect)
        return 0;
    memcpy((void *) (rd_base + (uint32_t) lba * SECTOR_SIZE), rd_scratch,
           SECTOR_SIZE);
    return 1;
}

void ramdisk_init(void) {
    if (!initrd_phys_start)
        return;

    rd_base  = initrd_phys_start;
    rd_nsect = (initrd_phys_end - initrd_phys_start) / SECTOR_SIZE;

    rd_dev.id   = 0;
    rd_dev.type = 1;
    rd_dev.kind = BLK_KIND_RAMDISK;
    rd_dev.sectors = rd_nsect;
    strcpy(rd_dev.mount, "rd");
    rd_dev.read  = &ramdisk_read;
    rd_dev.write = &ramdisk_write;
    fat_init(&rd_dev.fs);
    device_register(&rd_dev);   /* -> vfs_mount -> fat_mount + fat_defrag */

    klogf(LOG_INFO, "initrd: %u KiB RAM disk mounted as rd\n",
          rd_nsect / 2);
}

/* ------------------------------------------------------------------ *
 *  The installer's payload                                            *
 * ------------------------------------------------------------------ */

static uint8_t  pl_scratch[SECTOR_SIZE];
static uint32_t pl_base, pl_nsect;
static device_t pl_dev;

static char *payload_read(int lba) {
    if ((uint32_t) lba >= pl_nsect)
        memset(pl_scratch, 0, SECTOR_SIZE);
    else
        memcpy(pl_scratch, (void *) (pl_base + (uint32_t) lba * SECTOR_SIZE), SECTOR_SIZE);
    return (char *) pl_scratch;
}

/** The payload is read-only: whatever the loader gave us is what gets installed. */
static int payload_write(int lba) {
    (void) lba;
    return 0;
}

void payload_init(void) {
    if (!payload_phys_start)
        return;

    pl_base  = payload_phys_start;
    pl_nsect = (payload_mod_end - payload_mod_start) / SECTOR_SIZE;

    pl_dev.id   = 0;                    /* not in the VFS table: unused */
    pl_dev.type = 1;
    strcpy(pl_dev.mount, "pl");
    pl_dev.kind    = BLK_KIND_PAYLOAD;
    pl_dev.sectors = pl_nsect;
    pl_dev.read  = &payload_read;
    pl_dev.write = &payload_write;
    device_attach_raw(&pl_dev);

    klogf(LOG_INFO, "initrd: %u KiB installation payload present (block device pl)\n",
          pl_nsect / 2);
}
