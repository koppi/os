/**
 * @file fat.c
 * @brief FAT12/16 filesystem driver.
 *
 * Mounts a volume by parsing its BPB into @c device_t::minfo, then implements
 * the VFS operation vector (open, read, ls, cd, touch, delete) over the raw
 * sector reads provided by the block driver. One sector per cluster is assumed.
 */
#include <assert.h>
#include <lib/string.h>
#include <fat.h>
#include <fat_mount.h>
#include <mbr.h>
#include <kheap.h>
#include <printf.h>

#define SECTOR_SIZE 512

extern uint32_t *dma_buffer;

uint8_t FAT[SECTOR_SIZE * 2];

int offset;

void fat_mount(device_t *dev) {
    // Trying with bootsector
    bootsector_t *bs = (bootsector_t *) dev->read(0);
    if((bs->ignore[0] != 0xEB) || (bs->ignore[2] != 0x90)) // Not a FAT fs
        return;
    
    // Scan for partitions
    mbr_t *mbr = (mbr_t *) bs;
    for(int i = 0; i < 4; i++) {
        if(mbr->partition_table[i].sys_id != FAT32_SYSTEM_ID) {
            continue;
        } else {
            //uint32_t lba = mbr->partition_table[i].lba_start;
            //uint32_t totsec = mbr->partition_table[i].total_sectors;
            //printf("lba: %d sects: %d\n", lba, totsec);
            //bs = (bootsector_t *) dev->read(lba);
            break;
        }
        return;
    }
        
    dev->minfo.n_sectors = (bs->bpb.n_sectors == 0) ? bs->bpb.long_sectors : bs->bpb.n_sectors;
    dev->minfo.fat_offset = bs->bpb.reserved_sectors;
    dev->minfo.fat_size = (bs->bpb.fat_sectors == 0) ? bs->bpb_ext.fat_sectors : bs->bpb.fat_sectors;
    dev->minfo.fat_entry_size = 8;
    dev->minfo.cluster_sectors = bs->bpb.cluster_sectors;
    dev->minfo.n_root_entries = bs->bpb.n_dir_entries;
    // Root directory begins right after the reserved area and the FAT copies.
    dev->minfo.root_offset = dev->minfo.fat_offset + (bs->bpb.n_fats * dev->minfo.fat_size);
    dev->minfo.root_size = ((bs->bpb.n_dir_entries * 32) + (bs->bpb.sector_bytes - 1)) / bs->bpb.sector_bytes;
    dev->minfo.first_data_sector = dev->minfo.root_offset + dev->minfo.root_size;
    dev->minfo.data_sectors = dev->minfo.n_sectors - dev->minfo.first_data_sector;
    dev->minfo.n_fats = bs->bpb.n_fats;
    dev->minfo.sector_bytes = bs->bpb.sector_bytes;
    
    uint32_t total_clusters = dev->minfo.data_sectors / bs->bpb.cluster_sectors;
    if(total_clusters < 4085)
        dev->minfo.type = FAT12;
    else if(total_clusters < 65525)
        dev->minfo.type = FAT16;
    else if(total_clusters < 268435445)
        dev->minfo.type = FAT32;
    else
        dev->minfo.type = EXFAT;
}

void to_dos_file_name(char *name, char *str) {
    if((!name) || (!str))
        return;
    
    /* Caller must provide a buffer of at least NAME_LEN + 1 bytes. */
    memset(str, ' ', NAME_LEN);
    int i;
    for(i = 0; i < strlen(name) && i < NAME_LEN; i++) {
        if((name[i] == '.') || (i == 8))
            break;
        str[i] = toupper(name[i]);
    }
    
    if(name[i] == '.') {
        for(int j = 0; j < 3; j++) {
            i++;
            if(!name[i])
                break;
            str[8 + j] = toupper(name[i]);
        }
    }
    str[NAME_LEN] = 0;
}

void to_normal_file_name(char *name, char *str) {
    int j = 0, flag = 1;
    
    if((!name) || (!str))
        return;
    
    memset(str, ' ', NAME_LEN);
    for(int i = 0; i < strlen(name) && i < NAME_LEN; i++) {
        if(name[i] != ' ') {
            str[j] = tolower(name[i]);
            j++;
        } else if((flag == 1) && (name[9] != ' ')) {
            flag = 0;
            str[j] = '.';
            j++;
        }
    }
    str[j] = 0;
}

