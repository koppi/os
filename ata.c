/**
 * @file ata.c
 * @brief PIO-mode ATA/IDE disk driver: IDENTIFY-based drive detection and
 *        polled 512-byte sector reads, registered with the VFS as "hd{a,b,..}".
 */
#include <ata.h>

#include <device.h>
#include <lib/string.h>
#include <fat.h>
#include <idt.h>
#include <io.h>
#include <log.h>

static ata_drives_t ata_info;
uint8_t ata_irq_done = 0;

static device_t dev_info[4];

/** Shared scratch sector: @ref ata_read_sector fills it and returns it,
 *  @ref ata_write_sector sends it back out -- the same read-modify-write
 *  convention @c floppy_read_sector / @c floppy_write_sector use over
 *  @c dma_buffer (see fat_touch()). */
static uint8_t ata_buf[512];

extern void ata_int();

void ata_init() {
    install_ir(46, 0x80 | 0x0E, 0x8, &ata_int);
    install_ir(47, 0x80 | 0x0E, 0x8, &ata_int);
    ata_info_fill(&ata_info.primary_master, 1, ATA_PRIMARY_DATA, ATA_PRIMARY_ERR, ATA_PRIMARY_SECTORS, ATA_PRIMARY_LBA_LOW, ATA_PRIMARY_LBA_MID, ATA_PRIMARY_LBA_HIGH, ATA_PRIMARY_DRIVE_SEL, ATA_PRIMARY_STATUS, ATA_PRIMARY_IRQ);
    ata_info_fill(&ata_info.primary_slave, 0, ATA_PRIMARY_DATA, ATA_PRIMARY_ERR, ATA_PRIMARY_SECTORS, ATA_PRIMARY_LBA_LOW, ATA_PRIMARY_LBA_MID, ATA_PRIMARY_LBA_HIGH, ATA_PRIMARY_DRIVE_SEL, ATA_PRIMARY_STATUS, ATA_PRIMARY_IRQ);
    ata_info_fill(&ata_info.secondary_master, 1, ATA_SECONDARY_DATA, ATA_SECONDARY_ERR, ATA_SECONDARY_SECTORS, ATA_SECONDARY_LBA_LOW, ATA_SECONDARY_LBA_MID, ATA_SECONDARY_LBA_HIGH, ATA_SECONDARY_DRIVE_SEL, ATA_SECONDARY_STATUS, ATA_SECONDARY_IRQ);
    ata_info_fill(&ata_info.secondary_slave, 0, ATA_SECONDARY_DATA, ATA_SECONDARY_ERR, ATA_SECONDARY_SECTORS, ATA_SECONDARY_LBA_LOW, ATA_SECONDARY_LBA_MID, ATA_SECONDARY_LBA_HIGH, ATA_SECONDARY_DRIVE_SEL, ATA_SECONDARY_STATUS, ATA_SECONDARY_IRQ);
    ata_info.cur_hdd = ata_info.primary_master;
    drive_t *temp_info = &ata_info.primary_master;
    static const char *chan_name[4] = {
        "primary master", "primary slave", "secondary master", "secondary slave"
    };
    /* The RAM disk (initrd.c) claims device id 0 before this runs, so probe for
     * the next free slot rather than assuming channel index == id. Mount names
     * still run hda, hdb, ... in channel order. */
    int hd_letter = 0;
    for(int i = 0; i < 4; i++) {
        if(temp_info->present == 1) {
            int id = 0;
            while(id < 8 && get_dev_by_id(id))
                id++;
            if(id >= 8) {
                temp_info++;
                continue;   /* device table full */
            }
            dev_info[i].id = id;
            dev_info[i].type = 1;
            strcpy(dev_info[i].mount, "hd");
            dev_info[i].mount[2] = 'a' + hd_letter++;
            dev_info[i].mount[3] = 0;
            dev_info[i].read = &ata_read_sector;
            dev_info[i].write = &ata_write_sector;
            fat_init(&dev_info[i].fs);
            device_register(&dev_info[i]);
            klogf(LOG_INFO, "ATA %s: mounted as %s\n", chan_name[i], dev_info[i].mount);
        }
        temp_info++;
    }
}

void ata_wait_for_irq() {
    while(ata_irq_done == 0);
    ata_irq_done = 0;
}

void ata_info_fill(drive_t *drive, int type, uint32_t data, uint32_t err,
                   uint32_t sect, uint32_t lba_low, uint32_t lba_mid,
                   uint32_t lba_high, uint32_t sel, uint32_t status, uint32_t irq) {
    drive->type = type;
    drive->data_reg = data;
    drive->err_reg = err;
    drive->sectors_reg = sect;
    drive->lba_low_reg = lba_low;
    drive->lba_mid_reg = lba_mid;
    drive->lba_high_reg = lba_high;
    drive->sel_reg = sel;
    drive->status_reg = status;
    drive->irq_num = irq;
    identify(drive);
}

/* Bound on the IDENTIFY status polls. Large enough that a real, slow drive
 * still completes; small enough that a wedged or absent channel (common on
 * real hardware and when booting os.iso with no disk attached) fails the probe
 * in a few milliseconds instead of hanging the boot forever. */
#define ATA_POLL_MAX 500000

