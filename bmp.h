/**
 * @file bmp.h
 * @brief Minimal 32-bpp BMP image loader for the mouse cursor.
 */
#pragma once

#include <types.h>

#pragma pack(1)
/** The BMP file header (BITMAPFILEHEADER + BITMAPINFOHEADER), plus a resolved
 *  @c data pointer once the image is loaded into memory. */
typedef struct bmp_image {
  uint16_t  signature;    /**< "BM" (0x4D42). */
  uint32_t  total_size;   /**< File size in bytes. */
  uint16_t  reserved1;
  uint16_t  reserved2;
  uint32_t  offset;       /**< Byte offset to the pixel array. */
  uint32_t  hdr_size;     /**< Info-header size. */
  uint32_t  width;        /**< Image width in pixels. */
  uint32_t  height;       /**< Image height in pixels. */
  uint16_t  planes;
  uint16_t  bpp;          /**< Bits per pixel. */
  uint32_t  compression;
  uint32_t  img_size;
  uint32_t  foo[4];
  uint8_t*  data;         /**< Set by the loader: pixels = image base + @c offset. */
} bmp_image_t;

/**
 * @brief Load a BMP from the VFS into a kmalloc'd buffer.
 * @param filename Device-qualified path, e.g. "/fda/mouse.bmp".
 * @return The image, or NULL on error.
 */
bmp_image_t* bmp_image_from_file(char* filename);
