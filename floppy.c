/**
 * @file floppy.c
 * @brief 82077AA floppy-controller driver: FDC command sequencing, ISA-DMA
 *        sector transfers, motor/seek control and LBA↔CHS conversion for
 *        1.44 MB media. Registered with the VFS as "fd{a,b}".
 */
#include <floppy.h>
#include <device.h>
#include <lib/string.h>
#include <fat.h>
#include <idt.h>
#include <dma.h>
#include <io.h>
#include <printf.h>

#define FLOPPY_DMA_LEN 0x4800
#define FLOPPY_DMA_CHANNEL 2

uint8_t floppy_irq_done = 0;
int cur_drive = 0;
static device_t dev_info[4];

static const char floppy_dmabuf[FLOPPY_DMA_LEN] __attribute__((aligned(0x8000)));
uint32_t *dma_buffer = (uint32_t *) &floppy_dmabuf;

static char *drive_types[8] = {
    "none",
    "360kB 5.25",
    "1.2MB 5.25",
    "720kB 3.5",

    "1.44MB 3.5",
    "2.88MB 3.5",
    "unknown type",
    "unknown type"
};

static const char *status[] = {
    0,
    "error",
    "invalid",
    "drive"
};

extern void floppy_int();

/**
 * @brief Install the IRQ 6 handler and bring up any 1.44 MB drives found.
 *
 * Resets the controller and programs the drive timings only when a drive was
 * actually detected, so a machine with no floppy hardware costs one CMOS read.
 */
void floppy_init() {
    install_ir(38, 0x80 | 0x0E, 0x8, &floppy_int);
    int ndrives = floppy_detect_drives();
    if(ndrives > 0) {
        floppy_reset();
        floppy_drive_data(13, 1, 0xF, 1);
    }
}

/**
 * @brief Busy-wait for the controller's interrupt, then clear the flag.
 *
 * The IRQ stub in floppy_asm.S only bumps @c floppy_irq_done; every command
 * that completes with an interrupt waits here. Spins rather than sleeps, so
 * callers must already hold the filesystem lock.
 */
void floppy_wait_irq() {
    while(floppy_irq_done == 0);
    floppy_irq_done = 0;
}

/**
 * @brief Point ISA DMA channel 2 at the transfer buffer.
 *
 * Silently does nothing if the buffer is unusable for ISA DMA: above the 16 MB
 * the 8237 can address, longer than a 16-bit count, or straddling a 64 KB
 * page-register boundary. @c floppy_dmabuf is over-aligned to keep it clear of
 * that last case.
 */
void floppy_dma_init() {
    union {
        uint8_t byte[4];
        uint32_t l;
    } a, c;
    
    a.l = (uint32_t) &floppy_dmabuf;
    c.l = FLOPPY_DMA_LEN - 1;
    
    if((a.l >> 24) || (c.l >> 16) || (((a.l & 0xFFFF) + c.l) >> 16)) {
        return;
    }
    dma_reset();
    dma_mask_channel(FLOPPY_DMA_CHANNEL);
    dma_reset_flipflop(0);
    dma_set_address(FLOPPY_DMA_CHANNEL, a.byte[0], a.byte[1]);
    dma_set_external_page_register(FLOPPY_DMA_CHANNEL, a.byte[2]);
    dma_reset_flipflop(0);
    dma_set_count(FLOPPY_DMA_CHANNEL, c.byte[0], c.byte[1]);
    dma_set_read(FLOPPY_DMA_CHANNEL);
    dma_unmask_all();
}

/** @brief Write the Digital Output Register (drive select, motors, reset, DMA). */
void floppy_write_dor(uint8_t val) {
    outportb(FLOPPY_DOR, val);
}

/** @brief Read the Main Status Register. @return The raw MSR byte. */
uint8_t floppy_read_status() {
    return inportb(FLOPPY_MSR);
}

/**
 * @brief Push one command or parameter byte into the FIFO.
 * @param cmd Byte to send.
 *
 * Polls the MSR for up to 500 tries waiting for the data register to be ready,
 * and gives up silently if it never is.
 */
void floppy_send_cmd(uint8_t cmd) {
    int i;
    for(i = 0; i < 500; i++)
        if(floppy_read_status() & FLOPPY_MSR_MASK_DATAREG) {
            outportb(FLOPPY_FIFO, cmd);
            return;
        }
}

/**
 * @brief Read one result byte from the FIFO.
 * @return The byte, or 0 if the data register never became ready — which a
 *         real 0 result byte is indistinguishable from.
 */