void identify(drive_t *drive) {
    if(drive->type == 1)
        outportb(drive->sel_reg, 0xA0);
    else
        outportb(drive->sel_reg, 0xB0);
    delay_400ns();

    /* All-ones on the status port means the channel is floating: no drive, and
     * usually no controller either. Bail before issuing any command. */
    if(inportb(drive->status_reg) == 0xFF) {
        drive->present = 0;
        return;
    }

    //outportb(drive->sectors_reg, 0); // koppi
    outportb(drive->lba_low_reg, 0);
    outportb(drive->lba_mid_reg, 0);
    outportb(drive->lba_high_reg, 0);
    outportb(drive->status_reg, ATA_IDENTIFY);
    if(inportb(drive->status_reg) == 0) {
        drive->present = 0;
    } else {
        /* Wait out BSY, then require a clean LBA mid/high (0 => plain ATA, not
         * ATAPI/SATA) and DRQ. Every wait is bounded so an unresponsive
         * channel marks the drive absent instead of spinning forever. */
        int timeout = ATA_POLL_MAX;
        while((inportb(drive->status_reg) & 0x80) != 0) {
            if(--timeout <= 0) { drive->present = 0; return; }
        }
        if((inportb(drive->lba_mid_reg) != 0) || (inportb(drive->lba_high_reg) != 0)) {
            drive->present = 0;
        } else {
            timeout = ATA_POLL_MAX;
            uint8_t st;
            while(!((st = inportb(drive->status_reg)) & 0x08)) {
                if(st & 0x01) { drive->present = 0; return; }   /* ERR */
                if(--timeout <= 0) { drive->present = 0; return; }
            }
            {
                drive->present = 1;
                for(int i = 0; i < 256; i++) {
                    switch(i) {
                        case 83:
                            if(inportw(drive->data_reg) & 0x400)
                                drive->lba_mode = 1;
                            else
                                drive->lba_mode = 0;
                            break;
                        case 60:
                            drive->total_sectors_28 = inportw(drive->data_reg);
                            drive->total_sectors_28 <<= 16;
                            break;
                        case 61:
                            drive->total_sectors_28 |= inportw(drive->data_reg);
                            break;
                        case 100:
                            drive->total_sectors_48 = inportw(drive->data_reg);
                            drive->total_sectors_48 <<= 16;
                            break;
                        case 101:
                        case 102:
                            drive->total_sectors_48 |= inportw(drive->data_reg);
                            drive->total_sectors_48 <<= 16;
                            break;
                        case 103:
                            drive->total_sectors_48 |= inportw(drive->data_reg);
                            break;
                        default:
                            inportw(drive->data_reg);
                            break;
                    }
                }
            }
        }
    }
}

void delay_400ns() {
    for(int i = 0; i < 4; i++)
        inportb(ata_info.cur_hdd.status_reg);
}

char *ata_read_sector(int lba) {
    // Shared scratch sector, like floppy_read_sector(): the caller must consume
    // the data before the next read. Returning a fresh kmalloc() here leaked
    // 512 bytes of kernel heap on every sector read.
    outportb(ata_info.cur_hdd.sel_reg, 0xE0 | ((lba >> 24) & 0x0F)); // maybe or with (ata_info.cur_hdd.type << 4)
    outportb(ata_info.cur_hdd.err_reg, 0x00);
    outportb(ata_info.cur_hdd.sectors_reg, (uint8_t) 1);
    outportb(ata_info.cur_hdd.lba_low_reg, lba & 0x000000ff);
    outportb(ata_info.cur_hdd.lba_mid_reg, (lba & 0x0000ff00) >> 8);
    outportb(ata_info.cur_hdd.lba_high_reg, (lba & 0x00ff0000) >> 16);
    outportb(ata_info.cur_hdd.status_reg, 0x20);
    //ata_wait_for_irq();
    delay_400ns();

    // Wait for BSY to clear and DRQ to assert on the *status* register.
    // (Polling the data register here would consume bytes from the sector.)
    uint8_t st;
    do {
        st = inportb(ata_info.cur_hdd.status_reg);
    } while((st & 0x80) || !(st & 0x08));

    for(int i = 0; i < 256; i++) {
        ((uint16_t *) ata_buf)[i] = inportw(ata_info.cur_hdd.data_reg);
    }
    delay_400ns();
    return (char *) ata_buf;
}

int ata_write_sector(int lba) {
    outportb(ata_info.cur_hdd.sel_reg, 0xE0 | ((lba >> 24) & 0x0F));
    outportb(ata_info.cur_hdd.err_reg, 0x00);
    outportb(ata_info.cur_hdd.sectors_reg, (uint8_t) 1);
    outportb(ata_info.cur_hdd.lba_low_reg, lba & 0x000000ff);
    outportb(ata_info.cur_hdd.lba_mid_reg, (lba & 0x0000ff00) >> 8);
    outportb(ata_info.cur_hdd.lba_high_reg, (lba & 0x00ff0000) >> 16);
    outportb(ata_info.cur_hdd.status_reg, 0x30); // WRITE SECTORS
    delay_400ns();

    uint8_t st;
    do {
        st = inportb(ata_info.cur_hdd.status_reg);
    } while((st & 0x80) || !(st & 0x08));

    for(int i = 0; i < 256; i++) {
        outportw(ata_info.cur_hdd.data_reg, ((uint16_t *) ata_buf)[i]);
    }
    delay_400ns();

    do {
        st = inportb(ata_info.cur_hdd.status_reg);
    } while(st & 0x80);

    // FLUSH CACHE, so the write actually reaches the (virtual) platter
    // before a caller assumes it's durable.
    outportb(ata_info.cur_hdd.status_reg, 0xE7);
    delay_400ns();
    do {
        st = inportb(ata_info.cur_hdd.status_reg);
    } while(st & 0x80);

    return 1;
}