/** @brief Debug helper: print one directory entry. */
void print_dir(directory_t *dir) {
    printf("%s %s %d %d %u\n",
           dir->filename, dir->extension,
           dir->attrs, dir->first_cluster,
           dir->file_size);
}

uint32_t get_phys_sector(file *f) {
    device_t *dev = get_dev_by_id(f->dev);
    return dev->minfo.first_data_sector +
           (f->current_cluster - 2) * dev->minfo.cluster_sectors;
}

directory_t *fat_get_dir(file *f) {
    char *dos_file_name = kmalloc(NAME_LEN + 1);
    to_dos_file_name(f->name, dos_file_name);
    device_t *dev = get_dev_by_id(f->dev);

    for(uint32_t i = 0; i < dev->minfo.root_size; i++) {
        directory_t *dir = (directory_t *) dev->read(dev->minfo.root_offset + i);
        for(int j = 0; j < 16; j++, dir++) {
            if(strncmp(dos_file_name, (char *) dir->filename, NAME_LEN) == 0) {
                offset = i;
                kfree(dos_file_name);
                return dir;
            }
        }
    }
    kfree(dos_file_name);
    return NULL;
}

int fat_touch(char *name) {
    file f = fat_search(name);
    if(f.type != FS_NULL) {
        return 0;
    }
    
    char *dos_file_name = kmalloc(NAME_LEN + 1);
    to_dos_file_name(f.name, dos_file_name);
    device_t *dev = get_dev_by_id(f.dev);
    
    for(uint32_t i = 0; i < dev->minfo.root_size; i++) {
        directory_t *dir = (directory_t *) dev->read(dev->minfo.root_offset + i);
        for(int j = 0; j < 16; j++, dir++) {
            if(dir->filename[0] == 0) {
                /* filename[8] and extension[3] are contiguous in the packed
                 * directory entry; copy exactly NAME_LEN bytes and no NUL. */
                memcpy(dir->filename, dos_file_name, NAME_LEN);
                dir->file_size = 0;
                dir->first_cluster = 0;
                dev->write(dev->minfo.root_offset + i);
                kfree(dos_file_name);
                return 1;
            }
        }
    }
    kfree(dos_file_name);
    return 0;
}

void fat_read(file *f, char *buf) {
    if(!f)
        return;
    
    device_t *dev = get_dev_by_id(f->dev);
    unsigned char *sector = (unsigned char *) dev->read(get_phys_sector(f));
    memcpy(buf, sector, SECTOR_SIZE);
    
    uint32_t fat_offset = 0;
    switch(dev->minfo.type) {
    case FAT12:
        fat_offset = f->current_cluster + (f->current_cluster / 2);
        break;
    case FAT16:
        fat_offset = f->current_cluster * 2;
        break;
    case FAT32:
        fat_offset = f->current_cluster * 4;
        break;
    default:
        assert(0);
        break;
    }
    uint32_t fat_sector = dev->minfo.fat_offset + (fat_offset / SECTOR_SIZE);
    uint32_t entry_offset = fat_offset % SECTOR_SIZE;
    sector = (unsigned char *) dev->read(fat_sector);
    memcpy(FAT, sector, SECTOR_SIZE);
    sector = (unsigned char *) dev->read(fat_sector + 1);
    memcpy(FAT + SECTOR_SIZE, sector, SECTOR_SIZE);
    
    if(dev->minfo.type == FAT12) {
        uint16_t next_cluster = *(uint16_t *) &FAT[entry_offset];
        if(f->current_cluster & 0x0001)
            next_cluster >>= 4;
        else
            next_cluster &= 0x0FFF;
    
        if((next_cluster >= 0xFF8) || (next_cluster == 0)) {
            f->eof = 1;
            return;
        }
        f->current_cluster = next_cluster;
    } else if(dev->minfo.type == FAT16) {
        uint16_t next_cluster = *(uint16_t *) &FAT[entry_offset];
        if((next_cluster >= 0xFFF8) || (next_cluster == 0)) {
            f->eof = 1;
            return;
        }
        f->current_cluster = next_cluster;
    } else if(dev->minfo.type == FAT32) {
        uint32_t next_cluster = *(uint32_t *) &FAT[entry_offset] & 0x0FFFFFFF;
        if((next_cluster >= 0x0FFFFFF8) || (next_cluster == 0)) {
            f->eof = 1;
            return;
        }
        f->current_cluster = next_cluster;
    }
}

