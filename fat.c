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

#include <kheap.h>
#include <printf.h>

#define SECTOR_SIZE 512

extern uint32_t *dma_buffer;

uint8_t FAT[SECTOR_SIZE * 2];

int offset;

/**
 * @brief Reject anything that is not a plausible FAT BPB.
 *
 * The old check was just "starts with EB xx 90", which a GRUB MBR (its boot.img
 * mimics the BPB layout) and various VBRs also pass -- and then fat_mount()
 * divided the geometry by sector_bytes / cluster_sectors, which are code bytes
 * there, usually zero: a #DE panic the moment a real disk with an OS on it is
 * probed (e.g. the ThinkPad's SSD).
 */
static int bpb_looks_like_fat(const bootsector_t *bs) {
    const bios_parameter_block_t *b = &bs->bpb;
    const uint8_t *raw = (const uint8_t *) bs;

    if (bs->ignore[0] != 0xEB && bs->ignore[0] != 0xE9)
        return 0;
    if (raw[510] != 0x55 || raw[511] != 0xAA)          /* boot signature */
        return 0;

    uint16_t sb = b->sector_bytes;
    if (sb != 512 && sb != 1024 && sb != 2048 && sb != 4096)
        return 0;
    uint8_t sc = b->cluster_sectors;
    if (sc == 0 || (sc & (sc - 1)) != 0)               /* must be a power of two */
        return 0;
    if (b->n_fats == 0 || b->n_fats > 2)
        return 0;
    if (b->reserved_sectors == 0)
        return 0;
    if (b->media != 0xF0 && b->media < 0xF8)
        return 0;
    if ((b->n_sectors == 0 ? b->long_sectors : b->n_sectors) == 0)
        return 0;
    return 1;
}

/**
 * @brief Parse a volume's BPB into @c device_t::minfo.
 * @param dev The block device to probe.
 *
 * Reads sector 0 through the driver's shared buffer and fills in the geometry,
 * deriving the FAT width from the cluster count. A device that is not FAT, or
 * whose geometry is self-inconsistent (data starting past the end of the
 * volume), leaves @c minfo zeroed — so @c minfo.mounted staying 0 is the only
 * failure signal; nothing is returned and nothing is printed.
 */
void fat_mount(device_t *dev) {
    // Trying with bootsector
    bootsector_t *bs = (bootsector_t *) dev->read(0);
    if (!bpb_looks_like_fat(bs))
        return;                 /* minfo stays zeroed, minfo.mounted == 0 */

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
    dev->minfo.n_fats = bs->bpb.n_fats;
    dev->minfo.sector_bytes = bs->bpb.sector_bytes;

    /* Computed geometry must be self-consistent (a BPB can pass the field
     * checks and still be nonsense). */
    if (dev->minfo.first_data_sector >= dev->minfo.n_sectors) {
        memset(&dev->minfo, 0, sizeof(dev->minfo));
        return;
    }
    dev->minfo.data_sectors = dev->minfo.n_sectors - dev->minfo.first_data_sector;

    uint32_t total_clusters = dev->minfo.data_sectors / bs->bpb.cluster_sectors;
    if(total_clusters < 4085)
        dev->minfo.type = FAT12;
    else if(total_clusters < 65525)
        dev->minfo.type = FAT16;
    else if(total_clusters < 268435445)
        dev->minfo.type = FAT32;
    else
        dev->minfo.type = EXFAT;

    dev->minfo.mounted = 1;
}

/**
 * @brief Convert a path component to a padded, upper-case 8.3 directory name.
 * @param name Source name, "file.ext" style.
 * @param str  Destination, at least @ref NAME_LEN + 1 bytes.
 *
 * The result is space-padded to exactly @ref NAME_LEN characters with no dot,
 * which is how the name sits in a directory entry, and is NUL-terminated one
 * past that. A name longer than 8 characters or an extension longer than 3 is
 * truncated rather than rejected, so two long names can collide.
 */
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

/**
 * @brief Convert a padded 8.3 directory name back to "file.ext", lower case.
 * @param name The @ref NAME_LEN raw bytes from a directory entry.
 * @param str  Destination, at least @ref NAME_LEN + 1 bytes.
 */
