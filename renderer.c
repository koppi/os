/**
 * @file renderer.c
 * @brief microui rendering back end: translates the toolkit's draw commands
 *        into framebuffer writes, with real clipping.
 *
 * Everything microui draws goes through here, so this is where a window stops
 * being a picture of a window and starts behaving like one. microui emits a
 * @c MU_COMMAND_CLIP whenever the region a later command may touch narrows --
 * a window's body, a scrolling panel, a text run that reaches past its
 * container -- and expects the back end to honour it. This one does: every
 * primitive below intersects with @ref r_clip first, so a scrolled log stays
 * inside its panel and a window that overlaps another cannot paint over the
 * one in front.
 *
 * Text is the awkward case. ssfn draws whole cells into whatever surface it is
 * pointed at and has no clip of its own, so a glyph the clip rectangle cuts
 * through is rendered into an ink mask (@ref draw_char_mask) and plotted a
 * pixel at a time; a glyph wholly inside the clip takes the fast whole-cell
 * path instead. Partial glyphs are rare -- the edges of a scrolling panel --
 * so the slow path almost never runs.
 *
 * @ref r_draw_surface is the window manager's: it paints a ring-3 program's
 * 8-bpp frame through that program's own palette, clipped like everything else,
 * which is what lets a userspace program be a window on this desktop rather
 * than a program that has taken the screen away from it. See wm.c.
 */
#include <renderer.h>

#include <video.h>

/**
 * The clip rectangle. Starts effectively unbounded, which is what microui
 * means by its own "unclipped" rect; the primitives clamp to the screen
 * anyway, so nothing here needs to know the display size.
 */
static mu_Rect r_clip = { 0, 0, 1 << 24, 1 << 24 };

/** @brief Renderer init (nothing to do — the framebuffer is already up). */
void r_init(void) {
    r_clip = mu_rect(0, 0, 1 << 24, 1 << 24);
}

/** Pack 8-bit r/g/b into a 0x00RRGGBB pixel. */
#define RGB(r, g, b) ((((uint32_t) (r)) << 16) | (((uint32_t) (g)) << 8) | ((uint32_t) (b)))

/** @brief The overlap of @p a and @p b (w/h <= 0 when they do not meet). */
static mu_Rect isect(mu_Rect a, mu_Rect b) {
    int x0 = a.x > b.x ? a.x : b.x;
    int y0 = a.y > b.y ? a.y : b.y;
    int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    return mu_rect(x0, y0, x1 - x0, y1 - y0);
}

/** @brief True when @p r lies wholly inside the clip rectangle. */
static int inside_clip(mu_Rect r) {
    return r.x >= r_clip.x && r.y >= r_clip.y &&
           r.x + r.w <= r_clip.x + r_clip.w &&
           r.y + r.h <= r_clip.y + r_clip.h;
}

/** @brief Restrict subsequent drawing to @p rect. */
void r_set_clip_rect(mu_Rect rect) {
    r_clip = rect;
}

/** @brief Fill @p rect with @p color, clipped. */
void r_draw_rect(mu_Rect rect, mu_Color color) {
    rect = isect(rect, r_clip);
    if (rect.w > 0 && rect.h > 0)
        draw_rect(rect.x, rect.y, rect.w, rect.h, RGB(color.r, color.g, color.b));
}

/** @brief Draw @p text at @p pos in @p color, clipped per character cell. */
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
    uint32_t c = RGB(color.r, color.g, color.b);
    int x = pos.x, y = pos.y;

    /* Nothing of this line's band can be visible: skip the whole run rather
     * than testing every cell in it. */
    if (y + VIDEO_CH <= r_clip.y || y >= r_clip.y + r_clip.h)
        return;

    for (; *text; text++, x += VIDEO_CW) {
        if (x + VIDEO_CW <= r_clip.x)
            continue;                       /* still left of the clip */
        if (x >= r_clip.x + r_clip.w)
            return;                         /* and past its right edge: done */
        if (*text == ' ')
            continue;

        mu_Rect cell = mu_rect(x, y, VIDEO_CW, VIDEO_CH);
        if (inside_clip(cell)) {
            draw_char(x, y, *text, c);
            continue;
        }

        /* Cut through by the clip: plot the ink that falls inside it. */
        uint8_t mask[VIDEO_CW * VIDEO_CH];
        draw_char_mask(*text, mask);
        mu_Rect vis = isect(cell, r_clip);
        for (int row = vis.y - y; row < vis.y - y + vis.h; row++)
            for (int col = vis.x - x; col < vis.x - x + vis.w; col++)
                if (mask[row * VIDEO_CW + col])
                    draw_pixel(x + col, y + row, c);
    }
}

