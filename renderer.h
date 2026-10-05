/**
 * @file renderer.h
 * @brief microui rendering back end — maps microui draw commands onto the
 *        framebuffer primitives in video.c.
 */
#pragma once

#include <types.h>

#include "microui.h"

/** @brief Initialise the renderer. */
void r_init(void);
/** @brief Fill @p rect with @p color. */
void r_draw_rect(mu_Rect rect, mu_Color color);
/** @brief Draw @p text at @p pos in @p color. */
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
/** @brief Draw the built-in icon @p id inside @p rect. */
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
/**
 * @brief Paint a ring-3 program's 8-bpp frame into @p dst through its palette.
 *
 * @param dst   Where the window's body landed on the desktop.
 * @param src   @p sw * @p sh palette indices -- the program's last frame.
 * @param sw,sh The program's surface size.
 * @param pal   Its own 256 entries of 0x00RRGGBB.
 *
 * Drawn 1:1 from @p dst's top-left and cut off at the clip rectangle, which
 * is what lets a userspace program be a window rather than a program that has
 * taken the screen (see wm.h).
 */
void r_draw_surface(mu_Rect dst, const uint8_t *src, int sw, int sh,
                    const uint32_t *pal);
/** @return Pixel width of the first @p len chars of @p text. */
int r_get_text_width(const char *text, int len);
/** @return Line height of the UI font. */
int r_get_text_height(void);
/** @brief Restrict subsequent drawing to @p rect. */
void r_set_clip_rect(mu_Rect rect);
/** @brief Clear the whole surface to @p color. */
void r_clear(mu_Color color);
/** @brief Present the finished frame. */
void r_present(void);
