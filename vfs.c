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

static int fs_enter(void) {
    fs_saved_flags = spin_lock(&fs_lock);
    int prev = get_sched_state();
    sched_state(0);
    return prev;
}

static void fs_leave(int prev) {
    sched_state(prev);
    spin_unlock(&fs_lock, fs_saved_flags);
}

void vfs_init() {
    for(int i = 0; i < MAX_DEVICES; i++)
        devs[i] = 0;
}

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

file *vfs_file_open_user(char *name, char *mode) {
    process_t *cur = get_cur_proc();
    if(cur && cur->thread_list) {
        file *f = (file *) umalloc(sizeof(file), (vmm_addr_t *) cur->thread_list->heap);
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

void vfs_file_read(file *f, char *str) {
    if(f && f->dev == NFS_DEV_ID) {
        nfs_vfs_read(f, str);
        return;
    }
    if(f && devs[f->dev]) {
        int s = fs_enter();
        devs[f->dev]->read(f, str);
        fs_leave(s);
    }
}

void vfs_file_write(file *f, char *str) {
    if(f && f->dev == NFS_DEV_ID) {
        nfs_vfs_write(f, str);
        return;
    }
    if(f && devs[f->dev]) {
        int s = fs_enter();
        devs[f->dev]->write(f, str);
        fs_leave(s);
    }
}

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

void vfs_file_close(file *f) {
    if(f) {
        if(f->dev == NFS_DEV_ID) {
            nfs_vfs_close(f);
            kfree(f);
            return;
        }
        if(devs[f->dev]) {
            int s = fs_enter();
            devs[f->dev]->close(f);
            fs_leave(s);
            kfree(f);
        }
    }
}

void vfs_file_close_user(file *f) {
    if(f) {
        if(devs[f->dev]) {
            int s = fs_enter();
            devs[f->dev]->close(f);
            fs_leave(s);
            process_t *cur = get_cur_proc();
            if(cur && cur->thread_list) {
                ufree(f, (vmm_addr_t *) cur->thread_list->heap);
            }
        }
    }
}

int vfs_get_dev(char *name) {
    if(name[3] == '\\') {
        if(strncmp(name, "hd", 2) == 0)
            return 1;
        else if(strncmp(name, "fd", 2) == 0)
            return 0;
    }
    return -1;
}

void vfs_mount(char *name) {
    device_t *dev = get_dev_by_name(name);
    if(!dev)
        return;
    /* Parse the geometry, defragment the volume, then make it visible. The
     * defrag pass shuffles raw sectors and drives its own progress output, so
     * it has to run before any other thread can reach the filesystem. */
    fat_mount(dev);
    int s = fs_enter();
    fat_defrag(dev);
    fs_leave(s);
    devs[dev->id] = &dev->fs;
}

void vfs_unmount(char *name) {
    device_t *dev = get_dev_by_name(name);
    devs[dev->id] = 0;
}
