/**
 * @file device.c
 * @brief Block-device registry — maps mount names ("fda", "hda") and numeric
 *        ids to registered @ref device_t entries.
 */
#include <device.h>
#include <lib/string.h>
#include <vfs.h>

/** Registered devices, indexed by @c device_t::id (ATA takes 0-3, floppies 4-5). */
static device_t *devices[8];

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
    if(name[0] == '/')
        name++;
    for(int i = 0; i < 8; i++) {
        if(devices[i] != NULL && strncmp(devices[i]->mount, name, 3) == 0)
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
    if(name[0] == '/')
        name++;
    for(int i = 0; i < 8; i++) {
        if(devices[i] != NULL && strncmp(devices[i]->mount, name, 3) == 0)
            return devices[i]->id;
    }
    return -1;
}