uint8_t floppy_read_data() {
    int i;
    for(i = 0; i < 500; i++)
        if(floppy_read_status() & FLOPPY_MSR_MASK_DATAREG)
            return inportb(FLOPPY_FIFO);
    return NULL;
}

/** @brief Write the Configuration Control Register (data rate). */
void floppy_write_ccr(uint8_t val) {
    outportb(FLOPPY_CTRL, val);
}

/**
 * @brief Issue one READ command for an already-sought CHS address.
 * @param head   Head number.
 * @param track  Cylinder.
 * @param sector Sector, 1-based.
 *
 * Assumes the motor is running and the heads are on @p track. Drains the seven
 * result bytes and reports a not-ready drive, but the data itself lands in the
 * DMA buffer rather than being returned.
 */
void floppy_read_sector_imp(uint8_t head, uint8_t track, uint8_t sector) {
    uint32_t st0, cyl;
    
    floppy_dma_init();
    dma_set_read(FLOPPY_DMA_CHANNEL);
    floppy_send_cmd(FLOPPY_CMD_READ_SECT | FLOPPY_CMD_EXT_MULTITRACK | FLOPPY_CMD_EXT_SKIP | FLOPPY_CMD_EXT_DENSITY);
    floppy_send_cmd((head << 2) | cur_drive);
    floppy_send_cmd(track);
    floppy_send_cmd(head);
    floppy_send_cmd(sector);
    floppy_send_cmd(FLOPPY_SECTOR_DTL_512);
    floppy_send_cmd(((sector + 1) >= FLOPPY_SECTORS_PER_TRACK) ? FLOPPY_SECTORS_PER_TRACK : sector + 1);
    floppy_send_cmd(FLOPPY_GAP3_LENGTH_3_5);
    floppy_send_cmd(0xFF);
    
    floppy_wait_irq();
    for(int i = 0; i < 7; i++)
        floppy_read_data();
    floppy_check_int(&st0, &cyl);
    
    if(st0 & 0x08) {
        printf("floppy_read_sector: drive not ready\n");
    }
}

/**
 * @brief Read one 512-byte sector. Registered as the device read hook.
 * @param lba Linear sector number.
 * @return The DMA buffer holding the sector, 0 if the seek failed, or
 *         (char *) -1 if no drive is selected.
 *
 * The buffer is shared and overwritten by the next transfer, so the caller has
 * to consume it before yielding.
 */
char *floppy_read_sector(int lba) {
    if(cur_drive > 3)
        return (char *) -1;
    int head = 0, track = 0, sector = 1;
    floppy_lba_to_chs(lba, &head, &track, &sector);
    floppy_control_motor(1);
    if(floppy_seek((uint8_t) track, (uint8_t) head) != 0) {
        floppy_control_motor(0);
        return 0;
    }
    floppy_read_sector_imp((uint8_t) head, (uint8_t) track, (uint8_t) sector);
    floppy_control_motor(0);
    return (char *) &floppy_dmabuf;
}

/**
 * @brief Issue one WRITE command for an already-sought CHS address.
 * @param head   Head number.
 * @param track  Cylinder.
 * @param sector Sector, 1-based.
 * @return Non-zero on success.
 *
 * Writes whatever is already in the DMA buffer; the caller fills it first.
 */
int floppy_write_sector_imp(uint8_t head, uint8_t track, uint8_t sector) {
    uint32_t st0, cyl;
    
    floppy_dma_init();
    dma_set_write(FLOPPY_DMA_CHANNEL);
    floppy_send_cmd(FLOPPY_CMD_WRITE_SECT | FLOPPY_CMD_EXT_MULTITRACK | FLOPPY_CMD_EXT_SKIP | FLOPPY_CMD_EXT_DENSITY);
    floppy_send_cmd((head << 2) | cur_drive);
    floppy_send_cmd(track);
    floppy_send_cmd(head);
    floppy_send_cmd(sector);
    floppy_send_cmd(FLOPPY_SECTOR_DTL_512);
    floppy_send_cmd(((sector + 1) >= FLOPPY_SECTORS_PER_TRACK) ? FLOPPY_SECTORS_PER_TRACK : sector + 1);
    floppy_send_cmd(FLOPPY_GAP3_LENGTH_3_5);
    floppy_send_cmd(0xFF);
    
    floppy_wait_irq();
    for(int i = 0; i < 7; i++)
        floppy_read_data();
    floppy_check_int(&st0, &cyl);
    
    if(st0 & 0xC0) {
        printf("floppy_write_sector: status = %s\n", status[st0 >> 6]);
        return 0;
    }
    if(st0 & 0x08) {
        printf("floppy_write_sector: drive not ready\n");
        return 0;
    }
    return 1;
}

