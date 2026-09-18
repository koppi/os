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

/**
 * @name Runtime resolution support (a virtio-gpu backend)
 *
 * A display driver that can change mode at runtime installs @ref
 * video_present_hook -- @ref fb_present then routes the finished frame through
 * it (region transfer + flush) instead of copying to the linear framebuffer.
 * @ref video_grow_shadow enlarges the 32-bpp back buffer once, before the
 * compositor starts, so @ref video_set_geometry can later switch the logical
 * desktop size in place without touching page tables. @ref video_lock brackets
 * a present against a concurrent mode change.
 */
///@{
extern void (*video_present_hook)(int x, int y, int w, int h);
/** @return Non-zero if a 32-bpp back buffer was allocated (not direct-to-fb). */
int  video_has_shadow(void);
/** @brief Grow the back buffer so any mode up to @p max_w x @p max_h fits.
 *         @return 1 on success (or already large enough), 0 on failure. */
int  video_grow_shadow(uint32_t max_w, uint32_t max_h);
/** @brief Set the logical desktop size in place; wipes the back buffer.
 *         @p w * @p h * 4 must fit the buffer @ref video_grow_shadow reserved. */
void video_set_geometry(uint32_t w, uint32_t h);
/** @brief Acquire / release the present lock (held across @ref video_present_hook). */
void video_lock(void);
void video_unlock(void);
///@}

/** @brief Render one char straight to the visible framebuffer (boot / panic).
 *         No-op until @ref vbe_init has run. */
void fbcon_putc(char c);
/** @return non-zero once the framebuffer text console is usable. */
int fbcon_active(void);
/** @brief Stop / restart kernel-log rendering to the framebuffer. The desktop
 *         compositor suspends it; panic() resumes it. */
void fbcon_suspend(void);
void fbcon_resume(void);

/** @brief Draw a green boot progress bar at the bottom of the screen.
 *  @param cur,total Bar fills cur/total of the way across. @p msg is the name
 *         of the subsystem currently initializing, drawn above the bar.
 *  Works before and after @ref vbe_init: before it paints straight into the
 *  boot framebuffer (paging off); after it draws into the shadow and
 *  presents. Call every time a subsystem finishes (see main.c). */
void boot_progress(uint32_t cur, uint32_t total, const char *msg);
/** @brief Note that paging is now enabled. Between paging-on and @ref vbe_init
 *         the framebuffer can be unmapped, so @ref boot_progress stops painting
 *         to the physical fb for that brief window. */
void boot_progress_paging_on(void);
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
