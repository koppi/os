/**
 * @file bmp.c
 * @brief Minimal BMP loader — reads the header, validates it, and copies the
 *        whole file into a kmalloc'd buffer with @c data pointing at the pixels.
 */
#include <bmp.h>
#include <vfs.h>
#include <kheap.h>
#include <printf.h>
#include <lib/string.h>

/** @brief Load @p filename via the VFS. @return The image, or NULL on error. */
bmp_image_t* bmp_image_from_file(char* filename) {
    char buff[512];
    struct bmp_image hdr;
    bmp_image_t* bmp;

    file* fd = vfs_file_open(filename, "r");
    if(fd->type != FS_FILE) {
        printf("bmp_image_from_file(): %s not found\n", filename);
        return NULL;
    }

    vfs_file_read(fd, buff);
    vfs_file_close(fd);

    memcpy(&hdr, buff, sizeof(struct bmp_image));

    if(hdr.signature != 0x4D42 || hdr.total_size < hdr.offset ||
       hdr.total_size > (1u << 20)) {
        printf("bmp_image_from_file(): %s is not a usable BMP\n", filename);
        return NULL;
    }

    bmp = (bmp_image_t*)kmalloc(hdr.total_size);
    if(bmp == NULL) {
        printf("bmp_image_from_file(): out of memory\n");
        return NULL;
    }

    // Copy the whole file in, clamping the final sector to the allocation.
    fd = vfs_file_open(filename, "r");
    uint32_t j = 0;
    while(fd->eof != 1 && j < hdr.total_size) {
        char buf[512];
        vfs_file_read(fd, buf);
        uint32_t n = hdr.total_size - j;
        if(n > 512)
            n = 512;
        memcpy((uint8_t*)((uint32_t)bmp + j), buf, n);
        j += 512;
    }
    vfs_file_close(fd);

    bmp->total_size = hdr.total_size;
    bmp->offset = hdr.offset;
    bmp->width = hdr.width;
    bmp->height = hdr.height;
    bmp->bpp = hdr.bpp;
    bmp->data = (uint8_t*)((uint32_t)bmp + hdr.offset);

    return bmp;
}
