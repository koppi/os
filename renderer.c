/**
 * @file renderer.c
 * @brief microui rendering back end — translates microui draw commands into
 *        the framebuffer primitives in video.c. Text is a fixed 8x16 cell.
 */
#include <renderer.h>

#include <video.h>

/** @brief Renderer init (nothing to do — the framebuffer is already up). */
void r_init(void) {
}

/** Pack 8-bit r/g/b into a 0x00RRGGBB pixel. */
#define RGB(r, g, b) (((uint32_t) r) << 16) | (((uint32_t) g) << 8) | ((uint32_t) b)

/** @brief Fill @p rect with @p color. */
void r_draw_rect(mu_Rect rect, mu_Color color) {
    draw_rect(rect.x, rect.y, rect.w, rect.h, RGB(color.r, color.g, color.b));
}

/** @brief Draw @p text at @p pos in @p color. */
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
    (void)color;
    draw_string(pos.x, pos.y, text, RGB(color.r, color.g, color.b));
}

/** @brief Draw one of the built-in microui icons centred in @p rect. */
void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    int c = 0, w = 0, h = 0;
    mu_Vec2 pos;
    char buf[2];
    switch (id) {
    case MU_ICON_CLOSE: c = 'x'; break;
    case MU_ICON_CHECK: c = 'X'; break;
    case MU_ICON_COLLAPSED: c = '>'; break;
    case MU_ICON_EXPANDED: c = 'v'; break;
    }
    buf[0] = c; buf[1] = 0;
    w = r_get_text_width(buf, 1);
    h = r_get_text_height();
    pos.x = rect.x + (rect.w - w) / 2;
    pos.y = rect.y + (rect.h - h) / 2;
    r_draw_text(buf, pos, color);
}


/** @brief @return Width of @p len characters (8 px per cell). */
int r_get_text_width(const char *text, int len) {
    (void)text;
    return 8*len;
}

/** @brief @return The UI line height (16 px). */
int r_get_text_height(void) {
    return 16;
}

/** @brief Set the clip rectangle (currently a no-op). */
void r_set_clip_rect(mu_Rect rect) {
    (void)rect;
    //draw_rect(rect.x, rect.y, rect.w, rect.h, 0xffffff);
}

/** @brief Clear the surface (no-op; the desktop repaints fully each frame). */
void r_clear(mu_Color clr) {
    (void)clr;
}

/** @brief Present the frame (no-op; video.c owns the buffer swap). */
void r_present(void) {
}
