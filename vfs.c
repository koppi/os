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
}

void vfs_ls_dir(char *dir) {
    int device = get_dev_id_by_name(dir);
    if(device >= 0 && devs[device]) {
        int s = fs_enter();
        devs[device]->ls(dir + 1);
        fs_leave(s);
    }
}

int vfs_cd(char *name) {
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
    int device = get_dev_id_by_name(name);
    file *f = kmalloc(sizeof(file));
    f->type = FS_NULL;
    f->dev = 0;   // keep vfs_file_close() in-bounds if the open fails
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
    if(f && devs[f->dev]) {
        int s = fs_enter();
        devs[f->dev]->read(f, str);
        fs_leave(s);
    }
}

void vfs_file_write(file *f, char *str) {
    if(f && devs[f->dev]) {
        int s = fs_enter();
        devs[f->dev]->write(f, str);
        fs_leave(s);
    }
}

void vfs_file_close(file *f) {
    if(f) {
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
    devs[dev->id] = &dev->fs;
    fat_mount(dev);
}

void vfs_unmount(char *name) {
    device_t *dev = get_dev_by_name(name);
    devs[dev->id] = 0;
}
