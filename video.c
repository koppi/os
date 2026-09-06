/**
 * @file video.c
 * @brief Linear-framebuffer graphics: maps the boot framebuffer, keeps a
 *        software back buffer, and provides pixel/line/rect/text/blit
 *        primitives plus the per-frame @ref refresh_screen swap.
 *
 * Also hosts @ref fbcon — a bare text console that renders straight to the
 * visible framebuffer with no heap and no scheduler. It is the only way to
 * see boot progress or a panic on a machine with no serial port (e.g. a
 * ThinkPad): kernel log output is fanned out to it by kconsole.c.
 */
#include <video.h>

#include <stdlib.h>
#include <graphics.h>
#include <memory.h>
#include <paging.h>
#include <lib/string.h>
#include <bfb.h>
#include <log.h>

#define SSFN_NOIMPLEMENTATION
#define SSFN_CONSOLEBITMAP_TRUECOLOR
#include "ssfn.h"

struct vbe_mem vbemem;

extern ssfn_font_t _binary_unifont_sfn_start;

/* Channel shifts into a native 32-bpp pixel (default: BGRX, the common GOP
 * layout). Overridden from the multiboot RGB field positions when given. */
static uint32_t px_rsh = 16, px_gsh = 8, px_bsh = 0;
static uint32_t fb_bytespp = 4;

/** @brief Pack an 0x00RRGGBB colour into the framebuffer's native pixel. */
static inline uint32_t pack_color(uint32_t c) {
    return (((c >> 16) & 0xFF) << px_rsh) |
           (((c >> 8)  & 0xFF) << px_gsh) |
           (( c        & 0xFF) << px_bsh);
}

/* ------------------------------------------------------------------ *
 *  fbcon — direct-to-framebuffer text console                         *
 * ------------------------------------------------------------------ */
#define FBCON_CW 8
#define FBCON_CH 16

static int      fbcon_on;
static uint32_t fbcon_cols, fbcon_rows, fbcon_cx, fbcon_cy;
static uint32_t fbcon_fg = 0xC8C8C8, fbcon_bg = 0x000000;

/** @return non-zero if the visible framebuffer is mapped in the current CR3. */
static int fb_reachable(void) {
    return vbemem.mem &&
           get_phys_addr(get_page_directory(), (uint32_t) vbemem.mem) != 0;
}

/** @brief Paint one @ref FBCON_CH-tall band of the visible fb to @p colour. */
static void fbcon_fill_row(uint32_t row, uint32_t colour) {
    uint32_t native = pack_color(colour);
    uint8_t *base = (uint8_t *) vbemem.mem + row * FBCON_CH * vbemem.pitch;
    for (uint32_t y = 0; y < FBCON_CH; y++) {
        uint8_t *p = base + y * vbemem.pitch;
        for (uint32_t x = 0; x < vbemem.xres; x++, p += fb_bytespp) {
            if (fb_bytespp == 4) {
                *(uint32_t *) p = native;
            } else {
                p[0] = native & 0xFF; p[1] = (native >> 8) & 0xFF;
                p[2] = (native >> 16) & 0xFF;
            }
        }
    }
}

static void fbcon_scroll(void) {
    uint32_t rowbytes = FBCON_CH * vbemem.pitch;
    memmove(vbemem.mem, (uint8_t *) vbemem.mem + rowbytes,
            (fbcon_rows - 1) * rowbytes);
    fbcon_fill_row(fbcon_rows - 1, fbcon_bg);
    fbcon_cy = fbcon_rows - 1;
}

/**
 * @brief Render one character to the visible framebuffer. Control characters
 *        (\n \r \b \t) are handled; the screen scrolls at the bottom.
 *
 * Silently does nothing until @ref fbcon_init has run or when the framebuffer
 * is not mapped in the running address space (a user process's CR3) — the
 * caller still gets the byte on every other console sink.
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
        ssfn_dst_ptr   = (uint8_t *) vbemem.mem;
        ssfn_dst_pitch = vbemem.pitch;
        ssfn_dst_w     = vbemem.xres;
        ssfn_dst_h     = vbemem.yres;
        ssfn_fg        = pack_color(fbcon_fg);
        ssfn_x         = fbcon_cx * FBCON_CW;
        ssfn_y         = fbcon_cy * FBCON_CH;
        ssfn_putc((unsigned char) c);
        fbcon_cx++;
        return;
    }
    if (fbcon_cy >= fbcon_rows)
        fbcon_scroll();
}

/** @return non-zero once @ref fbcon_init has set up a usable framebuffer. */
int fbcon_active(void) { return fbcon_on; }

/** @brief Stop rendering kernel log to the framebuffer (the desktop compositor
 *         has taken over the screen; the log lives in its on-screen window now).
 *         A later panic() re-enables it so the fault is still visible. */
void fbcon_suspend(void) { fbcon_on = 0; }
void fbcon_resume(void)  { if (vbemem.mem) fbcon_on = 1; }

/** @brief Set up fbcon over the framebuffer already recorded in @ref vbemem. */
static void fbcon_init(void) {
    ssfn_font = &_binary_unifont_sfn_start;
    fbcon_cols = vbemem.xres / FBCON_CW;
    fbcon_rows = vbemem.yres / FBCON_CH;
    fbcon_cx = fbcon_cy = 0;
    for (uint32_t r = 0; r < fbcon_rows; r++)
        fbcon_fill_row(r, fbcon_bg);
    fbcon_on = 1;
}

/* ------------------------------------------------------------------ *
 *  Framebuffer bring-up                                               *
 * ------------------------------------------------------------------ */

