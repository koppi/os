/**
 * @file video.c
 * @brief Linear-framebuffer graphics.
 *
 * All drawing goes to one 32-bpp software surface (the "shadow"). @ref
 * fb_present copies a region of it to the real framebuffer, converting to the
 * hardware pixel format (24- or 32-bpp, any channel order, any pitch) on the
 * way. This keeps the SSFN text renderer -- which only does 32-bit pixel
 * writes -- correct on a 24-bpp VBE mode.
 *
 * @ref fbcon is a bare text console rendered the same way: the only place boot
 * progress or a panic shows on a machine with no serial port (a ThinkPad).
 */
#include <video.h>


#include <graphics.h>

#include <paging.h>
#include <lib/string.h>
#include <bfb.h>
#include <log.h>
#include <pat.h>
#include <io.h>
#include <spinlock.h>
#include <bootdiag.h>

#define SSFN_NOIMPLEMENTATION
#define SSFN_CONSOLEBITMAP_TRUECOLOR
#include "ssfn.h"

struct vbe_mem vbemem;

extern ssfn_font_t _binary_unifont_sfn_start;

/* Hardware framebuffer parameters (the shadow is always 32-bpp; vbemem.pitch
 * is the *shadow* pitch so the draw primitives need no special-casing).
 * fb_base is mapped write-combining (or uncached, no PAT) so a plain pointer
 * is fine -- but WC writes are weakly ordered, hence the sfence in fb_present. */
static uint8_t *fb_base;
static uint32_t fb_pitch;
static uint32_t fb_bpp;
static uint32_t fb_bytespp = 4;
/* Channel shifts into a native pixel (default BGRX, the common GOP layout). */
static uint32_t px_rsh = 16, px_gsh = 8, px_bsh = 0;

/* A back buffer was allocated (vs. drawing straight to the hardware fb). */
static int have_shadow_flag;

/* Present hook: when set (a virtio-gpu backend), fb_present() hands the
 * finished region to it instead of copying to the linear framebuffer. */
void (*video_present_hook)(int x, int y, int w, int h);

/* Serialises a present against a runtime mode change. Only contended once a
 * present hook is installed, so the plain copy path stays lock-free. */
static spinlock_t fb_lock = SPINLOCK_INIT;
static uint32_t   fb_lock_if;
void video_lock(void)   { fb_lock_if = spin_lock(&fb_lock); }
void video_unlock(void) { spin_unlock(&fb_lock, fb_lock_if); }
int  video_has_shadow(void) { return have_shadow_flag; }

/** @brief Pack an 0x00RRGGBB colour into the framebuffer's native pixel. */
static inline uint32_t pack_color(uint32_t c) {
    return (((c >> 16) & 0xFF) << px_rsh) |
           (((c >> 8)  & 0xFF) << px_gsh) |
           (( c        & 0xFF) << px_bsh);
}

/** @return non-zero if the hardware framebuffer is mapped in the current CR3. */
static int fb_reachable(void) {
    return fb_base &&
           get_phys_addr(get_page_directory(), (uint32_t) fb_base) != 0;
}

/**
 * @brief Copy a rectangle of the 32-bpp shadow to the hardware framebuffer.
 *        Clipped to the screen; converts pixel format and pitch.
 */
static void fb_present(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > vbemem.xres) w = vbemem.xres - x;
    if (y + h > vbemem.yres) h = vbemem.yres - y;
    if (w <= 0 || h <= 0)
        return;

    /* A runtime-resolution backend (virtio-gpu) presents its own way; the
     * lock keeps the region consistent with a concurrent video_set_geometry. */
    if (video_present_hook) {
        video_lock();
        video_present_hook(x, y, w, h);
        video_unlock();
        return;
    }

    if (!fb_reachable() || vbemem.buffer == (uint32_t *) fb_base)
        return;

    for (int row = 0; row < h; row++) {
        const uint32_t *src = (const uint32_t *)
            ((const uint8_t *) vbemem.buffer + (uint32_t)(y + row) * vbemem.pitch) + x;
        uint8_t *dst = (uint8_t *) fb_base + (uint32_t)(y + row) * fb_pitch
                       + (uint32_t) x * fb_bytespp;
        if (fb_bytespp == 4) {
            memcpy(dst, (void *) src, (size_t) w * 4);
        } else {
            for (int i = 0; i < w; i++, dst += 3) {
                uint32_t p = src[i];
                dst[0] = p & 0xFF; dst[1] = (p >> 8) & 0xFF; dst[2] = (p >> 16) & 0xFF;
            }
        }
    }
    /* Drain the write-combining buffers so the pixels reach the panel now
     * (a panic() screen must not linger half-written in a fill buffer). */
    asm volatile("sfence" ::: "memory");
}