void fat_write(file *f, char *str) {
    if(!f)
        return;
    /* Legacy one-record write: replace the whole file with a single
     * NUL-terminated string (used by the console "write <file> <text>"). */
    fat_write_all(f, str, strlen(str));
}

/* ------------------------------------------------------------------------- *
 *  FAT16 chain writes: cluster allocation, chain linking and multi-sector
 *  file writes. Enough for a program on the OS to save a multi-KB output
 *  (e.g. the self-hosting C compiler). FAT12 is handled for completeness.
 * ------------------------------------------------------------------------- */

/** End-of-chain marker for the volume's FAT width. */
static uint32_t fat_eoc(device_t *dev) {
    return (dev->minfo.type == FAT12) ? 0x0FF8 : 0xFFF8;
}

/** Highest usable cluster number + 1. */
static uint32_t fat_cluster_count(device_t *dev) {
    return 2 + dev->minfo.data_sectors / dev->minfo.cluster_sectors;
}

/** Read FAT entry @p cl (cluster number) from the first FAT copy. */
static uint32_t fat_get_entry(device_t *dev, uint32_t cl) {
    if(dev->minfo.type == FAT12) {
        uint32_t fo = cl + (cl / 2);
        uint32_t sec = dev->minfo.fat_offset + fo / SECTOR_SIZE;
        uint32_t off = fo % SECTOR_SIZE;
        uint8_t *b = (uint8_t *) dev->read(sec);
        uint16_t lo = b[off];
        uint16_t hi = (off == SECTOR_SIZE - 1)
                          ? ((uint8_t *) dev->read(sec + 1))[0]
                          : b[off + 1];
        uint16_t v = (uint16_t) (lo | (hi << 8));
        return (cl & 1) ? (uint32_t) (v >> 4) : (uint32_t) (v & 0x0FFF);
    }
    uint32_t fo = cl * 2;
    uint32_t sec = dev->minfo.fat_offset + fo / SECTOR_SIZE;
    uint32_t off = fo % SECTOR_SIZE;
    uint8_t *b = (uint8_t *) dev->read(sec);
    return (uint32_t) (b[off] | (b[off + 1] << 8));
}

/** Write FAT entry @p cl = @p val, mirrored to every FAT copy. */
static void fat_set_entry(device_t *dev, uint32_t cl, uint32_t val) {
    for(uint32_t fat = 0; fat < dev->minfo.n_fats; fat++) {
        uint32_t base = dev->minfo.fat_offset + fat * dev->minfo.fat_size;
        if(dev->minfo.type == FAT12) {
            uint32_t fo = cl + (cl / 2);
            uint32_t sec = base + fo / SECTOR_SIZE;
            uint32_t off = fo % SECTOR_SIZE;
            uint8_t *b = (uint8_t *) dev->read(sec);
            uint8_t next0 = (off == SECTOR_SIZE - 1) ? 0 : b[off + 1];
            if(cl & 1) {
                b[off] = (uint8_t) ((b[off] & 0x0F) | ((val << 4) & 0xF0));
                next0 = (uint8_t) ((val >> 4) & 0xFF);
            } else {
                b[off] = (uint8_t) (val & 0xFF);
                next0 = (uint8_t) ((next0 & 0xF0) | ((val >> 8) & 0x0F));
            }
            if(off == SECTOR_SIZE - 1) {
                dev->write(sec);
                uint8_t *b2 = (uint8_t *) dev->read(sec + 1);
                b2[0] = next0;
                dev->write(sec + 1);
            } else {
                b[off + 1] = next0;
                dev->write(sec);
            }
        } else {
            uint32_t fo = cl * 2;
            uint32_t sec = base + fo / SECTOR_SIZE;
            uint32_t off = fo % SECTOR_SIZE;
            uint8_t *b = (uint8_t *) dev->read(sec);
            b[off] = (uint8_t) (val & 0xFF);
            b[off + 1] = (uint8_t) ((val >> 8) & 0xFF);
            dev->write(sec);
        }
    }
}

