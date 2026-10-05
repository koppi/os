/**
 * @file apps/microui/mui.h
 * @brief The ring-3 microui runtime: a microui window for a userspace
 *        program, on the desktop or on the whole screen.
 *
 * The kernel's own desktop drives the same vendored microui (../../microui.c)
 * through renderer.c, straight into the 32-bpp framebuffer shadow. A ring-3
 * program cannot touch that shadow. What it gets instead is an 8-bpp indexed
 * surface of its own, presented one frame at a time through a 256-entry
 * palette -- either as a window the desktop composites (the window manager,
 * syscalls 39..43, see wm.h) or, when there is no desktop to put a window on,
 * as the whole screen (the grab, syscalls 22..25, see video.h). This runtime
 * is the renderer and event pump for both --
 *
 *   mui_run()   opens the window, then calls the frame callback in a loop:
 *               input in, one microui frame out, blit, repeat at ~60 Hz, and
 *               gives the window back however the loop ends.
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
 * A program names the size of its *content* and the runtime finds it a
 * surface. The ceiling is what the runtime keeps a frame buffer for; it is
 * also under the kernel's own window limit (@c WM_MAX_W/@c WM_MAX_H) and well
 * under any panel this kernel boots on, which matters for the full-screen
 * fallback -- @c video_blit8 refuses a surface larger than the screen.
 */
///@{
#define MUI_MAX_W 640
#define MUI_MAX_H 480
///@}

/** The 8-bpp surface, as handed to a custom-draw callback. */
typedef struct {
    unsigned char *pix;   /**< @c w * @c h palette indices. */
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

/** Per-frame callback: lay out the window's contents for one frame. */
typedef void (*mui_FrameFn)(mu_Context *ctx, void *udata);

/**
 * @brief Put a @p w x @p h window on screen and run @p frame until it closes.
 *
 * Two ways to get there, tried in that order:
 *
 *   **a window on the desktop** (`wm_open`, syscall 39). The desktop keeps
 *   drawing, the window has a title bar it can be dragged by and a close box,
 *   other programs can have windows at the same time, and the keyboard
 *   reaches this one only while it is focused. The window manager draws the
 *   frame; this program draws only the content.
 *
 *   **the whole screen** (`gfx_open`, syscall 22), when there is no desktop to
 *   put a window on -- a `nofb` boot, or another program already holding the
 *   screen. The same content is drawn with a title bar of its own, centred on
 *   black and scaled up by whatever whole number fits.
 *
 * Either way @p frame is called inside a window that is already open, so it
 * only lays widgets out: no mu_begin_window, no mu_end_window, and nothing
 * that needs to know which of the two is in use.
 *
 * @p w and @p h are the content size, excluding any title bar.
 * Esc quits, so does the window's close box, and so does @ref mui_quit.
 *
 * @return 0 on a normal exit, -1 if neither a window nor the screen could be
 *         had, or if @p w / @p h exceed @ref MUI_MAX_W / @ref MUI_MAX_H.
 */
int mui_run(const char *title, int w, int h, mui_FrameFn frame, void *udata);

/**
 * @brief Ask for the whole screen instead of a window, before @ref mui_run.
 *
 * Normally the choice is made for the program -- a window when there is a
 * desktop, the screen when there is not. This forces the second, which is both
 * what someone who wants the thing full-screen asks for and the only way to
 * exercise that path on a machine whose desktop is working.
 */
void mui_fullscreen(int on);

/** @return Non-zero while the program is a window on the desktop rather than
 *          holding the whole screen. */
int mui_windowed(void);

/** @brief Ask the loop to stop after this frame. */
void mui_quit(void);

/** @name Clocks (the `clock` and `time` syscalls) */
///@{
unsigned mui_ms(void);     /**< Milliseconds of uptime. */
unsigned mui_unix(void);   /**< Seconds since the Unix epoch. */
///@}
