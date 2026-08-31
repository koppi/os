/**
 * @file floppy.h
 * @brief 82077AA floppy-disk controller driver: FDC commands, register bits and
 *        the LBA sector read/write entry points used by the FAT layer.
 */
#pragma once

#include <types.h>

/** FDC I/O ports. */
enum floppy_io {
    FLOPPY_DOR     = 0x3F2,
    FLOPPY_MSR     = 0x3F4,
    FLOPPY_FIFO    = 0x3F5,
    FLOPPY_CTRL    = 0x3F7
};

enum floppy_cmd {
    FLOPPY_CMD_READ_TRACK = 2,
    FLOPPY_CMD_SPECIFY = 3,
    FLOPPY_CMD_CHECK_STAT = 4,
    FLOPPY_CMD_WRITE_SECT = 5,
    FLOPPY_CMD_READ_SECT = 6,
    FLOPPY_CMD_CALIBRATE = 7,
    FLOPPY_CMD_CHECK_INT = 8,
    FLOPPY_CMD_WRITE_DEL_S = 9,
    FLOPPY_CMD_READ_ID_S = 0xA,
    FLOPPY_CMD_READ_DEL_S = 0xC,
    FLOPPY_CMD_FORMAT_TRACK = 0xD,
    FLOPPY_CMD_SEEK = 0xF
};

enum floppy_cmd_ext {
    FLOPPY_CMD_EXT_SKIP = 0x20,
    FLOPPY_CMD_EXT_DENSITY = 0x40,
    FLOPPY_CMD_EXT_MULTITRACK = 0x80
};

enum floppy_gap3_length {
    FLOPPY_GAP3_LENGTH_STD = 42,
    FLOPPY_GAP3_LENGTH_5_14 = 32,
    FLOPPY_GAP3_LENGTH_3_5 = 27
};

enum floppy_sector_dtl {
    FLOPPY_SECTOR_DTL_128 = 0,
    FLOPPY_SECTOR_DTL_256 = 1,
    FLOPPY_SECTOR_DTL_512 = 2,
    FLOPPY_SECTOR_DTL_1024 = 4
};

#define FLOPPY_SECTORS_PER_TRACK 18

// DOR Reg
#define FLOPPY_DOR_MASK_DRIVE0          0
#define FLOPPY_DOR_MASK_DRIVE1          1
#define FLOPPY_DOR_MASK_DRIVE2          2
#define FLOPPY_DOR_MASK_DRIVE3          3
#define FLOPPY_DOR_MASK_RESET           4
#define FLOPPY_DOR_MASK_DMA             8
#define FLOPPY_DOR_MASK_DRIVE0_MOTOR    16
#define FLOPPY_DOR_MASK_DRIVE1_MOTOR    32
#define FLOPPY_DOR_MASK_DRIVE2_MOTOR    64
#define FLOPPY_DOR_MASK_DRIVE3_MOTOR    128

// MSR Reg
#define FLOPPY_MSR_MASK_DRIVE0_POS_MODE     1
#define FLOPPY_MSR_MASK_DRIVE1_POS_MODE     2
#define FLOPPY_MSR_MASK_DRIVE2_POS_MODE     4
#define FLOPPY_MSR_MASK_DRIVE3_POS_MODE     8
#define FLOPPY_MSR_MASK_BUSY                16
#define FLOPPY_MSR_MASK_DMA                 32
#define FLOPPY_MSR_MASK_DATAIO              64
#define FLOPPY_MSR_MASK_DATAREG             128

/** @brief Install the FDC IRQ, detect drives, reset and configure the FDC. */
void floppy_init();
/** @brief Block until the floppy IRQ fires. */
void floppy_wait_irq();
/** @brief Program the ISA DMA channel for the next floppy transfer. */
void floppy_dma_init();
/** @brief Write the Digital Output Register. */
void floppy_write_dor(uint8_t val);
/** @brief Read the Main Status Register. */
uint8_t floppy_read_status();
/** @brief Push one byte to the FDC FIFO (command / parameter). */
void floppy_send_cmd(uint8_t cmd);
/** @brief Read one result byte from the FDC FIFO. */
uint8_t floppy_read_data();
/** @brief Write the Configuration Control Register (data rate). */
void floppy_write_ccr(uint8_t val);
/** @brief Low-level "read sector" by CHS into the DMA buffer. */
void floppy_read_sector_imp(uint8_t head, uint8_t track, uint8_t sector);
/**
 * @brief Read one 512-byte sector by LBA.
 * @return Pointer to the shared DMA buffer; consume before the next call.
 */
char *floppy_read_sector(int lba);
/** @brief Low-level "write sector" by CHS from the DMA buffer. */
int floppy_write_sector_imp(uint8_t head, uint8_t track, uint8_t sector);
/** @brief Write one 512-byte sector by LBA. */
int floppy_write_sector(int lba);
/** @brief Issue SPECIFY with the given step/load/unload timings. */
void floppy_drive_data(uint32_t stepr, uint32_t loadt, uint32_t unloadt, int dma);
/** @brief Recalibrate (seek to cylinder 0) drive @p drive. */
int floppy_calibrate(uint32_t drive);
/** @brief Issue SENSE INTERRUPT and return st0 / cylinder. */
void floppy_check_int(uint32_t * st0, uint32_t *cyl);
/** @brief Seek to (@p cyl, @p head). */
int floppy_seek(uint32_t cyl, uint32_t head);
/** @brief Disable the FDC (clear DOR). */
void floppy_disable();
/** @brief Enable the FDC (set DOR reset/DMA bits). */
void floppy_enable();
/** @brief Full controller reset and reconfigure. */
void floppy_reset();
/** @brief Turn the current drive's spindle motor on/off. */
void floppy_control_motor(int on);
/** @brief Convert an LBA to head/track/sector for a 1.44M floppy. */
void floppy_lba_to_chs(int lba, int *head, int *track, int *sector);
/** @brief Read the CMOS drive types and register 1.44M drives with the VFS. */
int floppy_detect_drives();
/** @brief Select which physical drive subsequent operations act on. */
void floppy_set_cur_drive(int drive);