/* ------------------------------------------------------------------ *
 *  fbcon — text console                                               *
 * ------------------------------------------------------------------ */
#define FBCON_CW 8
#define FBCON_CH 16
/* Screen rows reserved at the bottom for the boot progress bar + its
 * message, so the kernel-log text console never scrolls into it. */
#define FBCON_RESERVE_BOTTOM 32

static int      fbcon_on;
static uint32_t fbcon_cols, fbcon_rows, fbcon_cx, fbcon_cy;
static uint32_t fbcon_fg = 0xC8C8C8, fbcon_bg = 0x000000;

/** @brief Fill one @ref FBCON_CH-tall band of the shadow with @p colour. */
static void fbcon_fill_row(uint32_t row) {
    uint32_t native = pack_color(fbcon_bg);
    for (uint32_t y = 0; y < FBCON_CH; y++) {
        uint32_t *p = (uint32_t *)
            ((uint8_t *) vbemem.buffer + (row * FBCON_CH + y) * vbemem.pitch);
        for (uint32_t x = 0; x < vbemem.xres; x++)
            p[x] = native;
    }
}

static void fbcon_scroll(void) {
    uint32_t rowbytes = FBCON_CH * vbemem.pitch;
    memmove(vbemem.buffer, (uint8_t *) vbemem.buffer + rowbytes,
            (fbcon_rows - 1) * rowbytes);
    fbcon_fill_row(fbcon_rows - 1);
    fbcon_cy = fbcon_rows - 1;
    fb_present(0, 0, vbemem.xres, vbemem.yres);
}

/**
 * @brief Render one character. Control characters (\n \r \b \t) are handled;
 *        the screen scrolls at the bottom. No-op before @ref vbe_init or when
 *        the framebuffer is unreachable from the running CR3.
 */
void fbcon_putc(char c) {
    if (!fbcon_on || !fb_reachable())
        return;

    switch (c) {
    case '\n': fbcon_cx = 0; fbcon_cy++;               break;
    case '\r': fbcon_cx = 0;                           break;
    case '\t': fbcon_cx = (fbcon_cx + 8) & ~7u;        break;
    case '\b': if (fbcon_cx) fbcon_cx--;               break;
    case '\a':                                         return;
    default:
        if ((unsigned char) c < 0x20) return;
        if (fbcon_cx >= fbcon_cols) { fbcon_cx = 0; fbcon_cy++; }
        if (fbcon_cy >= fbcon_rows) fbcon_scroll();
        ssfn_font      = &_binary_unifont_sfn_start;
        ssfn_dst_ptr   = (uint8_t *) vbemem.buffer;
        ssfn_dst_pitch = vbemem.pitch;
        ssfn_dst_w     = vbemem.xres;
        ssfn_dst_h     = vbemem.yres;
        ssfn_fg        = pack_color(fbcon_fg);
        ssfn_x         = fbcon_cx * FBCON_CW;
        ssfn_y         = fbcon_cy * FBCON_CH;
        ssfn_putc((unsigned char) c);
        fb_present(fbcon_cx * FBCON_CW, fbcon_cy * FBCON_CH, FBCON_CW, FBCON_CH);
        fbcon_cx++;
        return;
    }
    if (fbcon_cy >= fbcon_rows)
        fbcon_scroll();
}

int fbcon_active(void) { return fbcon_on; }

/** @brief Stop / restart kernel-log rendering to the framebuffer. */
void fbcon_suspend(void) { fbcon_on = 0; }
void fbcon_resume(void)  { if (fb_base) fbcon_on = 1; }

static void fbcon_init(void) {
    fbcon_cols = vbemem.xres / FBCON_CW;
    fbcon_rows = (vbemem.yres - FBCON_RESERVE_BOTTOM) / FBCON_CH;
    fbcon_cx = fbcon_cy = 0;
    for (uint32_t r = 0; r < fbcon_rows; r++)
        fbcon_fill_row(r);
    fbcon_on = 1;
    fb_present(0, 0, vbemem.xres, vbemem.yres);
}

