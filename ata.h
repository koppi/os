/**
 * @file ata.h
 * @brief PIO-mode ATA/IDE disk driver — register map, per-drive state and the
 *        sector-read entry point used by the FAT layer.
 */
#pragma once

#include <types.h>

/** @name Primary channel I/O ports (and its IRQ) */
///@{
#define ATA_PRIMARY_DATA            0x1F0
#define ATA_PRIMARY_ERR             0x1F1
#define ATA_PRIMARY_SECTORS         0x1F2
#define ATA_PRIMARY_LBA_LOW         0x1F3
#define ATA_PRIMARY_LBA_MID         0x1F4
#define ATA_PRIMARY_LBA_HIGH        0x1F5
#define ATA_PRIMARY_DRIVE_SEL       0x1F6
#define ATA_PRIMARY_STATUS          0x1F7
#define ATA_PRIMARY_IRQ             14
///@}

/** @name Secondary channel I/O ports (and its IRQ) */
///@{
#define ATA_SECONDARY_DATA          0x170
#define ATA_SECONDARY_ERR           0x171
#define ATA_SECONDARY_SECTORS       0x172
#define ATA_SECONDARY_LBA_LOW       0x173
#define ATA_SECONDARY_LBA_MID       0x174
#define ATA_SECONDARY_LBA_HIGH      0x175
#define ATA_SECONDARY_DRIVE_SEL     0x176
#define ATA_SECONDARY_STATUS        0x177
#define ATA_SECONDARY_IRQ           15
///@}

#define ATA_IDENTIFY                0xEC  /**< IDENTIFY DEVICE command byte. */

/** State for one ATA drive (one of master/slave on one channel). */
typedef struct ata_drive {
    int present;              /**< Non-zero if IDENTIFY succeeded. */
    int type;                 /**< 1 = master, 0 = slave. */
    uint32_t data_reg;        /**< Data port. */
    uint32_t err_reg;         /**< Error / features port. */
    uint32_t sectors_reg;     /**< Sector-count port. */
    uint32_t lba_low_reg;     /**< LBA bits 0-7. */
    uint32_t lba_mid_reg;     /**< LBA bits 8-15. */
    uint32_t lba_high_reg;    /**< LBA bits 16-23. */
    uint32_t sel_reg;         /**< Drive/head select port. */
    uint32_t status_reg;      /**< Status / command port. */
    uint32_t irq_num;         /**< PIC IRQ line for this channel. */
    int lba_mode;             /**< 1 = LBA48 supported, else LBA28. */
    uint32_t total_sectors_28; /**< LBA28 capacity. */
    uint64_t total_sectors_48; /**< LBA48 capacity. */
} drive_t;

/** The four possible drives plus a pointer to the one currently selected. */
typedef struct ata_drives {
    drive_t cur_hdd;          /**< Currently selected drive (copy). */
    drive_t primary_master;
    drive_t primary_slave;
    drive_t secondary_master;
    drive_t secondary_slave;
} ata_drives_t;

/** @brief Probe both channels and register any FAT-formatted drive with the VFS. */
void ata_init();
/** @brief Block until the ATA IRQ fires (currently unused; reads poll). */
void ata_wait_for_irq();
/** @brief Fill @p drive's register-port fields, then call @ref identify. */
void ata_info_fill(drive_t *drive, int type, uint32_t data, uint32_t err, uint32_t sect, uint32_t lba_low, uint32_t lba_mid, uint32_t lba_high, uint32_t sel, uint32_t status, uint32_t irq);
/** @brief Run IDENTIFY DEVICE and populate presence/capacity fields. */
void identify(drive_t *drive);
/** @brief ~400 ns delay by reading the status port four times. */
void delay_400ns();
/**
 * @brief Read one 512-byte sector by LBA.
 * @param lba Logical block address.
 * @return Pointer to a shared static buffer; consume it before the next read.
 */
char *ata_read_sector(int lba);

/**
 * @brief Write the shared buffer (see @ref ata_read_sector) to one 512-byte
 *        sector by LBA, then flush the drive's write cache.
 * @param lba Logical block address.
 * @return 1 on success.
 */
int ata_write_sector(int lba);
