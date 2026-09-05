/**
 * @file fat.h
 * @brief FAT12/16 driver — on-disk structures (BPB, boot sector, directory
 *        entry) and the filesystem operations plugged into the VFS.
 */
#pragma once

#include <lib/string.h>
#include <types.h>
#include <vfs.h>
#include <device.h>

#define FAT12       0  /**< minfo.type: FAT12. */
#define FAT16       1  /**< minfo.type: FAT16. */
#define FAT32       2  /**< minfo.type: FAT32 (parsed but not fully supported). */
#define EXFAT       3  /**< minfo.type: exFAT (unsupported). */

/** The DOS 3.31 BIOS Parameter Block (33 bytes at offset 3 of the boot sector). */
typedef struct {
    uint8_t oem_name[8];
    uint16_t sector_bytes;
    uint8_t cluster_sectors;
    uint16_t reserved_sectors;
    uint8_t n_fats;
    uint16_t n_dir_entries;
    uint16_t n_sectors;
    uint8_t media;
    uint16_t fat_sectors;
    uint16_t track_sectors;
    uint16_t cylinder_heads;
    uint32_t hidden_sectors;
    uint32_t long_sectors;
} __attribute((__packed__)) bios_parameter_block_t;

/** The FAT32 extended BPB (28 bytes); only present on FAT32 volumes. */
typedef struct {
    uint32_t fat_sectors;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
    uint16_t info_cluster;
    uint16_t backup_boot;
    uint16_t reserved[6];
} __attribute__((__packed__)) bios_parameter_block_ext_t;

/** A 512-byte FAT boot sector. */
typedef struct {
    uint8_t ignore[3];                   /**< Jump instruction (EB xx 90). */
    bios_parameter_block_t bpb;          /**< The BPB. */
    bios_parameter_block_ext_t bpb_ext;  /**< FAT32 extended BPB. */
    uint8_t fill[448];                   /**< Boot code / padding. */
} __attribute__((__packed__)) bootsector_t;

/** @name Directory-entry attribute bits */
///@{
#define DIR_RO          1
#define DIR_HIDDEN      2
#define DIR_SYS         4
#define DIR_VOL_LABEL   8
#define DIR_SUBDIR      0x10
#define DIR_ARCHIVE     0x20
#define DIR_DEVICE      0x60
///@}

/** A 32-byte 8.3 directory entry. */
typedef struct directory {
    uint8_t filename[8];
    uint8_t extension[3];
    uint8_t attrs;
    uint8_t reserved;
    uint8_t time_created_ms;
    uint16_t time_created;
    uint16_t date_created;
    uint16_t date_last_accessed;
    uint16_t first_cluster_high_bytes;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t first_cluster;
    uint32_t file_size;
} __attribute__((__packed__)) directory_t;

/** Length of an 8.3 name packed with no dot (8 + 3). */
#define NAME_LEN 11

/** @brief Read the boot sector and fill @p dev->minfo with the FAT geometry. */
void fat_mount(device_t *dev);
/** @brief Compact the root-directory files into contiguous, packed clusters,
 *         drawing an fsck-style progress bar. Called at mount time, before the
 *         volume is handed to the VFS. A no-op on anything but a healthy,
 *         less-than-half-full FAT12/FAT16 volume. */
void fat_defrag(device_t *dev);
/** @brief Pack a normal name into an 11-char space-padded 8.3 name. */
void to_dos_file_name(char *name, char *str);
/** @brief Unpack an 11-char 8.3 name into a lowercase "name.ext" string. */
void to_normal_file_name(char *name, char *str);
/** @brief LBA of @p f's current cluster (first_data_sector + (cluster-2)*spc). */
uint32_t get_phys_sector(file *f);
/** @brief Locate @p f's root-directory entry. */
directory_t *fat_get_dir(file *f);
/** @brief Create an empty file @p name. */
int fat_touch(char *name);
/** @brief Read one cluster of @p f into @p buf and advance the FAT chain. */
void fat_read(file *f, char *buf);
/** @brief Write @p str into @p f's first cluster. */
void fat_write(file *f, char *str);
/** @brief Replace @p f's contents with @p len bytes of @p buf (reallocs the
 *         cluster chain and updates the directory entry). */
void fat_write_all(file *f, char *buf, uint32_t len);
/** @brief Delete file @p name (clears its directory entry). */
int fat_delete(char *name);
/** @brief Mark @p f closed. */
void fat_close(file *f);
/** @brief Open a name relative to the volume root. */
file fat_directory(char *dir_name, int devid);
/** @brief Open @p name inside an already-open directory. */
file fat_open_subdir(file directory, char *name);
/** @brief Open the file at path @p name (VFS `open` hook). */
file fat_open(char *name);
/** @brief Resolve directory path @p dir (VFS `cd` hook). */
file fat_cd(char *dir);
/** @brief Walk a "dev/a/b/c" path and return the final entry. */
file fat_search(char *name);
/** @brief List the root directory of device @p dir (VFS `ls` hook). */
void fat_ls(char *dir);
/** @brief Populate a @ref filesystem op vector with the FAT hooks. */
void fat_init(filesystem *fs_fat);
