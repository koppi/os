/**
 * @file device.h
 * @brief Block-device registry sitting between the VFS and the disk drivers.
 */
#pragma once

#include <vfs.h>
#include <fat_mount.h>

/** A registered block device (a floppy or an ATA disk). */
typedef struct device {
    int id;                     /**< Slot / VFS device id. */
    int type;                   /**< 0 = floppy, 1 = ATA HDD. */
    char mount[4];              /**< Mount name, e.g. "fda", "hda". */
    char* (*read) (int lba);    /**< Read one sector; returns a shared buffer. */
    int (*write) (int lba);     /**< Write the shared buffer to a sector. */
    filesystem fs;              /**< Filesystem op vector (FAT). */
    fat_mount_info_t minfo;     /**< Parsed FAT geometry for this volume. */
} device_t;

/** @brief Register @p dev and mount its filesystem. */
void device_register(device_t *dev);
/** @brief Look up a device by the first 3 chars of @p name (skips a leading '/'). */
device_t *get_dev_by_name(char *name);
/** @brief Look up a device by numeric id. */
device_t *get_dev_by_id(int id);
/** @brief Like @ref get_dev_by_name but returns the id (or -1). */
int get_dev_id_by_name(char *name);
