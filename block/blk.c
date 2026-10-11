/**
 * @file blk.c
 * @brief Raw block access for ring 3: enumerate the disks, read and write their
 *        sectors. See blk.h for the interface and for why it is safe enough.
 *
 * The drivers all work the same way (ata.c, ahci.c, nvme.c, initrd.c): one sector
 * through a shared scratch buffer, read-modify-write -- @c read(lba) fills the buffer
 * and returns it, the caller changes it, @c write(lba) sends it back. So a raw write
 * of a sector is a read of the same sector (to get the buffer), a memcpy over it and
 * a write. That is two commands where one would do, and it is the price of not
 * touching three drivers' internals; at the sizes involved (one 32 MiB image) it
 * costs seconds.
 *
 * The buffer is shared with the file system, which is not re-entrant, so every
 * touch of a driver here is under the VFS lock (vfs_lock). It is dropped between
 * small groups of sectors, because the lock holds off preemption and interrupts on
 * this CPU, and 256 sectors of PIO is long enough to be noticed on the desktop.
 */
#include <linux/blk.h>
#include <linux/device.h>
#include <linux/vfs.h>
#include <linux/mm.h>
#include <linux/printf.h>
#include <linux/string.h>

int blk_defer_flush = 0;

/** Sectors moved per lock hold. */
#define GROUP 16

/** @return non-zero if @p dev is a disk the raw write path may write. */
static int is_target(const device_t *dev) {
    return (dev->kind == BLK_KIND_ATA || dev->kind == BLK_KIND_AHCI ||
            dev->kind == BLK_KIND_NVME) && dev->sectors > 0;
}

/** @return non-zero if [@p p, @p p + @p len) is plausibly a ring-3 buffer. The
 *  pointer comes straight from user space, so keep it out of kernel memory and
 *  stop it wrapping -- the same test getrandom and getpad make. */
static int user_range(uint32_t p, uint32_t len) {
    return p >= KERNEL_SPACE_END && p + len >= p;
}

uint32_t blk_sys_info(uint32_t index, uint32_t out) {
    if (!user_range(out, sizeof(struct blk_info)))
        return (uint32_t) -1;
    device_t *dev = device_at((int) index);
    if (!dev)
        return 0;

    struct blk_info *bi = (struct blk_info *) out;
    memset(bi, 0, sizeof(*bi));
    bi->index = index;
    bi->kind = (uint32_t) dev->kind;
    bi->sectors = dev->sectors;
    bi->flags = (is_target(dev) ? BLK_F_TARGET : 0) | (dev->minfo.mounted ? BLK_F_MOUNTED : 0);
    strncpy(bi->name, dev->mount, sizeof(bi->name) - 1);
    strncpy(bi->model, dev->model, sizeof(bi->model) - 1);
    return 1;
}

uint32_t blk_sys_read(uint32_t index, uint32_t lba, uint32_t buf, uint32_t count) {
    device_t *dev = device_at((int) index);
    if (!dev || !dev->read || count == 0 || count > BLK_MAX_COUNT ||
        !user_range(buf, count * BLK_SECTOR))
        return (uint32_t) -1;
    if (dev->sectors && (lba > dev->sectors || count > dev->sectors - lba))
        return (uint32_t) -1;

    uint8_t *dst = (uint8_t *) buf;
    for (uint32_t done = 0; done < count; ) {
        uint32_t n = count - done < GROUP ? count - done : GROUP;
        int s = vfs_lock();
        for (uint32_t i = 0; i < n; i++)
            memcpy(dst + (done + i) * BLK_SECTOR, dev->read((int) (lba + done + i)), BLK_SECTOR);
        vfs_unlock(s);
        done += n;
    }
    return count;
}

uint32_t blk_sys_write(uint32_t index, uint32_t lba, uint32_t buf, uint32_t count) {
    device_t *dev = device_at((int) index);
    if (!dev || !is_target(dev) || !dev->read || !dev->write || count == 0 ||
        count > BLK_MAX_COUNT || !user_range(buf, count * BLK_SECTOR))
        return (uint32_t) -1;
    if (lba > dev->sectors || count > dev->sectors - lba)
        return (uint32_t) -1;

    const uint8_t *src = (const uint8_t *) buf;
    uint32_t ok = 1;
    for (uint32_t done = 0; ok && done < count; ) {
        uint32_t n = count - done < GROUP ? count - done : GROUP;
        int s = vfs_lock();
        blk_defer_flush = 1;                    /* the caller flushes (BLK_CTL_FLUSH) */
        for (uint32_t i = 0; i < n && ok; i++) {
            /* The driver's sector buffer: overwrite it and write it out. Without a scratch
             * hook, the only way to get at the buffer is to read the sector into it. */
            char *sec = dev->scratch ? dev->scratch() : dev->read((int) (lba + done + i));
            memcpy(sec, src + (done + i) * BLK_SECTOR, BLK_SECTOR);
            if (!dev->write((int) (lba + done + i)))
                ok = 0;
        }
        blk_defer_flush = 0;
        vfs_unlock(s);
        done += n;
    }
    return ok ? count : (uint32_t) -1;
}

uint32_t blk_sys_ctl(uint32_t index, uint32_t op) {
    device_t *dev = device_at((int) index);
    if (!dev || !is_target(dev))
        return (uint32_t) -1;

    if (op == BLK_CTL_FLUSH) {
        int s = vfs_lock();
        int r = dev->flush ? dev->flush() : 1;
        vfs_unlock(s);
        return r ? 0 : (uint32_t) -1;
    }
    if (op == BLK_CTL_RESCAN) {
        /* The partition table, the boot sector, the whole volume may have changed:
         * drop the old mount and look again, as at boot (vfs_mount does fat_mount). */
        vfs_unmount(dev->mount);
        dev->lba_base = 0;
        vfs_mount(dev->mount);
        return dev->minfo.mounted ? 1 : 0;
    }
    return (uint32_t) -1;
}

static const char *kind_name(int kind) {
    switch (kind) {
    case BLK_KIND_RAMDISK: return "ramdisk";
    case BLK_KIND_PAYLOAD: return "payload";
    case BLK_KIND_FLOPPY:  return "floppy";
    case BLK_KIND_ATA:     return "ata";
    case BLK_KIND_AHCI:    return "ahci";
    case BLK_KIND_NVME:    return "nvme";
    default:               return "?";
    }
}

void blk_print(void) {
    for (int i = 0; i < BLK_MAX; i++) {
        device_t *dev = device_at(i);
        if (!dev)
            continue;
        printf("%d  %-4s %-8s %8u MiB  %s%s%s\n", i, dev->mount, kind_name(dev->kind),
               dev->sectors / 2048, dev->minfo.mounted ? "mounted " : "",
               is_target(dev) ? "target " : "", dev->model);
    }
}
