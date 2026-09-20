/**
 * @file vfs.c
 * @brief Virtual filesystem layer: routes device-qualified paths ("fda/x",
 *        "hda/y") to the right mounted filesystem and serialises every
 *        operation against the cooperative scheduler (the block drivers and
 *        fat.c share non-reentrant buffers).
 */
//#include <console.h>
//#include <hal/hal.h>
#include <lib/string.h>
#include <vfs.h>
#include <fat.h>
#include <device.h>
#include <sched.h>
#include <pit.h>
#include <printf.h>
#include <kheap.h>
#include <spinlock.h>
#include <nfs.h>
#include <log.h>

static filesystem *devs[MAX_DEVICES];

/*
 * The block drivers hand back a pointer to one shared sector buffer that the
 * caller must consume before yielding, and fat.c keeps global FAT/scratch
 * state — none of it is re-entrant. A filesystem operation therefore has to run
 * to completion without another thread — on this CPU (sched_state) or another
 * (fs_lock) — touching the filesystem. These bracket every VFS entry point that
 * reaches a driver. fs_enter/fs_leave are not nested, so one saved-flags slot
 * (only written by the lock holder) is enough.
 */
static uint32_t fs_saved_flags;

/**
 * @brief Take the filesystem lock and close the local preemption gate.
 * @return The previous scheduler state, to hand back to @ref fs_leave.
 */
static int fs_enter(void) {
    fs_saved_flags = spin_lock(&fs_lock);
    int prev = get_sched_state();
    sched_state(0);
    return prev;
}

/**
 * @brief Release the filesystem lock and restore preemption.
 * @param prev The value @ref fs_enter returned.
 */
static void fs_leave(int prev) {
    sched_state(prev);
    spin_unlock(&fs_lock, fs_saved_flags);
}

/** @brief Clear the mount table. No filesystem is visible until @ref vfs_mount. */
void vfs_init() {
    for(int i = 0; i < MAX_DEVICES; i++)
        devs[i] = 0;
}

/** @brief Print the mount point of every mounted volume, plus NFS if it is up. */
void vfs_ls() {
    for(int i = 0; i < MAX_DEVICES; i++) {
        if(devs[i] != 0) {
            device_t *device = get_dev_by_id(i);
            if(device)
                printf("%s\n", device->mount);
        }
    }
    if(nfs_is_mounted())
        printf("%s\n", NFS_MOUNTPOINT);
}

/**
 * @brief Print a directory listing for the user.
 * @param dir Device-qualified path, leading slash included ("/hda/sub").
 *
 * NFS paths are answered by nfs.c; everything else is dispatched to the
 * filesystem bound to the device named in the path.
 */
void vfs_ls_dir(char *dir) {
    if(nfs_is_mounted() && nfs_owns_path(dir)) {
        nfs_vfs_ls(dir);
        return;
    }
    int device = get_dev_id_by_name(dir);
    if(device >= 0 && devs[device]) {
        int s = fs_enter();
        devs[device]->ls(dir + 1);
        fs_leave(s);
    }
}

/**
 * @brief Machine-readable directory listing, for the shell's completion.
 * @param dir   Device-qualified path.
 * @param out   Buffer for the packed names.
 * @param outsz Size of @p out; truncated to fit.
 * @return Number of entries written, 0 if the path is not a mounted FAT volume.
 *
 * NFS returns 0 here on purpose: the NFS client can list to the console but
 * has no packed form for a caller to parse.
 */
int vfs_listdir(char *dir, char *out, uint32_t outsz) {
    if(outsz)
        out[0] = 0;
    if(nfs_is_mounted() && nfs_owns_path(dir))
        return 0;                       /* NFS: no machine-readable listing */
    int device = get_dev_id_by_name(dir);
    if(device < 0 || !devs[device])
        return 0;
    int s = fs_enter();
    int n = fat_listdir(dir, out, outsz);
    fs_leave(s);
    return n;
}

/**
 * @brief Test whether a path can be entered as a directory.
 * @param name Device-qualified path.
 * @return Non-zero if @p name names a directory or a bare mount point.
 *
 * A bare mount point ("/hda") is accepted without touching the medium; only a
 * deeper path is looked up on the filesystem.
 */
int vfs_cd(char *name) {
    if(nfs_is_mounted() && nfs_owns_path(name)) {
        if(strchr(name + 1, '/')) {
            file f = nfs_vfs_cd(name);
            return (f.type == FS_DIR);
        }
        return 1;   /* bare "/nfs" mount point */
    }
    int device = get_dev_id_by_name(name);
    int ret = 0;
    if(device >= 0 && devs[device]) {
        if(strchr(name + 1, '/')) {
            int s = fs_enter();
            file f = devs[device]->cd(name + 1);
            fs_leave(s);
            ret = (f.type == FS_DIR);
        } else {
            ret = 1;
        }
    }
    return ret;
}

