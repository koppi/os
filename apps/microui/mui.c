/**
 * @file apps/microui/mui.c
 * @brief The ring-3 microui runtime: full-screen grab, software renderer,
 *        input pump and frame loop. See mui.h for the interface.
 *
 * The kernel presents a grabbed screen as 8-bpp indexed pixels through a
 * 256-entry palette (video.h), so this is a paletted renderer. Rather than
 * fix a colour cube and quantize into it -- which is what the Qt port must do,
 * because it is handed finished true-colour pictures -- the palette is built
 * from the drawing calls themselves: every colour a frame asks for is interned
 * into the next free slot, and the palette is uploaded just before the blit.
 * A microui frame uses on the order of a dozen distinct colours, so they all
 * land exactly, with no quantization error at all. A frame that somehow wants
 * more than 256 falls back to the closest slot already taken.
 *
 * The palette is rebuilt per frame (slot 0 is always the clear colour, so the
 * memset that clears the surface is also the background fill). That costs one
 * extra syscall and a 1 KiB copy per frame and means the indices in the
 * surface can never refer to a palette the kernel no longer has.
 */
#include <io.h>            /* the shim/ one: declares halt() -- see shim/io.h */
#include <stdlib.h>        /* the shim/ one too: declares qsort() */
#include <lib/string.h>

#include "mui.h"
#include "mui_font.h"

/* ------------------------------------------------------------------ *
 *  Syscalls                                                           *
 * ------------------------------------------------------------------ */

/*
 * Single-asm-statement stubs with explicit register constraints, as in
 * apps/doom/shim/ksys.h: the two-step "set ebx, then call" form in lib/ only
 * survives because those files are built at -O0, and this one is not.
 */
static inline unsigned ksys0(int n) {
    unsigned r;
    __asm__ volatile ("int $0x72" : "=a"(r) : "a"(n) : "memory");
    return r;
}
static inline unsigned ksys1(int n, unsigned a) {
    unsigned r;
    __asm__ volatile ("int $0x72" : "=a"(r) : "a"(n), "b"(a) : "memory");
    return r;
}
static inline unsigned ksys2(int n, unsigned a, unsigned b) {
    unsigned r;
    __asm__ volatile ("int $0x72" : "=a"(r) : "a"(n), "b"(a), "c"(b) : "memory");
    return r;
}

enum {
    SYS_EXIT        = 5,
    SYS_WRITE       = 12,
    SYS_CLOCK       = 15,
    SYS_TIME        = 14,
    SYS_GFX_OPEN    = 22,
    SYS_GFX_CLOSE   = 23,
    SYS_GFX_PALETTE = 24,
    SYS_GFX_BLIT    = 25,
    SYS_GETSCAN     = 26,
    SYS_MSLEEP      = 27,
    SYS_GETMOUSE    = 36
};

/** getscan (#26) result bits; mirrors KBD_RAW_* in the kernel's keyboard.h. */
#define SCAN_BREAK 0x0080
#define SCAN_E0    0x0100
#define SCAN_VALID 0x10000
/** getmouse (#36): bit 31 valid, 24..26 buttons, 12..23 dy, 0..11 dx. */
#define MOUSE_VALID 0x80000000u

unsigned mui_ms(void)   { return ksys0(SYS_CLOCK); }
unsigned mui_unix(void) { return ksys0(SYS_TIME); }

/**
 * @brief The character sink behind the vendored printf (../../printf.c's
 *        `putchar_` hook, which apps/doom/shim/printf.c expects the program
 *        to provide).
 *
 * Reaches the kernel console, which is behind the grabbed screen while the
 * program is running -- so this is for diagnostics that are read after the
 * fact, over the serial log, not for anything the user is meant to see now.
 */
void putchar_(char c) {
    ksys2(SYS_WRITE, (unsigned) (unsigned long) &c, 1);
}

/**
 * @brief What microui's `expect()` calls when an internal invariant breaks.
 *
 * The kernel's io.h names the `hlt` instruction this; in ring 3 the useful
 * meaning is "stop, but hand the display back first", because the alternative
 * is a frozen screen showing the last good frame with no way out. The
 * assertion text has already gone to the console (behind the grab, but it
 * reaches the serial log).
 */
void halt(void) {
    ksys0(SYS_GFX_CLOSE);
    ksys1(SYS_EXIT, (unsigned) -1);
    for (;;)                 /* unreachable: the syscall does not return */
        ;
}

