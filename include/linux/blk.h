/**
 * @file blk.h
 * @brief Raw block-device access for ring 3: what the installer (apps/install) reads
 *        and writes disks with.
 *
 * The VFS already gives a program the files of a FAT volume. This is the other level:
 * whole devices as arrays of 512-byte sectors, with no filesystem in between, which is
 * what partitioning a disk, formatting it and writing a boot loader need. Four
 * syscalls, numbered after the window-manager and game-pad ones (syscall.c):
 *
 *   46 blk_info(index, struct blk_info *out)    describe device @c index
 *   47 blk_read(index, lba, buf, count)         read  @c count sectors into @c buf
 *   48 blk_write(index, lba, buf, count)        write @c count sectors from @c buf
 *   49 blk_ctl(index, op)                       BLK_CTL_FLUSH / BLK_CTL_RESCAN
 *
 * A device's @c index is its position in the kernel's table (device_at()): the VFS
 * slots first, then the raw-only ones, so the numbers are stable for a boot but
 * mean nothing beyond it. Walk 0..BLK_MAX-1 and skip what @c blk_info reports as 0.
 *
 * There is no privilege model in this kernel, so the safety is in what can be written
 * at all: only whole disks the kernel can drive (@ref BLK_F_TARGET) -- never the boot
 * RAM disk, the payload or a floppy -- and only inside the disk. Nothing here checks
 * that a disk is not the one a program is running from, because none can be: the root
 * file system is the RAM disk.
 */
#pragma once

#include <linux/types.h>

#define BLK_SECTOR       512
#define BLK_MAX          10         /**< device indexes run 0..BLK_MAX-1 (device_at() in device.h) */

/** What a block device is, as @c blk_info reports it (struct blk_info::kind). */
enum {
    BLK_KIND_NONE = 0,
    BLK_KIND_RAMDISK,           /**< the boot RAM disk (initrd.c) */
    BLK_KIND_PAYLOAD,           /**< the installer's boot payload (initrd.c), read-only */
    BLK_KIND_FLOPPY,
    BLK_KIND_ATA,               /**< legacy IDE, PIO */
    BLK_KIND_AHCI,              /**< SATA */
    BLK_KIND_NVME
};
#define BLK_MAX_COUNT    256        /**< most sectors one read/write call takes */

/** @ref blk_info flags. */
#define BLK_F_TARGET     0x1u       /**< a disk that may be written and installed onto */
#define BLK_F_MOUNTED    0x2u       /**< a FAT volume on it is mounted (as the VFS device @c name) */

/** @ref blk_ctl operations. */
#define BLK_CTL_FLUSH    1          /**< push earlier writes to the medium */
#define BLK_CTL_RESCAN   2          /**< look for partitions and a FAT volume again, and re-mount */

/** What @c blk_info fills in. Layout is the ABI: 72 bytes. */
struct blk_info {
    uint32_t index;                 /**< the number it was asked for */
    uint32_t kind;                  /**< BLK_KIND_* (device.h) */
    uint32_t sectors;               /**< capacity in 512-byte sectors, 0 if unknown */
    uint32_t flags;                 /**< BLK_F_* */
    char     name[8];               /**< mount name: "hda", "rd", ... */
    char     model[44];             /**< drive model, may be empty */
};

/** @name Called from the syscall gate (syscall.c); the arguments are raw user words. */
///@{
uint32_t blk_sys_info(uint32_t index, uint32_t out);
uint32_t blk_sys_read(uint32_t index, uint32_t lba, uint32_t buf, uint32_t count);
uint32_t blk_sys_write(uint32_t index, uint32_t lba, uint32_t buf, uint32_t count);
uint32_t blk_sys_ctl(uint32_t index, uint32_t op);
///@}

/** @brief Print the block devices, one per line (the `lsblk` console command). */
void blk_print(void);