/**
 * @brief Create an empty file.
 * @param name Device-qualified path.
 * @return Non-zero on success.
 */
int vfs_touch(char *name) {
    if(nfs_is_mounted() && nfs_owns_path(name))
        return nfs_vfs_touch(name);
    int device = get_dev_id_by_name(name);
    int ret = 0;
    if(device >= 0 && devs[device]) {
        int s = fs_enter();
        ret = devs[device]->touch(name + 1);
        fs_leave(s);
    }
    return ret;
}

/**
 * @brief Delete a file.
 * @param name Device-qualified path.
 * @return Non-zero on success.
 */
int vfs_delete(char *name) {
    if(nfs_is_mounted() && nfs_owns_path(name))
        return nfs_vfs_delete(name);
    int device = get_dev_id_by_name(name);
    int ret = 0;
    if(device >= 0 && devs[device]) {
        int s = fs_enter();
        ret = devs[device]->delete(name + 1);
        fs_leave(s);
    }
    return ret;
}

/**
 * @brief Open a file into a kernel-heap handle.
 * @param name Device-qualified path.
 * @param mode "w" truncates; anything else opens for reading.
 * @return A handle, never NULL. A failed open comes back with type
 *         @c FS_NULL and @c dev 0, so @ref vfs_file_close stays in bounds.
 */
file *vfs_file_open(char *name, char *mode) {
    file *f = kmalloc(sizeof(file));
    f->type = FS_NULL;
    f->dev = 0;   // keep vfs_file_close() in-bounds if the open fails
    if(nfs_is_mounted() && nfs_owns_path(name)) {
        *f = nfs_vfs_open(name, mode);
        return f;
    }
    int device = get_dev_id_by_name(name);
    if(device >= 0 && devs[device]) {
        int s = fs_enter();
        *f = devs[device]->open(name + 1);
        fs_leave(s);
        if(f->type == FS_FILE && strcmp(mode, "w") == 0) {
            f->len = 0;
        }
    }
    return f;
}

/**
 * @brief Open a file into a handle allocated on the calling process's heap.
 * @param name Device-qualified path.
 * @param mode "w" truncates; anything else opens for reading.
 * @return The handle, or 0 if there is no current user process or the open
 *         failed — unlike @ref vfs_file_open, which always returns storage.
 */
file *vfs_file_open_user(char *name, char *mode) {
    process_t *cur = current_user_proc();
    if(cur && cur->thread_list) {
        file *f = (file *) umalloc_locked(sizeof(file), cur->thread_list, cur->pdir);
        if(nfs_is_mounted() && nfs_owns_path(name)) {
            file fil = nfs_vfs_open(name, mode);
            memcpy(f, &fil, sizeof(file));
            return fil.type == FS_NULL ? 0 : f;
        }
        int device = get_dev_id_by_name(name);
        if(device >= 0 && devs[device]) {
            int s = fs_enter();
            file fil = devs[device]->open(name + 1);
            fs_leave(s);
            memcpy(f, &fil, sizeof(file));
            if(f->type == FS_FILE) {
                if(strcmp(mode, "w") == 0) {
                    f->len = 0;
                }
                return f;
            }
        }
    }
    return 0;
}

/**
 * @brief Is @p f's device id one this table can be indexed with?
 *
 * A `file` handle arrives here from ring 3 -- the fread syscall passes the
 * pointer the program gave it straight through -- so its dev field is an
 * untrusted integer, not a device. Indexing devs[] with it unchecked reads
 * past the array and calls whatever function pointer is there. NFS handles
 * (@ref NFS_DEV_ID) are routed out before this is reached.
 */
static int dev_ok(const file *f) {
    return f && f->dev < MAX_DEVICES && devs[f->dev];
}

/**
 * @brief Read the next chunk of an open file.
 * @param f   Handle from @ref vfs_file_open.
 * @param str Destination buffer, sized by the caller from @c f->len.
 */
void vfs_file_read(file *f, char *str) {
    if(f && f->dev == NFS_DEV_ID) {
        nfs_vfs_read(f, str);
        return;
    }
    if(dev_ok(f)) {
        int s = fs_enter();
        devs[f->dev]->read(f, str);
        fs_leave(s);
    }
}

/**
 * @brief Append a NUL-terminated string to an open file.
 * @param f   Handle from @ref vfs_file_open.
 * @param str Text to write.
 */
void vfs_file_write(file *f, char *str) {
    if(f && f->dev == NFS_DEV_ID) {
        nfs_vfs_write(f, str);
        return;
    }
    if(dev_ok(f)) {
        int s = fs_enter();
        devs[f->dev]->write(f, str);
        fs_leave(s);
    }
}

