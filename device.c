#include <device.h>
#include <lib/string.h>
#include <vfs.h>

static device_t *devices[8];

void device_register(device_t *dev) {
    if(dev->id < 8) {
        devices[dev->id] = dev;
        vfs_mount(dev->mount);
    }
}

device_t *get_dev_by_name(char *name) {
    if(name[0] == '/')
        name++;
    for(int i = 0; i < 8; i++) {
        if(devices[i] != NULL && strncmp(devices[i]->mount, name, 3) == 0)
            return devices[i];
    }
    return NULL;
}

device_t *get_dev_by_id(int id) {
    for(int i = 0; i < 8; i++) {
        if(devices[i] != NULL && devices[i]->id == id)
            return devices[i];
    }
    return NULL;
}

int get_dev_id_by_name(char *name) {
    if(name[0] == '/')
        name++;
    for(int i = 0; i < 8; i++) {
        if(devices[i] != NULL && strncmp(devices[i]->mount, name, 3) == 0)
            return devices[i]->id;
    }
    return -1;
}