/* ------------------------------------------------------------------ *
 *  Framebuffer bring-up                                               *
 * ------------------------------------------------------------------ */

/** @brief Find a free @p pages-long run of kernel virtual space (kern_dir). */
static uint32_t kernel_va_hole(uint32_t pages) {
    /* 0x40000000..0xC0000000: above every process image / heap / the relocated
     * RAM disk, below where firmware puts MMIO and the framebuffer itself. */
    page_dir_t *kd = get_kern_directory();
    for (uint32_t va = 0x40000000; va < 0xC0000000; va += PAGE_SIZE) {
        uint32_t run = 0;
        while (run < pages) {
            uint32_t p = va + run * PAGE_SIZE;
            if (kd[p >> 22] && get_phys_addr(kd, p))
                break;
            run++;
        }
        if (run == pages)
            return va;
        va += run * PAGE_SIZE;
    }
    return 0;
}

void vbe_init() {
    if (!bfb_addr || !bfb_width || !bfb_height) {
        klogf(LOG_WARNING, "vbe: no usable framebuffer from the loader\n");
        return;
    }

    fb_bpp     = bfb_bpp ? bfb_bpp : 32;
    fb_bytespp = (fb_bpp + 7) / 8;
    if (fb_bytespp < 3) fb_bytespp = 4;      /* 15/16-bpp: treated as 32, garbled */
    fb_pitch   = bfb_scanline ? bfb_scanline : bfb_width * fb_bytespp;
    uint32_t fb_span = fb_pitch * bfb_height;
    bfb_span = fb_span;                      /* so create_address_space() clones it */

    if (bfb_red_size && bfb_blue_size) {     /* honour the loader's channel layout */
        px_rsh = bfb_red_pos;
        px_gsh = bfb_green_pos;
        px_bsh = bfb_blue_pos;
    }

    klogf(LOG_INFO, "vbe: fb 0x%x %ux%u %u bpp, pitch %u, %u KiB, rgb %u/%u/%u\n",
          (uint32_t) bfb_addr, bfb_width, bfb_height, fb_bpp, fb_pitch,
          fb_span / 1024, px_rsh, px_gsh, px_bsh);

    /* 1. Map the framebuffer 1:1. Write-combining if pat_init() gave us a WC
     *    PAT slot -- the CPU then bursts a whole cache line of pixels per
     *    transaction instead of one uncached word at a time, which is the
     *    difference between a fluid and a crawling desktop on real hardware
     *    (the X250's framebuffer is across the display link, not host RAM).
     *    Fall back to strong-uncacheable where PAT is unavailable. */
    int wc = pat_available();
    uint32_t fb_flags = PAGE_PRESENT | PAGE_RW |
                        (wc ? PAGE_WC : (PAGE_PCD | PAGE_PWT));
    uint32_t fb_pa = (uint32_t) bfb_addr;
    for (uint32_t off = 0; off < fb_span + PAGE_SIZE; off += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), fb_pa + off, fb_pa + off, fb_flags);
    fb_base = (uint8_t *) fb_pa;
    klogf(LOG_INFO, "vbe: framebuffer mapped %s\n",
          wc ? "write-combining" : "uncached");
    klogf(LOG_INFO, "vbe: DBG kern_dir=%x pa(fb)=%x\n",
          (uint32_t) get_kern_directory(),
          (uint32_t) get_phys_addr(get_kern_directory(), fb_pa));

    /* 2. The 32-bpp shadow surface. All drawing targets this; fb_present()
     *    converts it to the hardware format. Scattered frames, contiguous VA. */
    uint32_t sh_pitch = bfb_width * 4;
    uint32_t sh_span  = sh_pitch * bfb_height;
    uint32_t npages   = (sh_span + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t shadow   = kernel_va_hole(npages);
    int have_shadow = 0;
    if (shadow) {
        have_shadow = 1;
        for (uint32_t i = 0; i < npages; i++)
            if (!vmm_map(get_kern_directory(), shadow + i * PAGE_SIZE,
                         PAGE_PRESENT | PAGE_RW)) { have_shadow = 0; break; }
    }

    vbemem.mem         = (uint32_t *) fb_pa;
    vbemem.xres        = bfb_width;
    vbemem.yres        = bfb_height;
    vbemem.bpp         = 32;
    if (have_shadow) {
        vbemem.buffer      = (uint32_t *) shadow;
        vbemem.pitch       = sh_pitch;
        vbemem.buffer_size = sh_span;
        have_shadow_flag   = 1;
        /* Reachable from every address space so a panic while a user process
         * is current can still repaint (also covers the virtio-gpu path). */
        vmm_share_kernel_range(shadow, sh_span);
    } else {
        /* Low RAM: draw straight to the hardware fb (24-bpp text will tint). */
        klogf(LOG_WARNING, "vbe: no back buffer (low RAM), compositing direct\n");
        vbemem.buffer      = (uint32_t *) fb_pa;
        vbemem.pitch       = fb_pitch;
        vbemem.buffer_size = fb_span;
        vbemem.bpp         = fb_bpp;
    }

    fbcon_init();
}

