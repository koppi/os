/**
 * @file bfb.h
 * @brief "Boot framebuffer" — the linear framebuffer handed over by the
 *        bootloader (via the multiboot framebuffer tag).
 *
 * @c bfb_addr is 0 when the loader gave us no framebuffer, in which case the
 * kernel falls back to VGA text mode.
 */
#pragma once

#include <types.h>

extern uintptr_t bfb_addr;      /**< Physical address of the framebuffer, or 0. */
extern uint32_t  bfb_width;     /**< Width in pixels. */
extern uint32_t  bfb_height;    /**< Height in pixels. */
extern uint16_t  bfb_bpp;       /**< Bits per pixel. */
extern uint32_t  bfb_scanline;  /**< Bytes per scanline (pitch). */

extern uint8_t bfb_red_pos;     /**< Bit position of the red channel. */
extern uint8_t bfb_red_size;    /**< Bit width of the red channel. */

extern uint8_t bfb_green_pos;   /**< Bit position of the green channel. */
extern uint8_t bfb_green_size;  /**< Bit width of the green channel. */

extern uint8_t bfb_blue_pos;    /**< Bit position of the blue channel. */
extern uint8_t bfb_blue_size;   /**< Bit width of the blue channel. */

extern uint32_t bfb_span;       /**< Framebuffer size in bytes (pitch*height), set by vbe_init. */

/** @return First / last page-directory slot the framebuffer mapping spans, or
 *          -1 when there is no framebuffer. Used by create_address_space() so a
 *          panic from a user-process context can still paint the screen. */
int bfb_pde_lo(void);
int bfb_pde_hi(void);
