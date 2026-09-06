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
#include <initrd.h>

#include <align.h>
#include <device.h>
#include <fat.h>
#include <lib/string.h>
#include <log.h>
#include <memmap.h>
#include <mm.h>
#include <paging.h>

#define SECTOR_SIZE 512

/**
 * Relocation target for the module: 128 MiB. A user process's address space
 * runs from its image at 8 MiB up through a 64 MiB per-process heap ceiling
 * (heap.c PROC_HEAP_MAX) plus thread stacks — comfortably below this. Booting
 * needs a machine with a little over 136 MiB of RAM; a smaller box falls back
 * to a real disk (initrd_phys_start stays 0).
 */
#define INITRD_RELOC_BASE 0x08000000u

uint32_t initrd_mod_start = 0, initrd_mod_end = 0;
uint32_t initrd_phys_start = 0, initrd_phys_end = 0;

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
}

void initrd_map(void) {
    if (!initrd_phys_start)
        return;
    for (uint32_t va = initrd_phys_start; va < initrd_phys_end; va += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), va, va, PAGE_PRESENT | PAGE_RW);
}

int initrd_pde_lo(void) {
    return initrd_phys_start ? (int) (initrd_phys_start >> 22) : -1;
}

int initrd_pde_hi(void) {
    return initrd_phys_start ? (int) ((initrd_phys_end - 1) >> 22) : -1;
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
    strcpy(rd_dev.mount, "rd");
    rd_dev.read  = &ramdisk_read;
    rd_dev.write = &ramdisk_write;
    fat_init(&rd_dev.fs);
    device_register(&rd_dev);   /* -> vfs_mount -> fat_mount + fat_defrag */

    klogf(LOG_INFO, "initrd: %u KiB RAM disk mounted as rd\n",
          rd_nsect / 2);
}