/** LBA of the first sector of data cluster @p cl. */
static uint32_t fat_cluster_lba(device_t *dev, uint32_t cl) {
    return dev->minfo.first_data_sector + (cl - 2) * dev->minfo.cluster_sectors;
}

/** Free every cluster of the chain starting at @p first. */
static void fat_free_chain(device_t *dev, uint32_t first) {
    uint32_t cl = first;
    uint32_t eoc = fat_eoc(dev);
    while(cl >= 2 && cl < eoc && cl < fat_cluster_count(dev)) {
        uint32_t next = fat_get_entry(dev, cl);
        fat_set_entry(dev, cl, 0);
        cl = next;
    }
}

/**
 * Scan the first FAT for @p want free clusters, recording them in @p out.
 * One forward pass over the FAT sectors. @return the number found.
 */
static uint32_t fat_scan_free(device_t *dev, uint32_t want, uint32_t *out) {
    uint32_t total = fat_cluster_count(dev);
    uint32_t got = 0;
    if(dev->minfo.type == FAT12) {
        for(uint32_t cl = 2; cl < total && got < want; cl++)
            if(fat_get_entry(dev, cl) == 0)
                out[got++] = cl;
        return got;
    }
    for(uint32_t sec = 0; sec < dev->minfo.fat_size && got < want; sec++) {
        uint8_t *b = (uint8_t *) dev->read(dev->minfo.fat_offset + sec);
        for(uint32_t e = 0; e < SECTOR_SIZE / 2 && got < want; e++) {
            uint32_t cl = sec * (SECTOR_SIZE / 2) + e;
            if(cl < 2 || cl >= total)
                continue;
            if((b[e * 2] | (b[e * 2 + 1] << 8)) == 0)
                out[got++] = cl;
        }
    }
    return got;
}

/**
 * Replace the contents of @p f with @p len bytes of @p buf: reallocate the
 * cluster chain, write the data and update the directory entry.
 */
void fat_write_all(file *f, char *buf, uint32_t len) {
    if(!f)
        return;
    device_t *dev = get_dev_by_id(f->dev);
    if(!dev)
        return;

    uint32_t spc = dev->minfo.cluster_sectors;
    uint32_t bytes_per_cluster = spc * SECTOR_SIZE;
    uint32_t nc = (len + bytes_per_cluster - 1) / bytes_per_cluster;

    /* Drop the old chain first so its clusters become available again. */
    directory_t *dir = fat_get_dir(f);
    if(!dir)
        return;
    uint32_t old_first = dir->first_cluster |
                         ((uint32_t) dir->first_cluster_high_bytes << 16);
    if(old_first >= 2)
        fat_free_chain(dev, old_first);

    uint32_t first_cluster = 0;
    if(nc > 0) {
        uint32_t *cl = kmalloc(nc * sizeof(uint32_t));
        if(!cl)
            return;
        if(fat_scan_free(dev, nc, cl) < nc) {
            printf("fat: no space for %u clusters\n", nc);
            kfree(cl);
            return;
        }
        /* Link the chain. */
        for(uint32_t i = 0; i < nc; i++)
            fat_set_entry(dev, cl[i], (i + 1 < nc) ? cl[i + 1] : fat_eoc(dev) | 7);
        /* Write the data, one sector at a time. */
        for(uint32_t i = 0; i < nc; i++) {
            for(uint32_t s = 0; s < spc; s++) {
                uint32_t lba = fat_cluster_lba(dev, cl[i]) + s;
                uint32_t doff = i * bytes_per_cluster + s * SECTOR_SIZE;
                uint8_t *sec = (uint8_t *) dev->read(lba);
                uint32_t n = (doff < len) ? (len - doff) : 0;
                if(n > SECTOR_SIZE)
                    n = SECTOR_SIZE;
                if(n < SECTOR_SIZE)
                    memset(sec, 0, SECTOR_SIZE);
                if(n)
                    memcpy(sec, buf + doff, n);
                dev->write(lba);
            }
        }
        first_cluster = cl[0];
        kfree(cl);
    }

    /* Update the directory entry (re-fetch: the scans above reused the
     * shared sector buffer). */
    dir = fat_get_dir(f);
    if(dir) {
        dir->first_cluster = (uint16_t) (first_cluster & 0xFFFF);
        dir->first_cluster_high_bytes = (uint16_t) (first_cluster >> 16);
        dir->file_size = len;
        dev->write(dev->minfo.root_offset + offset);
    }
    f->len = len;
    f->current_cluster = first_cluster;
    f->eof = 0;
}

