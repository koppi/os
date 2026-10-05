/**
 * @file apps/microui/mui.h
 * @brief The ring-3 microui runtime: a full-screen microui surface for a
 *        userspace program.
 *
 * The kernel's own desktop drives the same vendored microui (../../microui.c)
 * through renderer.c, straight into the 32-bpp framebuffer shadow. A ring-3
 * program cannot touch that shadow; what it gets instead is the full-screen
 * grab (syscalls 22..25, see video.h): an 8-bpp indexed surface of its chosen
 * size, presented one frame at a time through a 256-entry palette. This
 * runtime is the renderer and event pump for that arrangement --
 *
 *   mui_run()   grabs the screen, then calls the frame callback in a loop:
 *               input in, one microui frame out, blit, repeat at ~60 Hz, and
 *               hands the screen back however the loop ends.
 *   mui_*()     drawing primitives for a @ref mui_draw_custom callback, which
 *               is how a program paints something microui has no widget for
 *               (a clock face, say) without leaving the command list and
 *               losing its clipping and z-order.
 *
 * Colours are true 8-bit RGB: the palette is interned as the frame draws, so
 * a program picks colours freely and only pays for the ones it uses (see
 * mui.c). Text is the 8x16 Unifont cell baked in by mkfont.c.
 */
#pragma once

#include "../../microui.h"

/**
 * @name The surface
 *
 * 640x400 like the Qt port's (third_party/qt6-gui), and for the same reasons:
 * @ref video_blit8 refuses a surface larger than the screen and otherwise
 * scales by the largest whole number that fits, so a small surface is the one
 * that works on every panel this kernel boots on -- and at 256 KiB a frame is
 * cheap enough to redraw and copy out whole, every frame.
 */
///@{
#define MUI_W 640
#define MUI_H 400
///@}

/** The 8-bpp surface, as handed to a custom-draw callback. */
typedef struct {
    unsigned char *pix;   /**< @ref MUI_W * @ref MUI_H palette indices. */
    int w, h;             /**< Surface size. */
    mu_Rect clip;         /**< Everything drawn is clipped to this. */
} mui_Surface;

/**
 * @name Drawing primitives
 *
 * All of these clip to @c s->clip and intern their colour, so a callback can
 * draw wherever it likes without checking bounds.
 */
///@{
void mui_px(const mui_Surface *s, int x, int y, mu_Color c);
void mui_fill(const mui_Surface *s, mu_Rect r, mu_Color c);
void mui_frame(const mui_Surface *s, mu_Rect r, mu_Color c);
void mui_line(const mui_Surface *s, int x0, int y0, int x1, int y1, int width, mu_Color c);
void mui_disc(const mui_Surface *s, int cx, int cy, int r, mu_Color c);
void mui_ring(const mui_Surface *s, int cx, int cy, int r, int width, mu_Color c);
void mui_string(const mui_Surface *s, int x, int y, const char *str, mu_Color c);
/** @brief As @ref mui_string, with every font pixel blown up @p scale times. */
void mui_string_scaled(const mui_Surface *s, int x, int y, const char *str,
                       int scale, mu_Color c);
///@}

/**
 * @brief Set the colour the screen shows where no window covers it.
 *
 * Defaults to a dark slate -- distinct from MU_COLOR_WINDOWBG, so window
 * edges read against it.
 */
void mui_backdrop(mu_Color c);

/** @brief Width in pixels of @p len characters (@p len < 0 means "to the NUL"). */
int mui_text_width(const char *str, int len);
/** @return The line height of the UI font. */
int mui_text_height(void);

/** A custom-draw callback: paint @p rect of @p s however you like. */
typedef void (*mui_DrawFn)(const mui_Surface *s, mu_Rect rect, void *udata);

/**
 * @brief Put a custom-draw callback into the command list at this point.
 *
 * Painted in list order with the clip rect microui would have used, so it
 * layers and clips exactly like a widget would -- which a program drawing
 * into the surface behind microui's back would not. @p udata is not copied.
 */
void mui_draw_custom(mu_Context *ctx, mu_Rect rect, mui_DrawFn fn, void *udata);

/** Per-frame callback: lay out and submit one microui frame. */
typedef void (*mui_FrameFn)(mu_Context *ctx, void *udata);

/**
 * @brief Take the screen and run @p frame until the program quits.
 *
 * Each iteration drains the keyboard and pointer rings into @p ctx, brackets
 * @p frame with mu_begin()/mu_end(), renders the command list, draws the
 * pointer and blits. Esc quits, as it does in every full-screen program here;
 * so does @ref mui_quit from inside the callback.
 *
 * @return 0 on a normal exit, -1 if the screen could not be grabbed (no
 *         framebuffer, or another program is holding it).
 */
int mui_run(mui_FrameFn frame, void *udata);

/** @brief Ask the loop to stop after this frame. */
void mui_quit(void);

/** @name Clocks (the `clock` and `time` syscalls) */
///@{
unsigned mui_ms(void);     /**< Milliseconds of uptime. */
unsigned mui_unix(void);   /**< Seconds since the Unix epoch. */
///@}
