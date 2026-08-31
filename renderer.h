/**
 * @file renderer.h
 * @brief microui rendering back end — maps microui draw commands onto the
 *        framebuffer primitives in video.c.
 */
#pragma once

#include "microui.h"

/** @brief Initialise the renderer. */
void r_init(void);
/** @brief Fill @p rect with @p color. */
void r_draw_rect(mu_Rect rect, mu_Color color);
/** @brief Draw @p text at @p pos in @p color. */
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
/** @brief Draw the built-in icon @p id inside @p rect. */
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
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
