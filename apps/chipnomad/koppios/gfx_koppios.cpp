/*
 * ChipNomad's Gfx on koppi-os.
 *
 * The kernel hands a ring-3 program the whole screen as one 8-bpp indexed
 * surface plus a 256-entry palette (video.c, syscalls 22-25): grab a size,
 * install colours, push frames. video_blit8() scales what it is given by a
 * whole number and centres it, so a 40x20 grid of the 12x16 font -- 480x320
 * -- comes out pixel-exact at 2x on a 1024x768 desktop and 3x on 1440x900,
 * never resampled. That is why this backend asks for a small surface instead
 * of the display's own resolution: a letterboxed integer multiple of the text
 * grid is sharper than a stretched approximation of it, and the blit is 150 KB
 * a frame rather than a megabyte.
 *
 * Indexed colour is the one real constraint, and the tracker fits inside it
 * comfortably: a theme is ten RGB values, and the only other colours on
 * screen are the shades the waveform bitmaps blend between foreground and
 * background (drawVerticalLine() in waveform_display.cpp uses four of them).
 * So rather than quantising a true-colour buffer every frame, this keeps the
 * indexed surface *as* the drawing surface and interns each RGB value into a
 * palette slot the first time it appears -- about fifty entries in practice,
 * with nearest-match as the fallback if a .cth theme ever pushes past 256.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_koppios.h"

#include "font_manager.h"
#include "ksys.h"

#define TEXT_COLS 40
#define TEXT_ROWS 20
#define PRINT_BUFFER_SIZE 256

/* The surface the kernel scales up. 40x20 of the smallest bundled font. */
#define DEFAULT_SCREEN_W (TEXT_COLS * 12)
#define DEFAULT_SCREEN_H (TEXT_ROWS * 16)

static uint8_t* surface;          /* screenW * screenH, one byte per pixel */
static int screenW, screenH;
static int charW = 12, charH = 16;
static int offsetX, offsetY;
static int grabbed;
static int isDirty;

static const FontResolution* currentResolution;
static const uint8_t* fontData;
static int fontBytesPerRow;       /* (charWidth + 7) / 8 */

static int fgColor, bgColor, cursorColor;

static char printBuffer[PRINT_BUFFER_SIZE];

/* ------------------------------------------------------------------ *
 *  Palette                                                           *
 * ------------------------------------------------------------------ */

static uint32_t palette[256];
static int paletteUsed;
static int paletteDirty;

/** Palette slot holding @p rgb, adding it if there is room. */
static uint8_t intern(uint32_t rgb) {
    rgb &= 0xFFFFFF;
    for (int i = 0; i < paletteUsed; i++) {
        if (palette[i] == rgb)
            return (uint8_t) i;
    }
    if (paletteUsed < 256) {
        palette[paletteUsed] = rgb;
        paletteDirty = 1;
        return (uint8_t) paletteUsed++;
    }

    /* Full. Closest entry by squared distance in RGB -- never reached by the
     * bundled themes, but a hand-written .cth plus a long wavetable preview
     * could get here, and a slightly wrong colour beats a wrong pixel. */
    int best = 0;
    long bestD = 0x7FFFFFFF;
    int r = (int) ((rgb >> 16) & 0xFF), g = (int) ((rgb >> 8) & 0xFF), b = (int) (rgb & 0xFF);
    for (int i = 0; i < 256; i++) {
        int dr = r - (int) ((palette[i] >> 16) & 0xFF);
        int dg = g - (int) ((palette[i] >> 8) & 0xFF);
        int db = b - (int) (palette[i] & 0xFF);
        long d = (long) dr * dr + (long) dg * dg + (long) db * db;
        if (d < bestD) { bestD = d; best = i; }
    }
    return (uint8_t) best;
}

/* ------------------------------------------------------------------ *
 *  Surface primitives                                                 *
 * ------------------------------------------------------------------ */

#define CHAR_X(x) ((x) * charW + offsetX)
#define CHAR_Y(y) ((y) * charH + offsetY)

