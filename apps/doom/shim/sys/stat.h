/*
 * <sys/stat.h> for the koppi-os Doom port.
 *
 * m_misc.c calls mkdir() to create the save-game directory. The FAT driver
 * has no mkdir, so the port keeps its save games in a flat directory that
 * already exists (see doomgeneric_koppi.c) and this reports success.
 */
#ifndef DOOM_SHIM_SYS_STAT_H
#define DOOM_SHIM_SYS_STAT_H

#include <sys/types.h>

static inline int mkdir(const char *path, mode_t mode) {
    (void) path; (void) mode;
    return 0;
}

#endif