/**
 * @brief Draw one of microui's four built-in icons centred in @p rect.
 *
 * Shapes rather than stand-in letters: a title bar's close box and a tree
 * node's arrow are small enough that an 'x' or a 'v' in their place is the
 * first thing that looks wrong about a window.
 */
void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    mu_Color c = color;
    int cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;
    int r = (rect.w < rect.h ? rect.w : rect.h) / 2 - 2;
    if (r < 2)
        r = 2;

    switch (id) {
    case MU_ICON_CLOSE: {
        int d = r * 2 / 3;
        for (int i = -d; i <= d; i++) {
            r_draw_rect(mu_rect(cx + i, cy + i, 2, 2), c);
            r_draw_rect(mu_rect(cx + i, cy - i, 2, 2), c);
        }
        break;
    }
    case MU_ICON_CHECK: {
        /* A tick, not a V: the ascender is twice the descender. */
        int lo = r / 2, hi = r;
        int bx = cx - lo / 2, by = cy + hi / 2;
        for (int i = 0; i <= lo; i++)
            r_draw_rect(mu_rect(bx - lo + i, by - lo + i - 1, 1, 2), c);
        for (int i = 0; i <= hi; i++)
            r_draw_rect(mu_rect(bx + i, by - i - 1, 1, 2), c);
        break;
    }
    case MU_ICON_COLLAPSED:                        /* right-pointing triangle */
        for (int i = 0; i <= r; i++)
            r_draw_rect(mu_rect(cx - r / 2 + i, cy - (r - i), 1, (r - i) * 2 + 1), c);
        break;
    case MU_ICON_EXPANDED:                         /* down-pointing triangle */
        for (int i = 0; i <= r; i++)
            r_draw_rect(mu_rect(cx - (r - i), cy - r / 2 + i, (r - i) * 2 + 1, 1), c);
        break;
    default:
        break;
    }
}

/**
 * @brief Paint a ring-3 program's indexed frame into @p dst, clipped.
 *
 * @p src is @p sw x @p sh bytes of palette indices -- exactly what the program
 * handed over through the blit syscall -- and @p pal is its own 256-entry
 * palette. The surface is drawn 1:1 at @p dst's top-left and cut off at its
 * edges rather than scaled: the window manager sizes a window to its program's
 * surface, so they match, and a resize the program has not followed yet shows
 * as a margin for a frame or two instead of a blurry rescale of the last one.
 *
 * Pixels are written straight into the shadow, a row at a time, because this
 * is the one primitive that runs over a whole window's area every frame.
 */
void r_draw_surface(mu_Rect dst, const uint8_t *src, int sw, int sh,
                    const uint32_t *pal) {
    if (!src || !pal || sw <= 0 || sh <= 0)
        return;

    mu_Rect vis = isect(isect(dst, r_clip), mu_rect(dst.x, dst.y, sw, sh));
    for (int y = vis.y; y < vis.y + vis.h; y++) {
        const uint8_t *row = src + (long) (y - dst.y) * sw + (vis.x - dst.x);
        for (int x = vis.x; x < vis.x + vis.w; x++)
            draw_pixel(x, y, pal[*row++]);
    }
}

/** @brief @return Width of @p len characters. */
int r_get_text_width(const char *text, int len) {
    (void) text;
    return VIDEO_CW * len;
}

/** @brief @return The UI line height. */
int r_get_text_height(void) {
    return VIDEO_CH;
}

/** @brief Clear the surface (no-op; the desktop repaints fully each frame). */
void r_clear(mu_Color clr) {
    (void) clr;
}

/** @brief Present the frame (no-op; video.c owns the buffer swap). */
void r_present(void) {
}
