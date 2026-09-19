/*
 * WAD backend for koppi-os: the whole file, in memory.
 *
 * w_file.c picks a wad_file_class_t to read lumps through; upstream ships a
 * stdio one that fseek()s to each lump. That is the wrong shape here -- the
 * kernel VFS reads a FAT chain forward, 512 bytes per syscall, with no seek
 * -- so this class reads the file once at open and hands w_wad.c the buffer
 * as `mapped`, which is the path the engine already has for an mmap()ed WAD.
 * Lumps are then pointers into that buffer and are never copied again, so the
 * 4 MiB the shareware IWAD costs is the whole cost: it replaces, rather than
 * adds to, what the zone would otherwise hold.
 *
 * The class keeps the name `stdc_wad_file` so w_file.c stays unmodified.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

typedef struct {
    wad_file_t wad;
    void      *data;
} mem_wad_file_t;

extern wad_file_class_t stdc_wad_file;

static wad_file_t *W_Mem_OpenFile(char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;

    size_t len = 0;
    void *data = __koppi_steal(f, &len);   /* takes fopen()'s buffer, closes f */
    if (data == NULL)
        return NULL;

    mem_wad_file_t *result = Z_Malloc(sizeof(mem_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &stdc_wad_file;
    result->wad.mapped     = data;
    result->wad.length     = (unsigned int) len;
    result->data           = data;

    return &result->wad;
}

static void W_Mem_CloseFile(wad_file_t *wad) {
    mem_wad_file_t *mw = (mem_wad_file_t *) wad;
    free(mw->data);
    Z_Free(mw);
}

static size_t W_Mem_Read(wad_file_t *wad, unsigned int offset,
                         void *buffer, size_t buffer_len) {
    mem_wad_file_t *mw = (mem_wad_file_t *) wad;

    if (offset >= wad->length)
        return 0;
    if (offset + buffer_len > wad->length)
        buffer_len = wad->length - offset;

    memcpy(buffer, (unsigned char *) mw->data + offset, buffer_len);
    return buffer_len;
}

wad_file_class_t stdc_wad_file = {
    W_Mem_OpenFile,
    W_Mem_CloseFile,
    W_Mem_Read,
};