/**
 * @brief Enlarge the 32-bpp back buffer in place so it can hold any mode up to
 *        @p max_w x @p max_h. Called once, before the compositor starts, by a
 *        display backend that changes resolution at runtime -- so no locking.
 * @return 1 on success (or if it is already big enough), 0 on failure.
 */
int video_grow_shadow(uint32_t max_w, uint32_t max_h) {
    if (!have_shadow_flag)
        return 0;

    uint32_t need = max_w * 4u * max_h;
    if (need <= vbemem.buffer_size)
        return 1;

    uint32_t npages = (need + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t nva    = kernel_va_hole(npages);
    if (!nva) {
        klogf(LOG_WARNING, "vbe: no VA hole to grow the back buffer to %ux%u\n",
              max_w, max_h);
        return 0;
    }
    for (uint32_t i = 0; i < npages; i++) {
        if (!vmm_map(get_kern_directory(), nva + i * PAGE_SIZE,
                     PAGE_PRESENT | PAGE_RW)) {
            klogf(LOG_WARNING, "vbe: back-buffer grow ran out of frames\n");
            for (uint32_t j = 0; j < i; j++)
                vmm_unmap(get_kern_directory(), nva + j * PAGE_SIZE);
            return 0;
        }
    }

    uint32_t old_va = (uint32_t) vbemem.buffer;
    uint32_t old_sz = vbemem.buffer_size;
    memcpy((void *) nva, (void *) old_va, old_sz);
    memset((uint8_t *) nva + old_sz, 0, need - old_sz);

    vbemem.buffer      = (uint32_t *) nva;
    vbemem.buffer_size = npages * PAGE_SIZE;

    for (uint32_t i = 0; i < (old_sz + PAGE_SIZE - 1) / PAGE_SIZE; i++)
        vmm_unmap(get_kern_directory(), old_va + i * PAGE_SIZE);

    vmm_share_kernel_range(nva, need);
    klogf(LOG_INFO, "vbe: back buffer %u KiB (fits %ux%u)\n",
          need / 1024, max_w, max_h);
    return 1;
}

/**
 * @brief Switch the logical desktop to @p w x @p h without reallocating: the
 *        back buffer must already be large enough (see @ref video_grow_shadow).
 *        Wipes it, so the compositor repaints from scratch next frame. Held
 *        under @ref video_lock by the caller.
 */
void video_set_geometry(uint32_t w, uint32_t h) {
    if (w < 320) w = 320;
    if (h < 200) h = 200;

    uint32_t need = w * 4u * h;
    if (need > vbemem.buffer_size) {
        klogf(LOG_WARNING, "vbe: mode %ux%u does not fit the back buffer\n", w, h);
        return;
    }

    vbemem.xres  = (uint16_t) w;
    vbemem.yres  = (uint16_t) h;
    vbemem.pitch = (uint16_t) (w * 4u);
    memset(vbemem.buffer, 0, need);

    fbcon_cols = w / FBCON_CW;
    fbcon_rows = (h - FBCON_RESERVE_BOTTOM) / FBCON_CH;
    if (fbcon_cx >= fbcon_cols) fbcon_cx = fbcon_cols ? fbcon_cols - 1 : 0;
    if (fbcon_cy >= fbcon_rows) fbcon_cy = fbcon_rows ? fbcon_rows - 1 : 0;
}

void refresh_screen() {
    /* Text-mode boot: nothing to composite; parking here also keeps
     * paint_desktop() from racing the console for the keyboard ring. */
    if (!bfb_addr) {
        for (;;)
            halt();
    }

    /* The compositor owns the screen now; boot log continues in the desktop's
     * console window (write_log), so stop doubling it onto the framebuffer. */
    fbcon_suspend();

    for (;;) {
        paint_desktop();
        fb_present(0, 0, vbemem.xres, vbemem.yres);
        /* Cap the compositor to ~60 fps. Left unthrottled it redraws the whole
         * desktop and re-blits the entire screen as fast as the memory bus
         * allows, pinning a core and saturating the framebuffer link for no
         * visible benefit. */
        sleep(16);
    }
}

/* ------------------------------------------------------------------ *
 *  Drawing primitives (all write the 32-bpp shadow)                   *
 * ------------------------------------------------------------------ */
void draw_pixel(int x, int y, uint32_t color) {
    if (!bfb_addr) return;
    if (x < 0 || x >= vbemem.xres || y < 0 || y >= vbemem.yres)
        return;
    uint32_t *p = (uint32_t *)
        ((uint8_t *) vbemem.buffer + (uint32_t) y * vbemem.pitch) + x;
    *p = (vbemem.buffer == (uint32_t *) fb_base) ? pack_color(color) : color;
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (!bfb_addr) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= vbemem.xres || y >= vbemem.yres || w <= 0 || h <= 0)
        return;
    if (x + w > vbemem.xres) w = vbemem.xres - x;
    if (y + h > vbemem.yres) h = vbemem.yres - y;

    uint32_t v = (vbemem.buffer == (uint32_t *) fb_base) ? pack_color(color) : color;
    for (int i = 0; i < h; i++) {
        uint32_t *d = (uint32_t *)
            ((uint8_t *) vbemem.buffer + (uint32_t)(y + i) * vbemem.pitch) + x;
        for (int j = 0; j < w; j++)
            d[j] = v;
    }
}

