/**
 * @file video.h
 * @brief Linear-framebuffer graphics: pixel/line/rect primitives, a text
 *        renderer and the double-buffered @ref vbe_mem surface.
 */
#pragma once

#include <types.h>

/** @brief Map the boot framebuffer, allocate the back buffer, start fbcon. */
void vbe_init();
/** @brief Blit the back buffer to the visible framebuffer (one frame). */
void refresh_screen();

/** @brief Render one char straight to the visible framebuffer (boot / panic).
 *         No-op until @ref vbe_init has run. */
void fbcon_putc(char c);
/** @return non-zero once the framebuffer text console is usable. */
int fbcon_active(void);
/** @brief Plot one pixel in the back buffer. */
void draw_pixel(int x, int y, uint32_t color);
/** @brief Fill an axis-aligned rectangle. */
void draw_rect(int x, int y, int w, int h, uint32_t color);
/** @brief Draw @p text at (@p x, @p y) using the built-in SSFN font. */
void draw_string(uint32_t x, uint32_t y, const char *text, uint32_t color);
/**
 * @brief Blit a 32-bpp image, treating a zero alpha byte as transparent.
 * @param data          Pixel array, row-major, @p width * @p height entries.
 * @param width,height  Image size.
 * @param x,y           Top-left destination.
 */
void draw_data_with_alfa(uint32_t* data, uint32_t width, uint32_t height, uint32_t x, uint32_t y);
/** @brief Draw a line with Bresenham's algorithm. */
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);

/** The framebuffer surface plus its software back buffer. */
struct vbe_mem {
    uint32_t buffer_size;   /**< Back-buffer size in bytes. */
    uint32_t *mem;          /**< Visible framebuffer. */
    uint32_t *buffer;       /**< Software back buffer. */
    uint16_t xres;          /**< Width in pixels. */
    uint16_t yres;          /**< Height in pixels. */
    uint8_t bpp;            /**< Bits per pixel. */
    uint16_t pitch;         /**< Bytes per scanline. */
};

extern struct vbe_mem vbemem; /**< The one framebuffer surface. */