static inline void putPixel(int x, int y, uint8_t idx) {
    if (x < 0 || y < 0 || x >= screenW || y >= screenH)
        return;
    surface[(size_t) y * screenW + x] = idx;
}

static void fillRectPx(int x, int y, int w, int h, uint8_t idx) {
    if (w <= 0 || h <= 0)
        return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > screenW) w = screenW - x;
    if (y + h > screenH) h = screenH - y;
    for (int row = 0; row < h; row++)
        memset(surface + (size_t) (y + row) * screenW + x, idx, (size_t) w);
    isDirty = 1;
}

/* ------------------------------------------------------------------ *
 *  Font                                                               *
 * ------------------------------------------------------------------ */

static void selectFont(FontManager& fonts) {
    currentResolution = fonts.selectResolution(fonts.getCurrent(), screenW, screenH);
    if (currentResolution) {
        charW = currentResolution->charWidth;
        charH = currentResolution->charHeight;
        fontData = currentResolution->data;
    } else {
        charW = 12;
        charH = 16;
        fontData = NULL;
    }
    fontBytesPerRow = (charW + 7) / 8;

    offsetX = (screenW - TEXT_COLS * charW) / 2;
    offsetY = (screenH - TEXT_ROWS * charH) / 2;
    if (offsetX < 0) offsetX = 0;
    if (offsetY < 0) offsetY = 0;
}

/** Draw one glyph's set bits in @p idx; the cell background is already laid down. */
static void drawGlyph(int px, int py, unsigned char ch, uint8_t idx) {
    if (!fontData || ch < 32 || ch > 126)
        return;
    const uint8_t* g = fontData + (size_t) (ch - 32) * fontBytesPerRow * charH;
    for (int row = 0; row < charH; row++) {
        const uint8_t* line = g + (size_t) row * fontBytesPerRow;
        for (int byte = 0; byte < fontBytesPerRow; byte++) {
            /* The last byte of a row is partly padding when charW is not a
             * multiple of 8 (12x16 uses 12 of its 16 bits). */
            int bits = (byte == fontBytesPerRow - 1 && (charW % 8)) ? charW % 8 : 8;
            uint8_t v = line[byte];
            for (int bit = 0; bit < bits; bit++) {
                if (v & (0x80 >> bit))
                    putPixel(px + byte * 8 + bit, py + row, idx);
            }
        }
    }
}

/* ------------------------------------------------------------------ *
 *  Lifecycle                                                          *
 * ------------------------------------------------------------------ */

GfxKoppiOS::~GfxKoppiOS() {
    teardown();
}

int GfxKoppiOS::setup(int* screenWidth, int* screenHeight) {
    if (screenWidth && *screenWidth > 0 && screenHeight && *screenHeight > 0) {
        screenW = *screenWidth;
        screenH = *screenHeight;
    } else {
        screenW = DEFAULT_SCREEN_W;
        screenH = DEFAULT_SCREEN_H;
    }

    /* Clamp to what the grab accepts and to a grid that still holds 40x20 of
     * the smallest font. */
    if (screenW < DEFAULT_SCREEN_W) screenW = DEFAULT_SCREEN_W;
    if (screenH < DEFAULT_SCREEN_H) screenH = DEFAULT_SCREEN_H;
    if (screenW > 4096) screenW = 4096;
    if (screenH > 4096) screenH = 4096;

    if (screenWidth)  *screenWidth = screenW;
    if (screenHeight) *screenHeight = screenH;

    surface = (uint8_t*) calloc((size_t) screenW * screenH, 1);
    if (!surface) {
        fprintf(stderr, "chipnomad: out of memory for a %dx%d surface\n", screenW, screenH);
        return 0;
    }

    selectFont(fontManager);

    if (!ksys2(SYS_GFX_OPEN, (unsigned long) screenW, (unsigned long) screenH)) {
        fprintf(stderr, "chipnomad: cannot take the screen (no framebuffer, "
                        "or another program is holding it)\n");
        free(surface);
        surface = NULL;
        return 0;
    }
    grabbed = 1;

    /* Slot 0 is black until the first setBgColor(); a frame blitted before
     * then is black rather than whatever the previous tenant left. */
    paletteUsed = 0;
    intern(0x000000);
    paletteDirty = 1;
    isDirty = 1;

    return 1;
}

