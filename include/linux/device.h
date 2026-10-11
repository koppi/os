/**
 * @file device.h
 * @brief Block-device registry sitting between the VFS and the disk drivers.
 */
#pragma once

#include <linux/vfs.h>
#include <linux/fat_mount.h>
#include <linux/blk.h>     /* BLK_KIND_*, BLK_MAX */

/** A registered block device (a RAM disk, a floppy, an ATA / SATA / NVMe disk). */
typedef struct device {
    int id;                     /**< Slot / VFS device id. */
    int type;                   /**< 0 = floppy, 1 = ATA HDD. */
    char mount[4];              /**< Mount name, e.g. "fda", "hda". */
    char* (*read) (int lba);    /**< Read one sector; returns a shared buffer. */
    int (*write) (int lba);     /**< Write the shared buffer to a sector. */
    filesystem fs;              /**< Filesystem op vector (FAT). */
    fat_mount_info_t minfo;     /**< Parsed FAT geometry for this volume. */

    int kind;                   /**< One of BLK_KIND_*. */
    uint32_t sectors;           /**< Capacity in 512-byte sectors; 0 if unknown. */
    char model[41];             /**< Drive model string, NUL-terminated; may be empty. */
    /** First sector of the FAT volume within the device: 0 for a bare FAT disk,
     *  the partition's start when the disk is partitioned (see @ref fat_mount).
     *  Filesystem code goes through @ref dev_read / @ref dev_write, which add it;
     *  the raw block syscalls use @c read / @c write directly. */
    uint32_t lba_base;
    /** Push earlier writes out to the medium (NULL: nothing to push). */
    int (*flush) (void);
    /** The driver's shared sector buffer, without reading anything into it. Lets the raw
     *  block write path fill a whole sector and write it, rather than read it first just to
     *  learn where the buffer is. NULL: the driver does not say, so read first. */
    char *(*scratch) (void);
} device_t;

/** @brief Read volume-relative sector @p lba. Same shared-buffer contract as @c read. */
static inline char *dev_read(device_t *d, int lba) { return d->read(lba + (int) d->lba_base); }
/** @brief Write the shared buffer to volume-relative sector @p lba. */
static inline int dev_write(device_t *d, int lba) { return d->write(lba + (int) d->lba_base); }

/**
 * Set by the raw block write path while it streams many sectors: the drivers skip
 * their per-sector cache flush while it is non-zero, and the caller flushes once
 * at the end. A flush per sector is invisible in an emulator and ruinous on a real
 * SSD.
 */
extern int blk_defer_flush;

/** @brief Register @p dev and mount its filesystem. */
void device_register(device_t *dev);
/** @brief Look up a device by the first 3 chars of @p name (skips a leading '/'). */
device_t *get_dev_by_name(char *name);
/** @brief Record @p dev as a block device without mounting a filesystem on it. */
void device_attach_raw(device_t *dev);
/** @brief Block device number @p index (0..9: the VFS slots, then the raw-only
 *         ones), or NULL. For @c block/blk.c; see @ref BLK_MAX. */
device_t *device_at(int index);

/** @brief Look up a device by numeric id. */
device_t *get_dev_by_id(int id);
/** @brief Like @ref get_dev_by_name but returns the id (or -1). */
int get_dev_id_by_name(char *name);