/** @brief Find a free @p pages-long run of kernel virtual space (kern_dir). */
static uint32_t kernel_va_hole(uint32_t pages) {
    /* Search 0x40000000..0xC0000000: above every process image / heap / the
     * relocated RAM disk, below where firmware puts MMIO and the framebuffer
     * itself on real hardware. */
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
        va += run * PAGE_SIZE;   /* skip the blocked span */
    }
    return 0;
}

void vbe_init() {
    if (!bfb_addr || !bfb_width || !bfb_height) {
        klogf(LOG_WARNING, "vbe: no usable framebuffer from the loader\n");
        return;
    }

    uint32_t bpp   = bfb_bpp ? bfb_bpp : 32;
    fb_bytespp     = (bpp + 7) / 8;
    if (fb_bytespp < 3) fb_bytespp = 4;          /* we only render 24/32-bpp */
    uint32_t pitch = bfb_scanline ? bfb_scanline : bfb_width * fb_bytespp;
    uint32_t span  = pitch * bfb_height;
    bfb_span = span;   /* so create_address_space() clones the fb mapping */

    if (bfb_red_size && bfb_blue_size) {         /* honour the loader's layout */
        px_rsh = bfb_red_pos;
        px_gsh = bfb_green_pos;
        px_bsh = bfb_blue_pos;
    }

    klogf(LOG_INFO, "vbe: fb 0x%x %ux%u %u bpp, pitch %u, %u KiB, rgb %u/%u/%u\n",
          (uint32_t) bfb_addr, bfb_width, bfb_height, bpp, pitch, span / 1024,
          px_rsh, px_gsh, px_bsh);

    /* 1. Map the framebuffer 1:1, cache-disabled (write-back MMIO would leave
     *    the panel showing stale pixels on real hardware). */
    uint32_t fb_pa = (uint32_t) bfb_addr;
    for (uint32_t off = 0; off < span + PAGE_SIZE; off += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), fb_pa + off, fb_pa + off,
                     PAGE_PRESENT | PAGE_RW | PAGE_PCD | PAGE_PWT);

    vbemem.mem         = (uint32_t *) fb_pa;
    vbemem.xres        = bfb_width;
    vbemem.yres        = bfb_height;
    vbemem.bpp         = bpp;
    vbemem.pitch       = pitch;
    vbemem.buffer_size = span;

    /* 2. Software back buffer: physically scattered frames mapped contiguously
     *    into a free slice of kernel virtual space. If RAM is too tight, fall
     *    back to compositing straight to the framebuffer. */
    uint32_t npages = (span + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t shadow = kernel_va_hole(npages);
    int have_shadow = 0;
    if (shadow) {
        have_shadow = 1;
        for (uint32_t i = 0; i < npages; i++) {
            if (!vmm_map(get_kern_directory(), shadow + i * PAGE_SIZE,
                         PAGE_PRESENT | PAGE_RW)) {
                have_shadow = 0;
                break;
            }
        }
    }
    vbemem.buffer = have_shadow ? (uint32_t *) shadow : vbemem.mem;
    if (!have_shadow)
        klogf(LOG_WARNING, "vbe: no back buffer (low RAM), compositing direct\n");

    /* 3. Text console for boot messages / panics. */
    fbcon_init();

    ssfn_font = &_binary_unifont_sfn_start;
    ssfn_dst_ptr = (uint8_t *) vbemem.buffer;
    ssfn_dst_pitch = vbemem.pitch;
    ssfn_fg = 0xFFFF00;
    ssfn_x = 0;
    ssfn_y = 0;
}

void refresh_screen() {
    /* Console/text boot: there is no framebuffer to composite, and
     * paint_desktop() polls the shared keyboard ring - running it here would
     * race the console for every keystroke. Park the thread instead. */
    if (!bfb_addr) {
        for (;;)
            halt();
    }

    /* The compositor owns the screen now; boot log continues in the desktop's
     * console window (write_log), so stop doubling it onto the framebuffer. */
    fbcon_suspend();

    for (;;) {
        paint_desktop();
        if (vbemem.buffer != vbemem.mem)
            memcpy(vbemem.mem, vbemem.buffer, vbemem.buffer_size);
    }
}

/** @brief Base of the drawing surface (back buffer, or the fb itself). */
static inline uint8_t *surf(void) { return (uint8_t *) vbemem.buffer; }

void draw_pixel(int x, int y, uint32_t color) {
    if (!bfb_addr) return;
    if (x < 0 || x >= vbemem.xres || y < 0 || y >= vbemem.yres)
        return;

    uint8_t *p = surf() + (uint32_t) y * vbemem.pitch + (uint32_t) x * fb_bytespp;
    uint32_t native = pack_color(color);
    if (fb_bytespp == 4) {
        *(uint32_t *) p = native;
    } else {
        p[0] = native & 0xFF; p[1] = (native >> 8) & 0xFF;
        p[2] = (native >> 16) & 0xFF;
    }
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (!bfb_addr) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= vbemem.xres || y >= vbemem.yres || w <= 0 || h <= 0)
        return;
    if (x + w > vbemem.xres) w = vbemem.xres - x;
    if (y + h > vbemem.yres) h = vbemem.yres - y;

    uint32_t native = pack_color(color);
    for (int i = 0; i < h; i++) {
        uint8_t *row = surf() + (uint32_t)(y + i) * vbemem.pitch + (uint32_t) x * fb_bytespp;
        if (fb_bytespp == 4) {
            uint32_t *d = (uint32_t *) row;
            for (int j = 0; j < w; j++)
                d[j] = native;
        } else {
            for (int j = 0; j < w; j++) {
                row[j*3] = native & 0xFF; row[j*3+1] = (native >> 8) & 0xFF;
                row[j*3+2] = (native >> 16) & 0xFF;
            }
        }
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
    ssfn_fg = pack_color(color);

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