/* ------------------------------------------------------------------ *
 *  qsort                                                              *
 * ------------------------------------------------------------------ */

/**
 * @brief The `qsort` the vendored microui.c calls (see shim/stdlib.h).
 *
 * Insertion sort, deliberately: the one caller is mu_end(), ordering at most
 * MU_ROOTLIST_SIZE (32) container pointers by z-index once a frame, on a list
 * that is already almost sorted from the frame before. Partitioning would cost
 * more than it saves at that size, and this is a fifth of the code.
 */
void qsort(void *base, size_t n, size_t size,
           int (*cmp)(const void *, const void *)) {
    unsigned char *a = (unsigned char *) base;
    for (size_t i = 1; i < n; i++)
        for (size_t j = i; j > 0; j--) {
            unsigned char *l = a + (j - 1) * size, *r = l + size;
            if (cmp(l, r) <= 0)
                break;
            for (size_t k = 0; k < size; k++) {
                unsigned char t = l[k];
                l[k] = r[k];
                r[k] = t;
            }
        }
}

/* ------------------------------------------------------------------ *
 *  The surface and its palette                                        *
 * ------------------------------------------------------------------ */

static unsigned char g_pix[MUI_W * MUI_H];   /**< The indexed frame. */

/**
 * What shows through where no window covers the screen. Deliberately not
 * MU_COLOR_WINDOWBG: the whole surface in the window colour makes every
 * window's own edge disappear into it.
 */
static mu_Color g_backdrop = { 24, 26, 32, 255 };

void mui_backdrop(mu_Color c) { g_backdrop = c; }

static unsigned g_pal[256];                  /**< 0x00RRGGBB per slot. */
static int g_pal_n;                          /**< Slots taken this frame. */

/** @brief Intern @p c into the frame's palette. @return Its slot. */
static unsigned char pal_index(mu_Color c) {
    unsigned rgb = ((unsigned) c.r << 16) | ((unsigned) c.g << 8) | c.b;
    for (int i = 0; i < g_pal_n; i++)
        if (g_pal[i] == rgb)
            return (unsigned char) i;
    if (g_pal_n < 256) {
        g_pal[g_pal_n] = rgb;
        return (unsigned char) g_pal_n++;
    }
    /* 256 distinct colours in one frame: settle for the nearest slot. No
     * microui frame gets near this; a custom-draw callback that shades
     * something smoothly could. */
    int best = 0;
    long best_d = 0x7FFFFFFF;
    for (int i = 0; i < 256; i++) {
        long dr = (long) c.r - (long) ((g_pal[i] >> 16) & 0xFF);
        long dg = (long) c.g - (long) ((g_pal[i] >> 8) & 0xFF);
        long db = (long) c.b - (long) (g_pal[i] & 0xFF);
        long d = dr * dr + dg * dg + db * db;
        if (d < best_d) { best_d = d; best = i; }
    }
    return (unsigned char) best;
}

/* ------------------------------------------------------------------ *
 *  Primitives                                                         *
 *                                                                     *
 *  Every public primitive interns its colour once and then works in    *
 *  index space, so a per-pixel loop does not re-scan the palette for   *
 *  each pixel it writes.                                               *
 * ------------------------------------------------------------------ */

/** @brief The overlap of @p a and @p b (w/h <= 0 if they do not meet). */
static mu_Rect isect(mu_Rect a, mu_Rect b) {
    int x0 = a.x > b.x ? a.x : b.x;
    int y0 = a.y > b.y ? a.y : b.y;
    int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    return mu_rect(x0, y0, x1 - x0, y1 - y0);
}

static void px_i(const mui_Surface *s, int x, int y, unsigned char ix) {
    const mu_Rect c = s->clip;
    if (x < c.x || y < c.y || x >= c.x + c.w || y >= c.y + c.h)
        return;
    if (x < 0 || y < 0 || x >= s->w || y >= s->h)
        return;
    s->pix[(long) y * s->w + x] = ix;
}

static void fill_i(const mui_Surface *s, mu_Rect r, unsigned char ix) {
    r = isect(isect(r, s->clip), mu_rect(0, 0, s->w, s->h));
    for (int y = 0; y < r.h; y++)
        memset(s->pix + (long) (r.y + y) * s->w + r.x, ix, (size_t) r.w);
}

