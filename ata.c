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
    for(int i = 0; i < 4; i++) {
        if(temp_info->present == 1) {
            dev_info[i].id = i;
            dev_info[i].type = 1;
            strcpy(dev_info[i].mount, "hd");
            dev_info[i].mount[2] = i + 'a';
            dev_info[i].mount[3] = 0;
            dev_info[i].read = &ata_read_sector;
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

void identify(drive_t *drive) {
    if(drive->type == 1)
        outportb(drive->sel_reg, 0xA0);
    else
        outportb(drive->sel_reg, 0xB0);
    //outportb(drive->sectors_reg, 0); // koppi
    outportb(drive->lba_low_reg, 0);
    outportb(drive->lba_mid_reg, 0);
    outportb(drive->lba_high_reg, 0);
    outportb(drive->status_reg, ATA_IDENTIFY);
    if(inportb(drive->status_reg) == 0) {
        drive->present = 0;
    } else {
        if((inportb(drive->lba_mid_reg) != 0) || (inportb(drive->lba_high_reg) != 0)) {
            drive->present = 0;
        } else {
            while((inportb(drive->status_reg) & 0x80) != 0);
            while(((inportb(drive->status_reg) & 0x8) != 8) && ((inportb(drive->status_reg) & 0x0) == 0));
            if((inportb(drive->status_reg) & 0x0) == 1) {
                drive->present = 0;
            } else {
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
    static uint8_t buf[512];
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
        ((uint16_t *) buf)[i] = inportw(ata_info.cur_hdd.data_reg);
    }
    delay_400ns();
    return (char *) buf;
}