/**
 * @brief Write one 512-byte sector. Registered as the device write hook.
 * @param lba Linear sector number.
 * @return 1 on success, -1 on a failed seek, write, or no selected drive.
 */
int floppy_write_sector(int lba) {
    if(cur_drive > 3)
        return -1;
    int head = 0, track = 0, sector = 1;
    floppy_lba_to_chs(lba, &head, &track, &sector);
    floppy_control_motor(1);
    if(floppy_seek((uint8_t) track, (uint8_t) head) != 0) {
        floppy_control_motor(0);
        return -1;
    }
    if(!floppy_write_sector_imp((uint8_t) head, (uint8_t) track, (uint8_t) sector)) {
        floppy_control_motor(0);
        return -1;
    }
    floppy_control_motor(0);
    return 1;
}

/**
 * @brief SPECIFY: program the drive's mechanical timings.
 * @param stepr   Step rate.
 * @param loadt   Head load time.
 * @param unloadt Head unload time.
 * @param dma     Non-zero to transfer over DMA rather than PIO.
 */
void floppy_drive_data(uint32_t stepr, uint32_t loadt, uint32_t unloadt, int dma) {
    if(cur_drive > 3)
        return;
    uint32_t data = 0;
    floppy_send_cmd(FLOPPY_CMD_SPECIFY);
    data = ((stepr & 0xF) << 4) | (unloadt & 0xF);
    floppy_send_cmd(data);
    data = (loadt << 1) | ((dma == 1) ? 1 : 0);
    floppy_send_cmd(data);
}

/**
 * @brief RECALIBRATE: drive the heads back to cylinder 0.
 * @param drive Drive number, 0-3.
 * @return 0 once the heads report cylinder 0, -1 after ten failed attempts.
 */
int floppy_calibrate(uint32_t drive) {
    uint32_t st0, cyl;
    
    if(drive > 3)
        return -1;

    floppy_control_motor(1);
    
    for(int i = 0; i < 10; i++) {
        floppy_send_cmd(FLOPPY_CMD_CALIBRATE);
        floppy_send_cmd(drive);
        floppy_wait_irq();
        floppy_check_int(&st0, &cyl);
        
        if(st0 & 0xC0) {
            printf("floppy_calibrate: status = %s\n", status[st0 >> 6]);
            continue;
        }
        
        if(!cyl) {
            floppy_control_motor(0);
            return 0;
        }
    }
    
    floppy_control_motor(0);
    printf("Error calibrating floppy\n");
    return -1;
}

/**
 * @brief SENSE INTERRUPT STATUS: collect the result of the last command.
 * @param st0 Receives status register 0.
 * @param cyl Receives the cylinder the heads ended on.
 *
 * Must follow every interrupt-completing command, or the controller will not
 * accept the next one.
 */
void floppy_check_int(uint32_t *st0, uint32_t *cyl) {
    floppy_send_cmd(FLOPPY_CMD_CHECK_INT);
    *st0 = floppy_read_data();
    *cyl = floppy_read_data();
}

/**
 * @brief Move the heads to a cylinder.
 * @param cyl  Target cylinder.
 * @param head Head to seek with.
 * @return 0 once the controller confirms the cylinder, -1 after ten attempts.
 *
 * Leaves the motor off on return, so a caller doing a transfer has to turn it
 * back on before the read or write.
 */
int floppy_seek(uint32_t cyl, uint32_t head) {
    uint32_t st0, cyl0 = -1;
    
    if(cur_drive > 3)
        return -1;
    
    floppy_control_motor(1);
    
    for(int i = 0; i < 10; i++) {
        floppy_send_cmd(FLOPPY_CMD_SEEK);
        floppy_send_cmd((head) << 2 | cur_drive);
        floppy_send_cmd(cyl);
        
        floppy_wait_irq();
        floppy_check_int(&st0, &cyl0);
        
        if(st0 & 0xC0) {
            // printf("floppy_seek: status = %s\n", status[st0 >> 6]);
            continue;
        }
        
        if(cyl0 == cyl) {
            floppy_control_motor(0);
            return 0;
        }
    }
    floppy_control_motor(0);
    printf("Floppy seek failed\n");
    return -1;
}

/** @brief Drop the DOR entirely, which also cuts the motors and asserts reset. */
void floppy_disable() {
    floppy_write_dor(0);
}

/** @brief Release reset and enable DMA, with every motor still off. */
void floppy_enable() {
    floppy_write_dor(FLOPPY_DOR_MASK_RESET | FLOPPY_DOR_MASK_DMA);
}