void GfxKoppiOS::teardown() {
    if (grabbed) {
        ksys0(SYS_GFX_CLOSE);
        grabbed = 0;
    }
    free(surface);
    surface = NULL;
}

/* ------------------------------------------------------------------ *
 *  Colours                                                            *
 * ------------------------------------------------------------------ */

void GfxKoppiOS::setFgColor(int rgb)     { fgColor = rgb; }
void GfxKoppiOS::setBgColor(int rgb)     { bgColor = rgb; }
void GfxKoppiOS::setCursorColor(int rgb) { cursorColor = rgb; }

/* ------------------------------------------------------------------ *
 *  Drawing                                                            *
 * ------------------------------------------------------------------ */

void GfxKoppiOS::clear() {
    if (!surface) return;
    memset(surface, intern((uint32_t) bgColor), (size_t) screenW * screenH);
    isDirty = 1;
}

void GfxKoppiOS::point(int x, int y, uint32_t color) {
    if (!surface) return;
    putPixel(x, y, intern(color));
    isDirty = 1;
}

void GfxKoppiOS::clearRect(int x, int y, int w, int h) {
    if (!surface) return;
    fillRectPx(CHAR_X(x), CHAR_Y(y), w * charW, h * charH, intern((uint32_t) bgColor));
}

void GfxKoppiOS::print(int x, int y, const char* text) {
    if (!surface || !text) return;

    const uint8_t bg = intern((uint32_t) bgColor);
    const uint8_t fg = intern((uint32_t) fgColor);
    const int right = offsetX + TEXT_COLS * charW;

    int cx = CHAR_X(x);
    int cy = CHAR_Y(y);

    for (const char* p = text; *p; p++) {
        /* "\r\n" wraps back to the starting column, as the SDL backends do --
         * the help screen's multi-line strings rely on it. */
        if (p[0] == '\r' && p[1] == '\n') {
            p++;
            cx = CHAR_X(x);
            cy += charH;
            continue;
        }

        fillRectPx(cx, cy, charW, charH, bg);
        drawGlyph(cx, cy, (unsigned char) *p, fg);

        cx += charW;
        if (cx >= right) {
            cx = CHAR_X(x);
            cy += charH;
        }
    }
    isDirty = 1;
}

void GfxKoppiOS::printf(int x, int y, const char* format, va_list args) {
    vsnprintf(printBuffer, PRINT_BUFFER_SIZE, format, args);
    print(x, y, printBuffer);
}

void GfxKoppiOS::cursor(int x, int y, int w) {
    if (!surface) return;
    fillRectPx(CHAR_X(x), CHAR_Y(y) + charH - 1, w * charW, 1,
               intern((uint32_t) cursorColor));
}

void GfxKoppiOS::rect(int x, int y, int w, int h) {
    if (!surface) return;
    const uint8_t fg = intern((uint32_t) fgColor);
    const int cx = CHAR_X(x), cy = CHAR_Y(y);
    const int cw = w * charW, ch = h * charH;

    fillRectPx(cx, cy, cw, 1, fg);              /* top    */
    fillRectPx(cx, cy + ch - 1, cw, 1, fg);     /* bottom */
    fillRectPx(cx, cy, 1, ch, fg);              /* left   */
    fillRectPx(cx + cw - 1, cy, 1, ch, fg);     /* right  */
}

void GfxKoppiOS::updateScreen() {
    if (!surface || !grabbed || !isDirty)
        return;

    if (paletteDirty) {
        for (int i = paletteUsed; i < 256; i++)
            palette[i] = 0;
        ksys1(SYS_GFX_PALETTE, (unsigned long) palette);
        paletteDirty = 0;
    }

    ksys1(SYS_GFX_BLIT, (unsigned long) surface);
    isDirty = 0;
}

/* ------------------------------------------------------------------ *
 *  Bitmaps                                                            *
 * ------------------------------------------------------------------ */