void to_normal_file_name(char *name, char *str) {
    int j = 0, flag = 1;
    
    if((!name) || (!str))
        return;
    
    memset(str, ' ', NAME_LEN);
    for(int i = 0; i < strlen(name) && i < NAME_LEN; i++) {
        if(name[i] != ' ') {
            str[j] = tolower(name[i]);
            j++;
        } else if((flag == 1) && (name[8] != ' ')) {
            /* name[8] is the first extension char: test that, not name[9], so a
             * single-letter extension ("CC      C  " -> "cc.c") still gets its
             * dot and round-trips through to_dos_file_name(). */
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

/**
 * @brief LBA of the first sector of the file's current cluster.
 * @param f An open handle.
 * @return The sector number to hand the block driver.
 */
uint32_t get_phys_sector(file *f) {
    device_t *dev = get_dev_by_id(f->dev);
    /* An unknown device id gives NULL here, and the dereference below
     * would read a function pointer out of the real-mode interrupt
     * vector table and call it. A `file` handle reaches this from ring 3
     * (the fread syscall passes one straight through), so its dev field
     * is not to be trusted. */
    if(!dev)
        return 0;
    return dev->minfo.first_data_sector +
           (f->current_cluster - 2) * dev->minfo.cluster_sectors;
}

/**
 * @brief Find a file's root-directory entry by name.
 * @param f Handle whose @c name and @c dev identify the file.
 * @return A pointer into the block driver's shared sector buffer, or NULL.
 *
 * Two things about the return value matter. It points into the buffer the
 * driver reuses for every read, so it is invalidated by the next read from
 * anything — which is why @ref fat_write_all re-fetches the entry after
 * scanning the FAT. And the sector it was found in is left in the file-scope
 * @c offset, which callers need in order to write the entry back with
 * @c dev->write(root_offset + offset); calling anything else that searches
 * the directory in between overwrites it.
 *
 * Root directory only: there is no path walk here.
 */
directory_t *fat_get_dir(file *f) {
    char *dos_file_name = kmalloc(NAME_LEN + 1);
    to_dos_file_name(f->name, dos_file_name);
    device_t *dev = get_dev_by_id(f->dev);
    /* An unknown device id gives NULL here, and the dereference below
     * would read a function pointer out of the real-mode interrupt
     * vector table and call it. A `file` handle reaches this from ring 3
     * (the fread syscall passes one straight through), so its dev field
     * is not to be trusted. */
    if(!dev)
        return 0;

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

/**
 * @brief Create an empty file in the root directory.
 * @param name Device-qualified path.
 * @return 1 on success, 0 if the file already exists or the directory is full.
 *
 * Note that an existing file reports failure, unlike touch(1), and that the
 * first entry with a zero first byte is reused — including one zeroed by
 * @ref fat_delete.
 */
int fat_touch(char *name) {
    file f = fat_search(name);
    if(f.type != FS_NULL) {
        return 0;
    }
    
    char *dos_file_name = kmalloc(NAME_LEN + 1);
    to_dos_file_name(f.name, dos_file_name);
    device_t *dev = get_dev_by_id(f.dev);
    /* An unknown device id gives NULL here, and the dereference below
     * would read a function pointer out of the real-mode interrupt
     * vector table and call it. A `file` handle reaches this from ring 3
     * (the fread syscall passes one straight through), so its dev field
     * is not to be trusted. */
    if(!dev)
        return 0;
    
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

/**
 * @brief Read the next 512-byte sector of a file and advance to its next cluster.
 * @param f   Open handle; its @c current_cluster and @c eof are updated.
 * @param buf Destination, which must have room for a whole sector.
 *
 * Exactly one sector per call, which is where this driver's one-sector-per-
 * cluster assumption bites: a volume formatted with larger clusters reads only
 * the first sector of each. Two consecutive FAT sectors are pulled into the
 * shared @ref FAT buffer so a FAT12 entry straddling a sector boundary can be
 * assembled. Reaching the end-of-chain marker, or a zero entry, sets @c eof
 * instead of advancing.
 */
void fat_read(file *f, char *buf) {
    if(!f)
        return;
    
    device_t *dev = get_dev_by_id(f->dev);
    /* An unknown device id gives NULL here, and the dereference below
     * would read a function pointer out of the real-mode interrupt
     * vector table and call it. A `file` handle reaches this from ring 3
     * (the fread syscall passes one straight through), so its dev field
     * is not to be trusted. */
    if(!dev)
        return;
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

/**
 * @brief Replace a file's contents with a NUL-terminated string.
 * @param f   Open handle.
 * @param str Text to write.
 *
 * The legacy single-record write behind the console's `write <file> <text>`.
 * Binary data with embedded NULs is truncated; @ref fat_write_all takes an
 * explicit length.
 */
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

/** @brief Lowest end-of-chain marker for the volume's FAT width; chain walks
 *         compare against it rather than testing for equality. */
static uint32_t fat_eoc(device_t *dev) {
    return (dev->minfo.type == FAT12) ? 0x0FF8 : 0xFFF8;
}

/** @brief One past the highest usable cluster number. */
static uint32_t fat_cluster_count(device_t *dev) {
    return 2 + dev->minfo.data_sectors / dev->minfo.cluster_sectors;
}

/**
 * @brief Read FAT entry @p cl from the first FAT copy.
 * @param dev A mounted volume.
 * @param cl  Cluster number.
 * @return The entry: the next cluster, 0 for free, or an end-of-chain marker.
 *
 * Only the first copy is read; @ref fat_set_entry keeps the others in step.
 * A FAT12 entry straddling a sector boundary is assembled from both sectors.
 */
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

/**
 * @brief Write FAT entry @p cl, mirrored to every FAT copy.
 * @param dev A mounted volume.
 * @param cl  Cluster number.
 * @param val Value to store.
 *
 * Each copy costs its own read-modify-write, and a FAT12 entry crossing a
 * sector boundary costs two, so allocating a long chain is the expensive part
 * of a write rather than moving the data.
 */
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

/** @brief LBA of the first sector of data cluster @p cl. */
static uint32_t fat_cluster_lba(device_t *dev, uint32_t cl) {
    return dev->minfo.first_data_sector + (cl - 2) * dev->minfo.cluster_sectors;
}

/**
 * @brief Free every cluster of the chain starting at @p first.
 * @param dev   A mounted volume.
 * @param first First cluster of the chain, or anything below 2 for a no-op.
 *
 * Terminates even on a chain that loops back on itself: each cluster is zeroed
 * as it is visited, so coming round again reads a free entry and stops.
 */
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
 * @brief Scan the first FAT for free clusters.
 * @param dev  A mounted volume.
 * @param want How many are needed.
 * @param out  Receives up to @p want cluster numbers, in ascending order.
 * @return The number found, which is short of @p want only if the volume has
 *         no more space.
 *
 * One forward pass over the FAT sectors. Nothing is reserved: the caller must
 * link the clusters before anything else can scan.
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
 * @brief Replace a file's contents with @p len bytes of @p buf.
 * @param f   Open handle; its length and current cluster are updated.
 * @param buf Data to write.
 * @param len Length in bytes.
 *
 * Frees the old chain before scanning for free clusters, so a rewrite can
 * reuse the space it just released. The directory entry is deliberately
 * re-fetched after the data is written: @ref fat_get_dir returns a pointer
 * into the block driver's shared sector buffer, and every FAT scan and data
 * write in between has overwritten it.
 *
 * Not atomic and not recoverable — a failure after the chain is freed leaves
 * the file empty. Running out of space prints and returns with the old
 * contents already gone.
 */
void fat_write_all(file *f, char *buf, uint32_t len) {
    if(!f)
        return;
    device_t *dev = get_dev_by_id(f->dev);
    /* An unknown device id gives NULL here, and the dereference below
     * would read a function pointer out of the real-mode interrupt
     * vector table and call it. A `file` handle reaches this from ring 3
     * (the fread syscall passes one straight through), so its dev field
     * is not to be trusted. */
    if(!dev)
        return;
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

/**
 * @brief Delete a file from the root directory.
 * @param name Device-qualified path.
 * @return 1 on success, 0 if the file does not exist.
 *
 * Zeroes the whole directory entry rather than writing the standard 0xE5
 * deleted marker. Every scan in this driver skips a zero entry and keeps
 * going, so it is self-consistent, but another FAT implementation is entitled
 * to read a zero first byte as end-of-directory and stop there, hiding
 * whatever follows.
 *
 * The cluster chain is not freed, so the file's data clusters stay marked in
 * use until the volume is reformatted or the next @ref fat_defrag pass.
 */
int fat_delete(char *name) {
    file f = fat_search(name);
    if(f.type == FS_NULL) {
        return 0;
    }
    
    device_t *dev = get_dev_by_id(f.dev);
    /* An unknown device id gives NULL here, and the dereference below
     * would read a function pointer out of the real-mode interrupt
     * vector table and call it. A `file` handle reaches this from ring 3
     * (the fread syscall passes one straight through), so its dev field
     * is not to be trusted. */
    if(!dev)
        return 0;
    directory_t *dir = fat_get_dir(&f);
    if(dir) {
        memset(dir, 0, sizeof(directory_t));
        dev->write(dev->minfo.root_offset + offset);
        return 1;
    }
    return 0;
}

/**
 * @brief Close a handle.
 * @param f Handle, may be NULL.
 *
 * Only marks it @c FS_NULL: every write in this driver reaches the device
 * before it returns, so there is nothing to flush.
 */
void fat_close(file *f) {
    if(f)
        f->type = FS_NULL;
}

/**
 * @brief Open a name in the root directory.
 * @param dir_name The 8.3 name, unqualified.
 * @param devid    Device id to open it on.
 * @return A handle of type @c FS_DIR, @c FS_FILE, or @c FS_NULL if not found.
 */
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

/**
 * @brief Find a name inside an already-open directory.
 * @param directory The directory to search; it is consumed, being read to EOF
 *                  on a miss.
 * @param name      The name to look for.
 * @return A handle, or one of type @c FS_NULL if the name is not there.
 *
 * On the miss path only @c type is set, so the other fields of the returned
 * handle hold whatever was on the stack — callers must check @c type before
 * reading anything else.
 */
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

/** @brief Open a file by device-qualified path. @see fat_search. */
file fat_open(char *name) {
    return fat_search(name);
}

/** @brief Resolve a directory by device-qualified path; the caller checks
 *         that the result is of type @c FS_DIR. @see fat_search. */
file fat_cd(char *dir) {
    return fat_search(dir);
}

/**
 * @brief Walk a device-qualified path to a handle.
 * @param name Path such as "/hda/sub/file", leading slash optional.
 * @return A handle, of type @c FS_NULL if any component is missing.
 *
 * The device component is skipped by finding the first '/' rather than by a
 * fixed length, so names of any length ("rd", "hda") work. A bare mount point
 * with no path after it returns @c FS_NULL: there is nothing to open. The
 * first component is looked up in the root directory and the rest through
 * @ref fat_open_subdir, and each is truncated to 15 characters.
 */
file fat_search(char *name) {
    file cur_dir;
    int root = 1;
    
    cur_dir.dev = get_dev_id_by_name(name);
    /* Skip the device component ("rd", "hda", ...) — its length is not fixed,
     * so advance to the first '/' (or the end) rather than a hard-coded += 3. */
    if(name[0] == '/')
        name++;
    char *sep = strchr(name, '/');
    if(!sep) {                 /* bare "/rd" with no path: nothing to open */
        cur_dir.type = FS_NULL;
        return cur_dir;
    }
    name = sep;
    while(name++) {
        char pathname[16];
        int i;
        for(i = 0; i < (int)sizeof(pathname) - 1; i++) {
            if((name[i] == '/') || (name[i] == '\0'))
                break;
            pathname[i] = name[i];
        }
        pathname[i] = 0;

        /* "a//b" and a trailing slash both produce an empty component. Skip
         * it the way every other filesystem does, rather than looking up ""
         * and then descending through the miss. */
        if(pathname[0] == 0) {
            name = strchr(name, '/');
            continue;
        }

        if(root) {
            cur_dir = fat_directory(pathname, cur_dir.dev);
            root = 0;
        } else {
            /* Only a directory can be descended into. Without this a missed
             * component hands fat_open_subdir a handle whose fields are
             * whatever was on the stack, and it reads a device id out of
             * them. */
            if(cur_dir.type != FS_DIR) {
                cur_dir.type = FS_NULL;
                return cur_dir;
            }
            cur_dir = fat_open_subdir(cur_dir, pathname);
        }
        name = strchr(name, '/');
    }
    return cur_dir;
}

/**
 * @brief Print the root directory to the console.
 * @param dir Device-qualified path; only its device component is used.
 *
 * Root directory only — a subdirectory argument is ignored, not walked. It
 * also prints raw entries that @ref fat_listdir filters out: long-filename
 * fragments and the volume label show up as garbage names, so its output can
 * differ from the shell's completion list for the same directory.
 */
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

/**
 * @brief List the root directory as newline-separated names.
 * @param dir   Device-qualified path; only its device component is used.
 * @param out   Destination buffer, NUL-terminated on return.
 * @param outsz Capacity of @p out.
 * @return The number of names written.
 *
 * The machine-readable counterpart of @ref fat_ls, used by the shell's
 * completion. Free, deleted, long-filename and volume-label entries are all
 * skipped. Filling the buffer stops the listing and returns the count so far,
 * which is indistinguishable from having reached the end.
 */
int fat_listdir(char *dir, char *out, uint32_t outsz) {
    if(outsz)
        out[0] = 0;
    device_t *dev = get_dev_by_name(dir);
    if(!dev || outsz < 2)
        return 0;

    char normal[NAME_LEN + 1];
    uint32_t w = 0;
    int count = 0;
    for(uint32_t i = 0; i < dev->minfo.root_size; i++) {
        directory_t *e = (directory_t *) dev->read(dev->minfo.root_offset + i);
        for(int j = 0; j < 16; j++, e++) {
            uint8_t c0 = e->filename[0];
            if(c0 == 0x00 || c0 == 0xE5)               /* free / deleted */
                continue;
            if((e->attrs & 0x0F) == 0x0F || (e->attrs & DIR_VOL_LABEL))
                continue;                              /* LFN fragment / label */
            to_normal_file_name((char *) e->filename, normal);
            uint32_t l = (uint32_t) strlen(normal);
            if(l == 0 || w + l + 1 >= outsz)
                return count;
            memcpy(out + w, normal, l);
            w += l;
            out[w++] = '\n';
            out[w] = 0;
            count++;
        }
    }
    return count;
}

/* ------------------------------------------------------------------------- *
 *  Boot-time defragmentation
 *
 *  fat_defrag() runs from vfs_mount(), after the BPB has been parsed but
 *  before the volume is announced to the VFS. It relocates every regular file
 *  in the root directory so that each file occupies one contiguous cluster run
 *  and the files are packed from cluster 2 in directory order, leaving all free
 *  space in a single run at the tail. A textual progress bar is drawn while
 *  clusters move, in the style of fsck / e2fsck -C.
 *
 *  It is deliberately conservative. A volume with subdirectories, cross-linked
 *  or lost clusters, an unexpected geometry, or one that is more than half
 *  full is reported and then left completely untouched.
 *
 *  Memory budget is tiny (the kernel heap is ~140 KiB): an in-use bitmap
 *  (~1 bit per cluster) plus one uint32 array per file for its cluster chain.
 *  File data is shuffled one cluster at a time through the block driver's
 *  shared sector buffer, never buffered whole.
 * ------------------------------------------------------------------------- */

#define DFG_BAR_W 32

/** @brief Test bit @p i of the in-use cluster bitmap. */
static int  dfg_bit(const uint8_t *bm, uint32_t i) { return (bm[i >> 3] >> (i & 7)) & 1; }
/** @brief Mark cluster @p i in use. */
static void dfg_set(uint8_t *bm, uint32_t i) { bm[i >> 3] |=  (uint8_t) (1u << (i & 7)); }
/** @brief Mark cluster @p i free. */
static void dfg_clr(uint8_t *bm, uint32_t i) { bm[i >> 3] &= (uint8_t) ~(1u << (i & 7)); }

/** @brief Render an 8.3 entry as a lowercase "name.ext" into @p out, which
 *         needs 13 bytes. Used only for the progress bar. */
static void dfg_name(const directory_t *e, char *out) {
    int n = 0;
    for(int i = 0; i < 8 && e->filename[i] != ' '; i++)
        out[n++] = tolower((char) e->filename[i]);
    if(e->extension[0] != ' ') {
        out[n++] = '.';
        for(int i = 0; i < 3 && e->extension[i] != ' '; i++)
            out[n++] = tolower((char) e->extension[i]);
    }
    out[n] = 0;
}

/**
 * @brief Redraw the defrag progress bar in place.
 * @param dev   The volume being defragmented, for its mount name.
 * @param done  Clusters processed.
 * @param total Clusters to process; 0 renders as complete.
 * @param name  File currently being moved.
 *
 * One printf, prefixed with a carriage return, so the line is rewritten rather
 * than scrolled — the style fsck uses.
 */
static void dfg_bar(device_t *dev, uint32_t done, uint32_t total, const char *name) {
    static const char eq[DFG_BAR_W + 1] = "================================";
    uint32_t pct = total ? (uint32_t) ((uint64_t) done * 100 / total) : 100;
    uint32_t k   = total ? (uint32_t) ((uint64_t) done * DFG_BAR_W / total) : DFG_BAR_W;
    printf("\r  %s: defrag |%.*s%*s| %3u%%  %-13.13s",
           dev->mount, (int) k, eq, (int) (DFG_BAR_W - k), "", pct, name);
}

/**
 * @brief Find the highest free cluster, used as scratch space for evictions.
 * @param occ    In-use bitmap.
 * @param ccount One past the highest cluster number.
 * @return The cluster, or 0 if the volume is full — which is why
 *         @ref fat_defrag refuses a volume more than half full.
 */
static uint32_t dfg_free_high(const uint8_t *occ, uint32_t ccount) {
    for(uint32_t c = ccount; c-- > 2;)
        if(!dfg_bit(occ, c))
            return c;
    return 0;
}

/**
 * @brief Find which file, and where in its chain, currently holds a cluster.
 * @param chain Per-file cluster lists.
 * @param clen  Length of each list.
 * @param m     Number of files.
 * @param cl    Cluster to locate.
 * @param gf    Receives the file index.
 * @param pf    Receives the position within that file's chain.
 * @return Non-zero if the cluster was found.
 *
 * A linear search across every chain. That is quadratic in the worst case, and
 * fine here: @ref fat_defrag only runs on small volumes it has already agreed
 * to touch.
 */
static int dfg_owner(uint32_t **chain, const uint32_t *clen, uint32_t m,
                     uint32_t cl, uint32_t *gf, uint32_t *pf) {
    for(uint32_t g = 0; g < m; g++)
        for(uint32_t p = 0; p < clen[g]; p++)
            if(chain[g][p] == cl) { *gf = g; *pf = p; return 1; }
    return 0;
}

/**
 * @brief Copy one whole cluster.
 * @param dev  A mounted volume.
 * @param from Source cluster.
 * @param to   Destination cluster.
 *
 * Sector by sector through the block driver's shared buffer — a read fills it
 * and the following write flushes it straight back out — so no heap is needed
 * however large a cluster is.
 */
static void dfg_copy(device_t *dev, uint32_t from, uint32_t to) {
    uint32_t a = fat_cluster_lba(dev, from), b = fat_cluster_lba(dev, to);
    for(uint32_t s = 0; s < dev->minfo.cluster_sectors; s++) {
        dev->read(a + s);        /* fills the shared sector buffer ... */
        dev->write(b + s);       /* ... which write() flushes straight back out */
    }
}

/**
 * @brief Point a root-directory entry at a new first cluster.
 * @param dev   A mounted volume.
 * @param idx   Index of the entry in the root directory.
 * @param first The new first cluster.
 */
static void dfg_set_first(device_t *dev, uint32_t idx, uint32_t first) {
    uint32_t per = SECTOR_SIZE / 32;
    uint32_t sec = dev->minfo.root_offset + idx / per;
    directory_t *d = (directory_t *) dev->read(sec);
    d[idx % per].first_cluster = (uint16_t) (first & 0xFFFF);
    d[idx % per].first_cluster_high_bytes = (uint16_t) (first >> 16);
    dev->write(sec);
}

/**
 * @brief Repack every root-directory file into one contiguous cluster run.
 * @param dev A mounted volume.
 *
 * Called from @ref vfs_mount before the volume is announced, so nothing else
 * can be touching it. Files are packed from cluster 2 in directory order,
 * leaving all free space in a single run at the tail; a cluster already
 * holding another file's data is evicted to the highest free cluster first.
 *
 * Bails out, untouched, on anything it does not fully understand: a FAT width
 * other than 12 or 16, an unexpected geometry, subdirectories, cross-linked or
 * lost clusters, or a volume more than half full — there has to be somewhere
 * to evict to. Every exit path frees its allocations.
 *
 * Memory is the constraint: with a ~140 KiB kernel heap it holds one bit per
 * cluster plus one cluster list per file, and moves file data one cluster at a
 * time through the driver's shared sector buffer rather than buffering a file.
 */
void fat_defrag(device_t *dev) {
    fat_mount_info_t *mi = &dev->minfo;

    /* Only FAT12/FAT16 with the geometry the driver understands. A device that
     * failed to mount leaves minfo zeroed, which these checks also reject. */
    if((mi->type != FAT12 && mi->type != FAT16) ||
       mi->cluster_sectors == 0 || mi->sector_bytes != SECTOR_SIZE ||
       mi->fat_size == 0 || mi->n_fats == 0)
        return;

    uint32_t ccount = fat_cluster_count(dev);        /* one past the last cluster */
    uint32_t fat_capacity = mi->fat_size * (SECTOR_SIZE / 2) + 2;
    if(ccount <= 3 || ccount > fat_capacity)
        return;

    uint32_t bmsz = (ccount + 7) / 8;
    uint8_t *occ = kmalloc(bmsz);
    if(!occ)
        return;
    memset(occ, 0, bmsz);

    /* ---- 1. in-use bitmap, straight from FAT copy #0 ---- */
    uint32_t used = 0;
    if(mi->type == FAT16) {
        for(uint32_t s = 0; s < mi->fat_size; s++) {
            uint8_t sec[SECTOR_SIZE];
            memcpy(sec, dev->read(mi->fat_offset + s), SECTOR_SIZE);
            for(uint32_t e = 0; e < SECTOR_SIZE / 2; e++) {
                uint32_t c = s * (SECTOR_SIZE / 2) + e;
                if(c < 2 || c >= ccount)
                    continue;
                if(sec[e * 2] | (sec[e * 2 + 1] << 8)) { dfg_set(occ, c); used++; }
            }
        }
    } else {
        for(uint32_t c = 2; c < ccount; c++)
            if(fat_get_entry(dev, c)) { dfg_set(occ, c); used++; }
    }

    /* Not enough slack to shuffle in place with a safety margin: leave it be. */
    if((uint64_t) used * 2 + 64 > ccount - 2) {
        printf("  %s: FAT%s too full to defragment (%u/%u clusters)\n",
               dev->mount, mi->type == FAT12 ? "12" : "16", used, ccount - 2);
        kfree(occ);
        return;
    }

    /* ---- 2. scan the root directory ---- */
    uint32_t per = SECTOR_SIZE / 32;
    uint32_t m = 0;
    int bad = 0, ended = 0;
    for(uint32_t s = 0; s < mi->root_size && !ended && !bad; s++) {
        uint8_t sec[SECTOR_SIZE];
        memcpy(sec, dev->read(mi->root_offset + s), SECTOR_SIZE);
        directory_t *d = (directory_t *) sec;
        for(uint32_t j = 0; j < per; j++) {
            uint8_t c0 = d[j].filename[0];
            if(c0 == 0x00) { ended = 1; break; }
            if(c0 == 0xE5 || d[j].attrs == 0x0F || (d[j].attrs & DIR_VOL_LABEL))
                continue;
            if(d[j].attrs & DIR_SUBDIR) { bad = 1; break; }   /* subdirs: skip volume */
            m++;
        }
    }
    if(bad || m == 0) {
        if(bad)
            printf("  %s: has subdirectories, skipping defragmentation\n", dev->mount);
        kfree(occ);
        return;
    }

    uint32_t  *ndir   = kmalloc(sizeof(uint32_t)   * m);
    uint32_t  *want   = kmalloc(sizeof(uint32_t)   * m);
    uint32_t  *clen   = kmalloc(sizeof(uint32_t)   * m);
    uint32_t **chain  = kmalloc(sizeof(uint32_t *) * m);
    char    (*nm)[16] = kmalloc(sizeof(*nm)        * m);
    uint32_t nchain = 0;
    if(!ndir || !want || !clen || !chain || !nm)
        goto cleanup;

    /* ---- 3. fill the file table and walk each current cluster chain ---- */
    ended = 0;
    uint32_t idx = 0;
    uint32_t eoc_lo = (mi->type == FAT12) ? 0x0FF8 : 0xFFF8;
    for(uint32_t s = 0; s < mi->root_size && !ended && !bad; s++) {
        uint8_t sec[SECTOR_SIZE];
        memcpy(sec, dev->read(mi->root_offset + s), SECTOR_SIZE);
        directory_t *d = (directory_t *) sec;
        for(uint32_t j = 0; j < per && !bad; j++) {
            uint8_t c0 = d[j].filename[0];
            if(c0 == 0x00) { ended = 1; break; }
            if(c0 == 0xE5 || d[j].attrs == 0x0F ||
               (d[j].attrs & DIR_VOL_LABEL) || (d[j].attrs & DIR_SUBDIR))
                continue;

            uint32_t bpc = mi->cluster_sectors * SECTOR_SIZE;
            uint32_t sz  = d[j].file_size;
            uint32_t nc  = (sz + bpc - 1) / bpc;
            uint32_t fc  = d[j].first_cluster |
                           ((uint32_t) d[j].first_cluster_high_bytes << 16);
            if(nc == 0)
                continue;                            /* empty file: no clusters */
            if(idx >= m) { bad = 1; break; }

            uint32_t *ch = kmalloc(sizeof(uint32_t) * nc);
            if(!ch) { bad = 1; break; }
            chain[nchain++] = ch;

            uint32_t cl = fc, n = 0;
            while(cl >= 2 && cl < eoc_lo && n < nc) {
                if(cl >= ccount) { bad = 1; break; }
                ch[n++] = cl;
                cl = fat_get_entry(dev, cl);
            }
            /* Exactly nc clusters, ending on a real EOC: anything else means the
             * directory size and the FAT disagree -> do not touch this volume. */
            if(bad || n != nc || (cl >= 2 && cl < eoc_lo)) { bad = 1; break; }

            dfg_name(&d[j], nm[idx]);
            ndir[idx]  = s * per + j;
            want[idx]  = nc;
            clen[idx]  = nc;
            idx++;
        }
    }
    if(bad || idx != m) {
        if(bad)
            printf("  %s: FAT needs repair, skipping defragmentation\n", dev->mount);
        goto cleanup;
    }

    /* Every allocated cluster must belong to exactly one file we can see. */
    uint32_t tot = 0;
    for(uint32_t f = 0; f < m; f++)
        tot += want[f];
    if(tot != used) {
        printf("  %s: %u lost/shared clusters, skipping defragmentation\n",
               dev->mount, used > tot ? used - tot : tot - used);
        goto cleanup;
    }

    /* ---- 4. how fragmented is it already? ---- */
    uint32_t base = 2, frag = 0;
    for(uint32_t f = 0; f < m; f++) {
        int ok = (chain[f][0] == base);
        for(uint32_t j = 1; j < clen[f] && ok; j++)
            if(chain[f][j] != chain[f][j - 1] + 1)
                ok = 0;
        if(!ok)
            frag++;
        base += want[f];
    }

    printf("  %s: FAT%s  %u/%u clusters  %u file%s  %u fragmented\n",
           dev->mount, mi->type == FAT12 ? "12" : "16", used, ccount - 2,
           m, m == 1 ? "" : "s", frag);
    if(frag == 0)
        goto cleanup;                                /* already contiguous */

    /* ---- 5. compact: place each file into [base, base + len) in dir order ---- */
    uint32_t moved_files = 0, moved_cl = 0, done = 0;
    base = 2;
    dfg_bar(dev, 0, tot, nm[0]);
    for(uint32_t f = 0; f < m; f++) {
        int touched = 0;
        for(uint32_t j = 0; j < clen[f]; j++) {
            uint32_t dst = base + j;
            uint32_t src = chain[f][j];
            if(src != dst) {
                touched = 1;
                if(dfg_bit(occ, dst)) {
                    /* dst is live data: our own later cluster, or another file */
                    uint32_t g = 0, p = 0;
                    if(!dfg_owner(chain, clen, m, dst, &g, &p)) {
                        printf("\n  %s: internal error, aborting defrag\n", dev->mount);
                        goto cleanup;
                    }
                    uint32_t ev = dfg_free_high(occ, ccount);
                    if(ev == 0) {
                        printf("\n  %s: out of scratch space, aborting defrag\n",
                               dev->mount);
                        goto cleanup;
                    }
                    dfg_copy(dev, dst, ev);
                    dfg_set(occ, ev);
                    dfg_clr(occ, dst);
                    if(g == f) {
                        chain[f][p] = ev;            /* fixed up by the relink below */
                    } else {
                        if(p == 0)
                            dfg_set_first(dev, ndir[g], ev);
                        else
                            fat_set_entry(dev, chain[g][p - 1], ev);
                        fat_set_entry(dev, ev, (p + 1 < clen[g]) ? chain[g][p + 1]
                                                                 : (fat_eoc(dev) | 7));
                        fat_set_entry(dev, dst, 0);
                        chain[g][p] = ev;
                    }
                    moved_cl++;
                }
                dfg_copy(dev, src, dst);
                dfg_set(occ, dst);
                dfg_clr(occ, src);
                chain[f][j] = dst;
                moved_cl++;
            }
            done++;
            if((done & 15) == 0 || j + 1 == clen[f])
                dfg_bar(dev, done, tot, nm[f]);
        }
        if(touched) {
            for(uint32_t j = 0; j < clen[f]; j++)
                fat_set_entry(dev, base + j, (j + 1 < clen[f]) ? (base + j + 1)
                                                              : (fat_eoc(dev) | 7));
            dfg_set_first(dev, ndir[f], base);
            moved_files++;
        }
        base += want[f];
    }

    /* ---- 6. free every cluster past the packed region ---- */
    if(mi->type == FAT16) {
        for(uint32_t s = 0; s < mi->fat_size; s++) {
            uint8_t sec[SECTOR_SIZE];
            memcpy(sec, dev->read(mi->fat_offset + s), SECTOR_SIZE);
            for(uint32_t e = 0; e < SECTOR_SIZE / 2; e++) {
                uint32_t c = s * (SECTOR_SIZE / 2) + e;
                if(c < 2 + tot || c >= ccount)
                    continue;
                if(sec[e * 2] | (sec[e * 2 + 1] << 8))
                    fat_set_entry(dev, c, 0);
            }
        }
    } else {
        for(uint32_t c = 2 + tot; c < ccount; c++)
            if(fat_get_entry(dev, c))
                fat_set_entry(dev, c, 0);
    }

    dfg_bar(dev, tot, tot, "done");
    printf("\n  %s: defragmented %u file%s, moved %u cluster%s\n",
           dev->mount, moved_files, moved_files == 1 ? "" : "s",
           moved_cl, moved_cl == 1 ? "" : "s");

cleanup:
    for(uint32_t i = 0; i < nchain; i++)
        kfree(chain[i]);
    kfree(nm);
    kfree(chain);
    kfree(clen);
    kfree(want);
    kfree(ndir);
    kfree(occ);
}

/**
 * @brief Fill in the VFS operation vector with this driver's entry points.
 * @param fs_fat The vector to populate, normally @c device_t::fs.
 *
 * @ref vfs_mount publishes it only after @ref fat_mount confirms the volume
 * really is FAT.
 */
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