void draw_string(uint32_t x, uint32_t y, const char *text, uint32_t color) {
    if (!bfb_addr) return;

    uint32_t startx = x;
    ssfn_font = &_binary_unifont_sfn_start;
    ssfn_dst_ptr = (uint8_t *) vbemem.buffer;
    ssfn_dst_pitch = vbemem.pitch;
    ssfn_dst_w = vbemem.xres;
    ssfn_dst_h = vbemem.yres;
    ssfn_fg = (vbemem.buffer == (uint32_t *) fb_base) ? pack_color(color) : color;

    while (*text) {
        switch (*text) {
        case '\n': y += FBCON_CH; x = startx; break;
        case ' ':  x += FBCON_CW;             break;
        case '\r': case '\b':                 break;
        default:
            if (x + FBCON_CW > vbemem.xres || y + FBCON_CH > vbemem.yres)
                break;
            ssfn_x = x; ssfn_y = y;
            ssfn_putc((unsigned char) *text);
            x += FBCON_CW;
            break;
        }
        text++;
    }
}

void draw_data_with_alfa(uint32_t* data, uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
    if (!bfb_addr) return;
    for (uint32_t j = 0; j < height; j++)
        for (uint32_t i = 0; i < width; i++)
            if (data[j * width + i] & 0xFF000000)
                draw_pixel(x + i, y + j, data[j * width + i]);
}

/** @brief Integer absolute value (used by @ref draw_line). */
int abs(int a) { return (a >= 0) ? a : -a; }

void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    if (!bfb_addr) return;

    int deltax = abs(x1 - x0);
    int deltay = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int error = deltax - deltay;
    while (1) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * error;
        if (e2 > -deltay) { error -= deltay; x0 += sx; }
        if (e2 < deltax)  { error += deltax; y0 += sy; }
    }
}

/* ------------------------------------------------------------------ *
 *  Boot-time progress bar                                             *
 * ------------------------------------------------------------------ */
/* Bar colours in the shadow's native 0x00RRGGBB format. */
#define BOOTP_TRACK 0x00404040
#define BOOTP_FILL  0x0033CC33
#define BOOTP_TEXT  0x00FFFFFF

static int bootp_paging;    /* paging is on (direct physical writes unsafe) */

/** @brief Record that paging is now enabled. Until @ref vbe_init maps the
 *         framebuffer the physical fb can be unmapped, so @ref boot_progress
 *         stops painting to it for that brief window. */
void boot_progress_paging_on(void) { bootp_paging = 1; }

/** Row cursor for @ref bootdiag_text (advances down the screen). */
static uint32_t bootdiag_text_y;