/**
 * @brief Reset the controller and return the current drive to a known state.
 *
 * Cycles the DOR, drains the four sense-interrupt results the reset generates,
 * selects the data rate, programs conservative timings and recalibrates.
 */
void floppy_reset() {
    if(cur_drive > 3)
        return;
    uint32_t st0, cyl;
    floppy_disable();
    floppy_enable();
    floppy_wait_irq();
    for(int i = 0; i < 4; i++)
        floppy_check_int(&st0, &cyl);
    
    floppy_write_ccr(0);
    floppy_drive_data(3, 16, 240, 1);
    floppy_calibrate(cur_drive);
}

/**
 * @brief Spin the selected drive's motor up or down.
 * @param on Non-zero to start the motor.
 *
 * Turning it off writes a DOR value that keeps reset released and DMA enabled
 * with all motors stopped; the constant it is spelled with happens to be a
 * command opcode of the same value, not a command being issued.
 */
void floppy_control_motor(int on) {
    if(cur_drive > 3)
        return;
    
    uint32_t motor = 0;
    
    switch(cur_drive) {
        case 0:
            motor = FLOPPY_DOR_MASK_DRIVE0_MOTOR;
            break;
        case 1:
            motor = FLOPPY_DOR_MASK_DRIVE1_MOTOR;
            break;
        case 2:
            motor = FLOPPY_DOR_MASK_DRIVE2_MOTOR;
            break;
        case 3:
            motor = FLOPPY_DOR_MASK_DRIVE3_MOTOR;
            break;
    }
    
    if(on)
        floppy_write_dor(cur_drive | motor | FLOPPY_DOR_MASK_RESET | FLOPPY_DOR_MASK_DMA);
    else
        floppy_write_dor(FLOPPY_CMD_READ_DEL_S);
}

/**
 * @brief Convert a linear sector number to CHS for 1.44 MB geometry.
 * @param lba    Linear sector number.
 * @param head   Receives the head, 0 or 1.
 * @param track  Receives the cylinder.
 * @param sector Receives the sector, 1-based as the controller expects.
 *
 * Fixed to two heads and @ref FLOPPY_SECTORS_PER_TRACK sectors per track.
 */
void floppy_lba_to_chs(int lba, int *head, int *track, int *sector) {
    *head = (lba % (FLOPPY_SECTORS_PER_TRACK * 2)) / FLOPPY_SECTORS_PER_TRACK;
    *track = lba / (FLOPPY_SECTORS_PER_TRACK * 2);
    *sector = (lba % FLOPPY_SECTORS_PER_TRACK) + 1;
}

/**
 * @brief Read the CMOS drive-type byte and register the drives found.
 * @return Number of drives registered.
 *
 * Only 1.44 MB 3.5" drives are taken; anything else is left alone rather than
 * driven with the wrong geometry. Each match is registered with the device
 * layer as "fda"/"fdb" with a FAT filesystem attached, and selects itself as
 * the current drive as a side effect.
 */
int floppy_detect_drives() {
    outportb(0x70, 0x10);
    sleep(100);
    uint8_t drives = inportb(0x71);
    //printf("drives: %d\n", drives);
    
    int ndrives = 0;
    
    if(strcmp(drive_types[drives >> 4], "1.44MB 3.5") == 0) {
        dev_info[0].id = 4;
        dev_info[0].type = 0;
        strcpy(dev_info[0].mount, "fd");
        dev_info[0].mount[2] = 'a';
        dev_info[0].mount[3] = 0;
        dev_info[0].read = &floppy_read_sector;
        dev_info[0].write = &floppy_write_sector;
        fat_init(&dev_info[0].fs);
        device_register(&dev_info[0]);
        cur_drive = 0;
        ndrives++;
    }
    if(strcmp(drive_types[drives & 0xF], "1.44MB 3.5") == 0) {
        dev_info[1].id = 5;
        dev_info[1].type = 0;
        strcpy(dev_info[1].mount, "fd");
        dev_info[1].mount[2] = 'b';
        dev_info[1].mount[3] = 0;
        dev_info[1].read = &floppy_read_sector;
        dev_info[1].write = &floppy_write_sector;
        fat_init(&dev_info[1].fs);
        device_register(&dev_info[1]);
        cur_drive = 1;
        ndrives++;
    }

    return ndrives;
}

/**
 * @brief Choose which drive subsequent commands address.
 * @param drive Drive number, 0-3; out-of-range values are ignored.
 */
void floppy_set_cur_drive(int drive) {
    if((drive < 0) || (drive > 3))
        return;
    cur_drive = drive;
}