void mui_px(const mui_Surface *s, int x, int y, mu_Color c) {
    px_i(s, x, y, pal_index(c));
}

void mui_fill(const mui_Surface *s, mu_Rect r, mu_Color c) {
    fill_i(s, r, pal_index(c));
}

void mui_frame(const mui_Surface *s, mu_Rect r, mu_Color c) {
    unsigned char ix = pal_index(c);
    fill_i(s, mu_rect(r.x, r.y, r.w, 1), ix);
    fill_i(s, mu_rect(r.x, r.y + r.h - 1, r.w, 1), ix);
    fill_i(s, mu_rect(r.x, r.y, 1, r.h), ix);
    fill_i(s, mu_rect(r.x + r.w - 1, r.y, 1, r.h), ix);
}

/**
 * @brief Bresenham line, @p width pixels thick.
 *
 * Thickness is a filled square brush rather than a proper stroke outline: at
 * the few pixels a clock hand needs, the difference is invisible and this
 * needs no division.
 */
void mui_line(const mui_Surface *s, int x0, int y0, int x1, int y1, int width,
              mu_Color c) {
    unsigned char ix = pal_index(c);
    if (width < 1)
        width = 1;
    int dx = x1 - x0, dy = y1 - y0;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int lo = -(width - 1) / 2, hi = lo + width - 1;
    for (;;) {
        if (width == 1)
            px_i(s, x0, y0, ix);
        else
            for (int oy = lo; oy <= hi; oy++)
                for (int ox = lo; ox <= hi; ox++)
                    px_i(s, x0 + ox, y0 + oy, ix);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/** @brief Filled circle of radius @p r centred on (@p cx, @p cy). */
void mui_disc(const mui_Surface *s, int cx, int cy, int r, mu_Color c) {
    unsigned char ix = pal_index(c);
    if (r < 0)
        return;
    /* Span per row rather than per pixel: one memset a line instead of a
     * sqrt-free distance test at every candidate pixel. */
    for (int y = -r; y <= r; y++) {
        int half = 0;
        while ((half + 1) * (half + 1) + y * y <= r * r)
            half++;
        fill_i(s, mu_rect(cx - half, cy + y, half * 2 + 1, 1), ix);
    }
}

/** @brief Circle outline of radius @p r, @p width pixels thick (drawn inward). */
void mui_ring(const mui_Surface *s, int cx, int cy, int r, int width,
              mu_Color c) {
    unsigned char ix = pal_index(c);
    if (width < 1)
        width = 1;
    int inner = r - width;
    if (inner < 0)
        inner = 0;
    int r2 = r * r, i2 = inner * inner;
    for (int y = -r; y <= r; y++) {
        int outer_half = 0, inner_half = -1;
        while ((outer_half + 1) * (outer_half + 1) + y * y <= r2)
            outer_half++;
        if (y * y < i2)
            while ((inner_half + 1) * (inner_half + 1) + y * y < i2)
                inner_half++;
        if (inner_half < 0) {
            fill_i(s, mu_rect(cx - outer_half, cy + y, outer_half * 2 + 1, 1), ix);
        } else {
            int w = outer_half - inner_half;
            fill_i(s, mu_rect(cx - outer_half, cy + y, w, 1), ix);
            fill_i(s, mu_rect(cx + inner_half + 1, cy + y, w, 1), ix);
        }
    }
}

/* ------------------------------------------------------------------ *
 *  Text                                                               *
 * ------------------------------------------------------------------ */

int mui_text_width(const char *str, int len) {
    if (len < 0)
        len = str ? strlen(str) : 0;
    return MUI_FONT_W * len;
}

int mui_text_height(void) {
    return MUI_FONT_H;
}

/**
 * @brief Draw @p len characters of @p str with the cell's top-left at (x,y),
 *        each font pixel as a @p scale x @p scale block.
 *
 * Nearest-neighbour blow-up rather than a second font at a second size: this
 * is a bitmap font, so a 2x or 3x cell is exactly what a 2x or 3x pixel grid
 * of it looks like, and the only alternative at this palette depth would be a
 * blurrier version of the same thing.
 */
static void draw_text(const mui_Surface *s, int x, int y, const char *str,
                      int len, int scale, unsigned char ix) {
    if (scale < 1)
        scale = 1;
    const int cw = MUI_FONT_W * scale;
    for (int n = 0; n < len; n++, x += cw) {
        unsigned char ch = (unsigned char) str[n];
        if (ch < MUI_FONT_FIRST || ch > MUI_FONT_LAST)
            ch = '?';
        /* Nothing of this cell is on screen: skip its 128 bit tests. */
        if (x + cw <= s->clip.x || x >= s->clip.x + s->clip.w)
            continue;
        const unsigned char *g = mui_font[ch - MUI_FONT_FIRST];
        for (int row = 0; row < MUI_FONT_H; row++) {
            unsigned bits = g[row];
            for (int col = 0; bits; col++, bits = (bits << 1) & 0xFF) {
                if (!(bits & 0x80))
                    continue;
                if (scale == 1)
                    px_i(s, x + col, y + row, ix);
                else
                    fill_i(s, mu_rect(x + col * scale, y + row * scale,
                                      scale, scale), ix);
            }
        }
    }
}

void mui_string(const mui_Surface *s, int x, int y, const char *str, mu_Color c) {
    if (str)
        draw_text(s, x, y, str, strlen(str), 1, pal_index(c));
}

void mui_string_scaled(const mui_Surface *s, int x, int y, const char *str,
                       int scale, mu_Color c) {
    if (str)
        draw_text(s, x, y, str, strlen(str), scale, pal_index(c));
}

/* ------------------------------------------------------------------ *
 *  Icons                                                              *
 * ------------------------------------------------------------------ */

/**
 * @brief Draw one of microui's four built-in icons centred in @p rect.
 *
 * Drawn as shapes, not as stand-in letters the way the kernel's renderer.c
 * does it: a close box and a tree-node arrow are small enough that an 'x' or a
 * 'v' in their place is the first thing that looks wrong.
 */
static void draw_icon(const mui_Surface *s, int id, mu_Rect rect,
                      unsigned char ix) {
    int cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;
    int r = (rect.w < rect.h ? rect.w : rect.h) / 2 - 2;
    if (r < 2)
        r = 2;
    switch (id) {
    case MU_ICON_CLOSE: {
        int d = r * 2 / 3;
        for (int i = -d; i <= d; i++) {
            px_i(s, cx + i, cy + i, ix);
            px_i(s, cx + i, cy - i, ix);
            px_i(s, cx + i + 1, cy + i, ix);
            px_i(s, cx + i + 1, cy - i, ix);
        }
        break;
    }
    case MU_ICON_CHECK: {
        /* A tick, not a V: the ascender is twice the descender, which is the
         * difference between reading as "checked" and as a stray glyph. */
        int lo = r / 2, hi = r;
        int bx = cx - lo / 2, by = cy + hi / 2;  /* the corner it turns at */
        for (int i = 0; i <= lo; i++) {          /* down-right, short */
            px_i(s, bx - lo + i, by - lo + i - 1, ix);
            px_i(s, bx - lo + i, by - lo + i, ix);
        }
        for (int i = 0; i <= hi; i++) {          /* up-right, long */
            px_i(s, bx + i, by - i - 1, ix);
            px_i(s, bx + i, by - i, ix);
        }
        break;
    }
    case MU_ICON_COLLAPSED:                       /* right-pointing triangle */
        for (int i = 0; i <= r; i++)
            fill_i(s, mu_rect(cx - r / 2 + i, cy - (r - i), 1, (r - i) * 2 + 1), ix);
        break;
    case MU_ICON_EXPANDED:                        /* down-pointing triangle */
        for (int i = 0; i <= r; i++)
            fill_i(s, mu_rect(cx - (r - i), cy - r / 2 + i, (r - i) * 2 + 1, 1), ix);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ *
 *  Custom-draw command                                                *
 * ------------------------------------------------------------------ */

/** A command type of our own, past the ones microui defines. */
#define MUI_COMMAND_CUSTOM MU_COMMAND_MAX

/** @brief The payload of a @ref MUI_COMMAND_CUSTOM command. */
typedef struct {
    mu_BaseCommand base;
    mu_Rect rect;
    mui_DrawFn fn;
    void *udata;
} mui_CustomCommand;

void mui_draw_custom(mu_Context *ctx, mu_Rect rect, mui_DrawFn fn, void *udata) {
    if (!fn)
        return;
    /* Same clip dance as mu_draw_icon: emit a clip command when the rect is
     * only partly visible, and put the unclipped rect back afterwards. */
    int clipped = mu_check_clip(ctx, rect);
    if (clipped == MU_CLIP_ALL)
        return;
    if (clipped == MU_CLIP_PART)
        mu_set_clip(ctx, mu_get_clip_rect(ctx));
    mui_CustomCommand *cmd = (mui_CustomCommand *)
        mu_push_command(ctx, MUI_COMMAND_CUSTOM, sizeof(mui_CustomCommand));
    cmd->rect = rect;
    cmd->fn = fn;
    cmd->udata = udata;
    if (clipped)
        mu_set_clip(ctx, mu_rect(0, 0, 0x1000000, 0x1000000));
}

/* ------------------------------------------------------------------ *
 *  Rendering one frame                                                *
 * ------------------------------------------------------------------ */

/** @brief Walk the command list and paint it into @ref g_pix. */
static void render(mu_Context *ctx) {
    const mu_Rect full = mu_rect(0, 0, MUI_W, MUI_H);
    mui_Surface s = { g_pix, MUI_W, MUI_H, full };
    mu_Command *cmd = 0;

    while (mu_next_command(ctx, &cmd)) {
        switch (cmd->type) {
        case MU_COMMAND_CLIP:
            s.clip = isect(cmd->clip.rect, full);
            break;
        case MU_COMMAND_RECT:
            fill_i(&s, cmd->rect.rect, pal_index(cmd->rect.color));
            break;
        case MU_COMMAND_TEXT:
            draw_text(&s, cmd->text.pos.x, cmd->text.pos.y, cmd->text.str,
                      strlen(cmd->text.str), 1, pal_index(cmd->text.color));
            break;
        case MU_COMMAND_ICON:
            draw_icon(&s, cmd->icon.id, cmd->icon.rect,
                      pal_index(cmd->icon.color));
            break;
        case MUI_COMMAND_CUSTOM: {
            mui_CustomCommand *cc = (mui_CustomCommand *) cmd;
            mui_Surface cs = s;
            cs.clip = isect(s.clip, cc->rect);
            if (cs.clip.w > 0 && cs.clip.h > 0)
                cc->fn(&cs, cc->rect, cc->udata);
            break;
        }
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------ *
 *  The pointer                                                        *
 * ------------------------------------------------------------------ */

static int g_mx = MUI_W / 2, g_my = MUI_H / 2;

/*
 * Drawn here because a grabbed screen has no compositor behind it: the
 * kernel's own pointer is part of the desktop it just parked. 'X' is the
 * outline, '.' the fill, as in the Qt port's overlay.
 */
static const char *const g_cursor[] = {
    "X           ", "XX          ", "X.X         ", "X..X        ",
    "X...X       ", "X....X      ", "X.....X     ", "X......X    ",
    "X.......X   ", "X........X  ", "X.....XXXXX ", "X..X..X     ",
    "X.X X..X    ", "XX  X..X    ", "X    X..X   ", "     X..X   ",
    "      X..X  ", "      X..X  ", "       XX   "
};

/** @brief Stamp the pointer into the finished frame. */
static void draw_cursor(void) {
    const mu_Rect full = mu_rect(0, 0, MUI_W, MUI_H);
    mui_Surface s = { g_pix, MUI_W, MUI_H, full };
    unsigned char edge = pal_index(mu_color(0, 0, 0, 255));
    unsigned char body = pal_index(mu_color(255, 255, 255, 255));
    for (int row = 0; row < (int) (sizeof g_cursor / sizeof *g_cursor); row++)
        for (int col = 0; g_cursor[row][col]; col++) {
            char k = g_cursor[row][col];
            if (k == 'X')
                px_i(&s, g_mx + col, g_my + row, edge);
            else if (k == '.')
                px_i(&s, g_mx + col, g_my + row, body);
        }
}

/* ------------------------------------------------------------------ *
 *  Input                                                              *
 * ------------------------------------------------------------------ */

/** One scancode's microui key bit and the characters it types. */
typedef struct { unsigned char key; char normal, shifted; } keydef;

/*
 * Scancode set 1 make codes, US layout, main block. `key` is the MU_KEY_* bit
 * microui wants (0 for keys that only type text); `normal`/`shifted` are the
 * characters, selected by Shift (and by Caps Lock for letters).
 */
static const keydef g_keys[0x54] = {
    [0x02] = {0, '1', '!'}, [0x03] = {0, '2', '@'}, [0x04] = {0, '3', '#'},
    [0x05] = {0, '4', '$'}, [0x06] = {0, '5', '%'}, [0x07] = {0, '6', '^'},
    [0x08] = {0, '7', '&'}, [0x09] = {0, '8', '*'}, [0x0A] = {0, '9', '('},
    [0x0B] = {0, '0', ')'}, [0x0C] = {0, '-', '_'}, [0x0D] = {0, '=', '+'},
    [0x0E] = {MU_KEY_BACKSPACE, 0, 0},
    [0x0F] = {0, '\t', '\t'},
    [0x10] = {0, 'q', 'Q'}, [0x11] = {0, 'w', 'W'}, [0x12] = {0, 'e', 'E'},
    [0x13] = {0, 'r', 'R'}, [0x14] = {0, 't', 'T'}, [0x15] = {0, 'y', 'Y'},
    [0x16] = {0, 'u', 'U'}, [0x17] = {0, 'i', 'I'}, [0x18] = {0, 'o', 'O'},
    [0x19] = {0, 'p', 'P'}, [0x1A] = {0, '[', '{'}, [0x1B] = {0, ']', '}'},
    [0x1C] = {MU_KEY_RETURN, 0, 0},
    [0x1D] = {MU_KEY_CTRL, 0, 0},
    [0x1E] = {0, 'a', 'A'}, [0x1F] = {0, 's', 'S'}, [0x20] = {0, 'd', 'D'},
    [0x21] = {0, 'f', 'F'}, [0x22] = {0, 'g', 'G'}, [0x23] = {0, 'h', 'H'},
    [0x24] = {0, 'j', 'J'}, [0x25] = {0, 'k', 'K'}, [0x26] = {0, 'l', 'L'},
    [0x27] = {0, ';', ':'}, [0x28] = {0, '\'', '"'}, [0x29] = {0, '`', '~'},
    [0x2A] = {MU_KEY_SHIFT, 0, 0},
    [0x2B] = {0, '\\', '|'},
    [0x2C] = {0, 'z', 'Z'}, [0x2D] = {0, 'x', 'X'}, [0x2E] = {0, 'c', 'C'},
    [0x2F] = {0, 'v', 'V'}, [0x30] = {0, 'b', 'B'}, [0x31] = {0, 'n', 'N'},
    [0x32] = {0, 'm', 'M'}, [0x33] = {0, ',', '<'}, [0x34] = {0, '.', '>'},
    [0x35] = {0, '/', '?'},
    [0x36] = {MU_KEY_SHIFT, 0, 0},
    [0x37] = {0, '*', '*'},                      /* keypad * */
    [0x38] = {MU_KEY_ALT, 0, 0},
    [0x39] = {0, ' ', ' '},
    [0x4A] = {0, '-', '-'},                      /* keypad - */
    [0x4E] = {0, '+', '+'},                      /* keypad + */
    [0x47] = {0, '7', '7'}, [0x48] = {0, '8', '8'}, [0x49] = {0, '9', '9'},
    [0x4B] = {0, '4', '4'}, [0x4C] = {0, '5', '5'}, [0x4D] = {0, '6', '6'},
    [0x4F] = {0, '1', '1'}, [0x50] = {0, '2', '2'}, [0x51] = {0, '3', '3'},
    [0x52] = {0, '0', '0'}, [0x53] = {0, '.', '.'}
};

static int g_shift, g_caps, g_quit, g_buttons;

void mui_quit(void) { g_quit = 1; }

/** @brief Drain the raw key ring into @p ctx. Esc sets the quit flag. */
static void pump_keys(mu_Context *ctx) {
    unsigned ev;
    while ((ev = ksys0(SYS_GETSCAN)) & SCAN_VALID) {
        int release = (ev & SCAN_BREAK) != 0, e0 = (ev & SCAN_E0) != 0;
        unsigned sc = ev & 0x7F;

        if (e0) {
            /* The grey keys. Only the two that collide with the keypad and
             * mean something to a microui frame are worth translating. */
            if (sc == 0x1C) {                     /* keypad Enter */
                if (release) mu_input_keyup(ctx, MU_KEY_RETURN);
                else         mu_input_keydown(ctx, MU_KEY_RETURN);
            } else if (sc == 0x1D) {              /* right Ctrl */
                if (release) mu_input_keyup(ctx, MU_KEY_CTRL);
                else         mu_input_keydown(ctx, MU_KEY_CTRL);
            } else if (sc == 0x35 && !release) {  /* keypad / */
                mu_input_text(ctx, "/");
            }
            continue;
        }

        if (sc == 0x01) {                         /* Esc: leave */
            if (!release)
                g_quit = 1;
            continue;
        }
        if (sc == 0x3A) {                         /* Caps Lock */
            if (!release)
                g_caps = !g_caps;
            continue;
        }
        if (sc >= sizeof g_keys / sizeof *g_keys)
            continue;

        keydef k = g_keys[sc];
        if (k.key == MU_KEY_SHIFT)
            g_shift = !release;
        if (k.key) {
            if (release) mu_input_keyup(ctx, k.key);
            else         mu_input_keydown(ctx, k.key);
            continue;
        }
        if (release || !k.normal)
            continue;
        int letter = k.normal >= 'a' && k.normal <= 'z';
        char c = (g_shift != (g_caps && letter)) ? k.shifted : k.normal;
        if (c && c != '\t') {
            char text[2] = { c, 0 };
            mu_input_text(ctx, text);
        }
    }
}

/** @brief Drain the pointer ring into @p ctx, tracking the cursor position. */
static void pump_mouse(mu_Context *ctx) {
    unsigned ev;
    while ((ev = ksys0(SYS_GETMOUSE)) & MOUSE_VALID) {
        /* 12-bit signed deltas, +dy downwards. */
        int dx = (int) (ev & 0xFFF), dy = (int) ((ev >> 12) & 0xFFF);
        if (dx & 0x800) dx -= 0x1000;
        if (dy & 0x800) dy -= 0x1000;
        int buttons = (int) ((ev >> 24) & 7);

        g_mx = mu_clamp(g_mx + dx, 0, MUI_W - 1);
        g_my = mu_clamp(g_my + dy, 0, MUI_H - 1);
        mu_input_mousemove(ctx, g_mx, g_my);

        /* microui's button bits happen to be the driver's (left 1, right 2,
         * middle 4), so the mask passes straight through. */
        for (int bit = 1; bit <= 4; bit <<= 1) {
            int now = buttons & bit, was = g_buttons & bit;
            if (now && !was)
                mu_input_mousedown(ctx, g_mx, g_my, bit);
            else if (!now && was)
                mu_input_mouseup(ctx, g_mx, g_my, bit);
        }
        g_buttons = buttons;
    }
}

/* ------------------------------------------------------------------ *
 *  The frame loop                                                     *
 * ------------------------------------------------------------------ */

static int text_width_cb(mu_Font font, const char *str, int len) {
    (void) font;
    return mui_text_width(str, len);
}

static int text_height_cb(mu_Font font) {
    (void) font;
    return mui_text_height();
}

/** One frame every ~16 ms: the rate the kernel's own compositor runs at. */
#define FRAME_MS 16

int mui_run(mui_FrameFn frame, void *udata) {
    static mu_Context ctx;        /* ~290 KiB: .bss, not the 256 KiB stack */

    if (!frame)
        return -1;
    if (!ksys2(SYS_GFX_OPEN, MUI_W, MUI_H))
        return -1;

    mu_init(&ctx);
    ctx.text_width = text_width_cb;
    ctx.text_height = text_height_cb;

    g_quit = 0;
    while (!g_quit) {
        unsigned t0 = mui_ms();

        pump_keys(&ctx);
        pump_mouse(&ctx);

        mu_begin(&ctx);
        frame(&ctx, udata);
        mu_end(&ctx);

        /* Slot 0 is the clear colour, so clearing the surface to 0 paints the
         * background; everything the frame draws interns after it. */
        g_pal_n = 0;
        memset(g_pix, pal_index(g_backdrop), sizeof g_pix);
        render(&ctx);
        draw_cursor();

        ksys1(SYS_GFX_PALETTE, (unsigned) (unsigned long) g_pal);
        ksys1(SYS_GFX_BLIT, (unsigned) (unsigned long) g_pix);

        unsigned spent = mui_ms() - t0;
        if (spent < FRAME_MS)
            ksys1(SYS_MSLEEP, FRAME_MS - spent);
    }

    ksys0(SYS_GFX_CLOSE);
    return 0;
}