int fat_delete(char *name) {
    file f = fat_search(name);
    if(f.type == FS_NULL) {
        return 0;
    }
    
    device_t *dev = get_dev_by_id(f.dev);
    directory_t *dir = fat_get_dir(&f);
    if(dir) {
        memset(dir, 0, sizeof(directory_t));
        dev->write(dev->minfo.root_offset + offset);
        return 1;
    }
    return 0;
}

void fat_close(file *f) {
    if(f)
        f->type = FS_NULL;
}

file fat_directory(char *dir_name, int devid) {
    file f;
    strncpy(f.name, dir_name, sizeof(f.name) - 1);
    f.dev = devid;
    f.eof = 0;
    
    directory_t *dir = fat_get_dir(&f);
    if(dir) {
        f.current_cluster = dir->first_cluster;
        f.len = dir->file_size;
        if(dir->attrs & 0x10)
            f.type = FS_DIR;
        else
            f.type = FS_FILE;
    } else {
        f.type = FS_NULL;
    }
    return f;
}

file fat_open_subdir(file directory, char *name) {
    file f;
    strncpy(f.name, name, sizeof(f.name) - 1);
    char *dos_file_name = kmalloc(NAME_LEN + 1);
    to_dos_file_name(name, dos_file_name);
    char *buf = kmalloc(SECTOR_SIZE);
    
    while(!directory.eof) {
        fat_read(&directory, buf);
        directory_t *dir = (directory_t *) buf;
        for(int i = 0; i < 16; i++) {
            if(strncmp(dos_file_name, (char *) dir->filename, NAME_LEN) == 0) {
                f.current_cluster = dir->first_cluster;
                f.len = dir->file_size;
                f.eof = 0;
                f.dev = directory.dev;
                if(dir->attrs & 0x10)
                    f.type = FS_DIR;
                else
                    f.type = FS_FILE;
                kfree(buf);
                kfree(dos_file_name);
                return f;
            }
            dir++;
        }
    }
    kfree(buf);
    kfree(dos_file_name);
    f.type = FS_NULL;
    return f;
}

file fat_open(char *name) {
    return fat_search(name);
}

file fat_cd(char *dir) {
    return fat_search(dir);
}

file fat_search(char *name) {
    file cur_dir;
    int root = 1;
    
    cur_dir.dev = get_dev_id_by_name(name);
    name += 3;
    while(name++) {
        char pathname[16];
        int i;
        for(i = 0; i < (int)sizeof(pathname) - 1; i++) {
            if((name[i] == '/') || (name[i] == '\0'))
                break;
            pathname[i] = name[i];
        }
        pathname[i] = 0;
        if(root) {
            cur_dir = fat_directory(pathname, cur_dir.dev);
            root = 0;
        } else {
            cur_dir = fat_open_subdir(cur_dir, pathname);
        }
        name = strchr(name, '/');
    }
    return cur_dir;
}

void fat_ls(char *dir) {
    char *normal_name = kmalloc(NAME_LEN + 1);
    // TODO nested folder
    device_t *dev = get_dev_by_name(dir);
    for(uint32_t i = 0; i < dev->minfo.root_size; i++) {
        directory_t *direc = (directory_t *) dev->read(dev->minfo.root_offset + i);
        for(int j = 0; j < 16; j++, direc++) {
            if(((char *) direc->filename)[0] == 0)
                continue;
            to_normal_file_name((char *) direc->filename, normal_name);
            printf("%s  ", normal_name);
        }
    }
    printf("\n");
    kfree(normal_name);
}

void fat_init(filesystem *fs_fat) {
    fs_fat->mount = &fat_mount;
    fs_fat->read = &fat_read;
    fs_fat->write = &fat_write;
    fs_fat->close = &fat_close;
    fs_fat->open = &fat_open;
    fs_fat->ls = &fat_ls;
    fs_fat->cd = &fat_cd;
    fs_fat->touch = &fat_touch;
    fs_fat->delete = &fat_delete;
    fs_fat->write_all = &fat_write_all;
}