/**
 * @brief Write a whole buffer to a path in one call, creating it if needed.
 * @param name Path, with or without a leading slash.
 * @param buf  Bytes to write.
 * @param len  Length of @p buf.
 * @return @p len on success, -1 on failure.
 *
 * The open, the create-if-missing retry, the write and the close all happen
 * inside one @ref fs_enter region, so a concurrent operation cannot land
 * between creating the file and filling it. The NFS path writes text only and
 * ignores @p len.
 */
int vfs_spit(char *name, char *buf, uint32_t len) {
    /* Normalise to a leading-slash device path ("/hda/foo"); the FAT hooks
     * want the device-qualified tail ("hda/foo"), i.e. path + 1. */
    char path[80];
    if(name[0] == '/') {
        strncpy(path, name, sizeof(path) - 1);
    } else {
        path[0] = '/';
        strncpy(path + 1, name, sizeof(path) - 2);
    }
    path[sizeof(path) - 1] = 0;

    if(nfs_is_mounted() && nfs_owns_path(path)) {
        file f = nfs_vfs_open(path, "w");
        if(f.type == FS_NULL)
            return -1;
        nfs_vfs_write(&f, buf);        /* NFS path: text record only */
        nfs_vfs_close(&f);
        return (int) len;
    }

    int device = get_dev_id_by_name(path);
    if(device < 0 || !devs[device] || !devs[device]->write_all)
        return -1;

    int s = fs_enter();
    file f = devs[device]->open(path + 1);
    if(f.type == FS_NULL) {
        devs[device]->touch(path + 1);
        f = devs[device]->open(path + 1);
    }
    int ret = -1;
    if(f.type == FS_FILE) {
        devs[device]->write_all(&f, buf, len);
        devs[device]->close(&f);
        ret = (int) len;
    }
    fs_leave(s);
    return ret;
}

/**
 * @brief Close a handle from @ref vfs_file_open and free it.
 * @param f Handle, may be NULL.
 */
void vfs_file_close(file *f) {
    if(f) {
        if(f->dev == NFS_DEV_ID) {
            nfs_vfs_close(f);
            kfree(f);
            return;
        }
        if(dev_ok(f)) {
            int s = fs_enter();
            devs[f->dev]->close(f);
            fs_leave(s);
            kfree(f);
        }
    }
}

/**
 * @brief Close a handle from @ref vfs_file_open_user and free it on the
 *        process heap.
 * @param f Handle, may be NULL.
 */
void vfs_file_close_user(file *f) {
    if(f) {
        if(dev_ok(f)) {
            int s = fs_enter();
            devs[f->dev]->close(f);
            fs_leave(s);
            process_t *cur = current_user_proc();
            if(cur && cur->thread_list) {
                ufree_locked(f, cur->thread_list);
            }
        }
    }
}

/**
 * @brief Map a legacy "hd\\..." / "fd\\..." path to a device index.
 * @param name Path using a backslash separator after the two-letter device.
 * @return 1 for hd, 0 for fd, -1 if the path is not in that form.
 */
int vfs_get_dev(char *name) {
    if(name[3] == '\\') {
        if(strncmp(name, "hd", 2) == 0)
            return 1;
        else if(strncmp(name, "fd", 2) == 0)
            return 0;
    }
    return -1;
}

/**
 * @brief Mount a registered block device and make its filesystem reachable.
 * @param name Device name as registered, e.g. "hda".
 *
 * Parses the BPB, and only on a real FAT volume defragments it and publishes
 * the filesystem. A device that is not FAT — a disk partitioned for another
 * OS, a blank one — stays registered as a block device but exposes no
 * filesystem, and is deliberately not defragmented.
 */
void vfs_mount(char *name) {
    device_t *dev = get_dev_by_name(name);
    if(!dev)
        return;
    /* Parse the geometry, defragment the volume, then make it visible. The
     * defrag pass shuffles raw sectors and drives its own progress output, so
     * it has to run before any other thread can reach the filesystem. */
    fat_mount(dev);
    if(!dev->minfo.mounted) {
        /* Not a FAT volume (a partitioned disk with another OS on it, a blank
         * device, ...). Leave the block device registered but don't expose a
         * filesystem and don't defrag it. */
        klogf(LOG_INFO, "vfs: %s is not a FAT volume, not mounted\n", dev->mount);
        return;
    }
    int s = fs_enter();
    fat_defrag(dev);
    fs_leave(s);
    devs[dev->id] = &dev->fs;
}

/**
 * @brief Remove a volume's filesystem from the mount table.
 * @param name Device name as registered.
 *
 * A name that matches no registered device is a no-op. Every registered
 * device has an id below 8 (@ref device_register drops the rest), which is
 * inside @ref MAX_DEVICES, so the index needs no further check.
 */
void vfs_unmount(char *name) {
    device_t *dev = get_dev_by_name(name);
    if(!dev)
        return;
    devs[dev->id] = 0;
}
