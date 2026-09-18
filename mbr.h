/**
 * @file mbr.h
 * @brief Master Boot Record layout and partition-type IDs.
 */
#pragma once

#include <types.h>

#define FAT12_SYSTEM_ID             0x1  /**< Partition type: FAT12. */
#define FAT16_SYSTEM_ID             0x4  /**< Partition type: FAT16 (<32 MiB). */
#define NTFS_SYSTEM_ID              0x7  /**< Partition type: NTFS/exFAT. */
#define FAT32_SYSTEM_ID             0xC  /**< Partition type: FAT32 (LBA). */

/** One 16-byte partition-table entry. */
typedef struct partition {
    uint8_t bootable;        /**< 0x80 = active/bootable. */
    uint8_t start_head;      /**< CHS start head. */
    uint16_t start_sect_cyl; /**< CHS start sector/cylinder. */
    uint8_t sys_id;          /**< Partition type ID. */
    uint8_t end_head;        /**< CHS end head. */
    uint16_t end_sect_cyl;   /**< CHS end sector/cylinder. */
    uint32_t lba_start;      /**< First LBA of the partition. */
    uint32_t total_sectors;  /**< Partition length in sectors. */
} __attribute__((__packed__)) partition_t;

/** A 512-byte MBR sector. */
typedef struct mbr {
    uint8_t fill[436];                   /**< Boot code / padding. */
    uint8_t id[10];                      /**< Optional disk signature area. */
    partition_t partition_table[4];      /**< The four primary partitions. */
    uint8_t signature[2];                /**< 0x55 0xAA boot signature. */
} __attribute__((__packed__)) mbr_t;