/** @brief Render @p text into the raw boot framebuffer (pre-vbe, 24/32-bpp).
 *         Reuses the SSFN set-up used by fbcon, against the physical fb. */
static void bootp_phys_string(const char *text, uint32_t y) {
    uint32_t pitch = bfb_scanline ? bfb_scanline : bfb_width * 4;
    ssfn_font      = &_binary_unifont_sfn_start;
    ssfn_dst_ptr   = (uint8_t *)(uintptr_t) bfb_addr;
    ssfn_dst_pitch = pitch;
    ssfn_dst_w     = bfb_width;
    ssfn_dst_h     = bfb_height;
    ssfn_fg        = pack_color(BOOTP_TEXT);
    ssfn_x         = 8;
    ssfn_y         = y;
    for (const char *p = text; *p; p++) {
        if (ssfn_x + FBCON_CW <= bfb_width && ssfn_y + FBCON_CH <= bfb_height)
            ssfn_putc((unsigned char) *p);
        ssfn_x += FBCON_CW;
    }
}

/** @brief Label a boot sub-step on the raw framebuffer (white text at the next
 *         free row from the top), for when `bootdiag` is on. */
void bootdiag_text(const char *text) {
    if (!bfb_addr || bfb_bpp < 24)
        return;
    bootp_phys_string(text, bootdiag_text_y);
    bootdiag_text_y += FBCON_CH;
}

void boot_progress(uint32_t cur, uint32_t total, const char *msg) {
    klogf(LOG_INFO, "DBG boot_progress: cur=%u total=%u msg=%s vbemem.buffer=%p fb_base=%p\n", cur, total, msg, (void *)vbemem.buffer, (void *)fb_base);
    if (!bfb_addr)
        return;

    /* Post-vbe: draw into the shadow surface and present the strip. */
    if (vbemem.buffer) {
        const uint32_t bar_h = 9;
        const uint32_t w    = vbemem.xres;
        const uint32_t y    = vbemem.yres - bar_h;
        if (y < FBCON_CH)
            return;
        uint32_t fill = (total > 0) ? (uint64_t) w * cur / total : 0;
        if (fill > w) fill = w;

        /* Clear one text row + the bar, then redraw. */
        draw_rect(0, y - FBCON_CH, w, FBCON_CH + bar_h, 0x000000);
        draw_rect(0, y, w, bar_h, BOOTP_TRACK);
        if (fill)
            draw_rect(0, y, fill, bar_h, BOOTP_FILL);
        draw_string(8, y - FBCON_CH, msg, BOOTP_TEXT);
        fb_present(0, y - FBCON_CH, w, FBCON_CH + bar_h);
        return;
    }

    /* Pre-vbe: paint straight into the boot framebuffer (24/32-bpp) while
     * paging is still off. Once paging is on and before vbe_init the fb may
     * be unmapped, so draw nothing -- the bar freezes, then resumes under the
     * shadow path above (fbcon_init clears the screen, so it repaints). */
    if (!bootp_paging && bfb_bpp >= 24 && bfb_height >= FBCON_CH + 9) {
        const uint8_t bytespp = bfb_bpp >= 32 ? 4 : 3;
        const uint32_t pitch = bfb_scanline ? bfb_scanline : bfb_width * bytespp;
        volatile uint8_t *fb = (volatile uint8_t *)(uintptr_t) bfb_addr;
        const uint32_t bar_h = 9;
        const uint32_t y0    = bfb_height - bar_h;
        uint32_t fill = (total > 0) ? (uint64_t) bfb_width * cur / total : 0;
        if (fill > bfb_width) fill = bfb_width;

        /* Clear the message row + track; draw the green fill across the bar. */
        for (uint32_t y = y0 - FBCON_CH; y < bfb_height; y++) {
            for (uint32_t x = 0; x < bfb_width; x++) {
                uint32_t color =
                    (y >= y0 && x < fill) ? BOOTP_FILL : 0x000000;
                volatile uint8_t *p = fb + y * pitch + x * bytespp;
                if (bytespp == 4)
                    *(volatile uint32_t *)p = color;
                else {
                    p[0] = color & 0xFF;
                    p[1] = (color >> 8) & 0xFF;
                    p[2] = (color >> 16) & 0xFF;
                }
            }
        }
        bootp_phys_string(msg, y0 - FBCON_CH);
    }
}