/*
 * A Bitmap is 8-bit coverage: 0 means background, 255 means foreground, and
 * the values between are blends (the AY waveform display draws envelope and
 * noise shapes at 64, 128-190 and 160). Each distinct level costs one palette
 * slot, interned on first use; the per-call table below keeps that to one
 * lookup per level rather than one per pixel.
 */
/** Mark every coverage level as not-yet-interned. */
static void blendTableReset(short lut[256]) {
    for (int i = 0; i < 256; i++)
        lut[i] = -1;
}

static uint8_t blendIndex(short lut[256], uint8_t alpha, int fg, int bg) {
    if (lut[alpha] >= 0)
        return (uint8_t) lut[alpha];

    int fr = (fg >> 16) & 0xFF, fgn = (fg >> 8) & 0xFF, fb = fg & 0xFF;
    int br = (bg >> 16) & 0xFF, bgn = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    int r = br + ((fr - br) * alpha) / 255;
    int g = bgn + ((fgn - bgn) * alpha) / 255;
    int b = bb + ((fb - bb) * alpha) / 255;

    uint8_t idx = intern(((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b);
    lut[alpha] = idx;
    return idx;
}

Bitmap* GfxKoppiOS::bitmapCreate(int widthChars, int heightChars) {
    Bitmap* bitmap = (Bitmap*) malloc(sizeof(Bitmap));
    if (!bitmap) return NULL;

    bitmap->widthChars = widthChars;
    bitmap->heightChars = heightChars;
    bitmap->widthPixels = widthChars * charW;
    bitmap->heightPixels = heightChars * charH;
    bitmap->userdata = NULL;

    size_t n = (size_t) bitmap->widthPixels * bitmap->heightPixels;
    bitmap->data = (uint8_t*) calloc(n ? n : 1, 1);
    if (!bitmap->data) {
        free(bitmap);
        return NULL;
    }
    return bitmap;
}

void GfxKoppiOS::bitmapClear(Bitmap* bitmap) {
    if (!bitmap || !bitmap->data) return;
    memset(bitmap->data, 0,
           (size_t) bitmap->widthPixels * bitmap->heightPixels);
}

void GfxKoppiOS::bitmapFree(Bitmap* bitmap) {
    if (!bitmap) return;
    free(bitmap->data);
    free(bitmap);
}

void GfxKoppiOS::drawBitmap(Bitmap* bitmap, int col, int row) {
    if (!surface || !bitmap || !bitmap->data) return;

    short lut[256];
    blendTableReset(lut);

    const int ox = CHAR_X(col), oy = CHAR_Y(row);
    for (int y = 0; y < bitmap->heightPixels; y++) {
        const uint8_t* src = bitmap->data + (size_t) y * bitmap->widthPixels;
        for (int x = 0; x < bitmap->widthPixels; x++)
            putPixel(ox + x, oy + y, blendIndex(lut, src[x], fgColor, bgColor));
    }
    isDirty = 1;
}

void GfxKoppiOS::drawCharBitmap(uint8_t* bitmap, int col, int row) {
    if (!surface || !bitmap) return;

    short lut[256];
    blendTableReset(lut);

    const int ox = CHAR_X(col), oy = CHAR_Y(row);
    for (int y = 0; y < charH; y++) {
        for (int x = 0; x < charW; x++)
            putPixel(ox + x, oy + y,
                     blendIndex(lut, bitmap[y * charW + x], fgColor, bgColor));
    }
    isDirty = 1;
}

/* ------------------------------------------------------------------ *
 *  Font queries                                                       *
 * ------------------------------------------------------------------ */

int GfxKoppiOS::getCharWidth()  { return charW; }
int GfxKoppiOS::getCharHeight() { return charH; }

void GfxKoppiOS::reloadFont() {
    selectFont(fontManager);
    isDirty = 1;
}

/*
 * No HUD. Both are for the touch builds' on-screen gamepad; this machine has
 * a real keyboard and the tracker reads it directly.
 */
void GfxKoppiOS::drawHUD() { }
void GfxKoppiOS::setButtonPressed(int buttonIndex, int pressed) {
    (void) buttonIndex; (void) pressed;
}
