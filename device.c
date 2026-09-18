/**
 * @file device.c
 * @brief Block-device registry — maps mount names ("fda", "hda") and numeric
 *        ids to registered @ref device_t entries.
 */
#include <device.h>
#include <lib/string.h>
#include <vfs.h>

/** Registered devices, indexed by @c device_t::id (the RAM disk takes 0, ATA
 *  and the floppies fill in after it). */
static device_t *devices[8];

/**
 * @brief Match a mount name against the first path component of @p name.
 *
 * Handles names of any length (the RAM disk is "rd", disks are "hda"/"fda"):
 * the mount name must be followed by '/' or end-of-string in @p name, so "rd"
 * matches "rd" and "rd/zsh" but not "rdx".
 *
 * @param mount NUL-terminated mount name.
 * @param name  Path, optionally with a leading '/'.
 */
static int dev_name_matches(const char *mount, const char *name) {
    if(name[0] == '/')
        name++;
    size_t n = strlen(mount);
    return strncmp((char *) mount, (char *) name, n) == 0 &&
           (name[n] == '/' || name[n] == '\0');
}

/**
 * @brief Record @p dev and mount its filesystem.
 * @param dev Device with @c id < 8; higher ids are ignored.
 */
void device_register(device_t *dev) {
    if(dev->id < 8) {
        devices[dev->id] = dev;
        vfs_mount(dev->mount);
    }
}

/**
 * @brief Find a device whose mount name matches the first 3 chars of @p name.
 * @param name Path, optionally with a leading '/'.
 * @return The device, or NULL.
 */
device_t *get_dev_by_name(char *name) {
    for(int i = 0; i < 8; i++) {
        if(devices[i] != NULL && dev_name_matches(devices[i]->mount, name))
            return devices[i];
    }
    return NULL;
}

/** @brief Find a registered device by numeric id. @return The device, or NULL. */
device_t *get_dev_by_id(int id) {
    for(int i = 0; i < 8; i++) {
        if(devices[i] != NULL && devices[i]->id == id)
            return devices[i];
    }
    return NULL;
}

/**
 * @brief Like @ref get_dev_by_name but returns the id.
 * @return The device id, or -1 if no match.
 */
int get_dev_id_by_name(char *name) {
    for(int i = 0; i < 8; i++) {
        if(devices[i] != NULL && dev_name_matches(devices[i]->mount, name))
            return devices[i]->id;
    }
    return -1;
}

