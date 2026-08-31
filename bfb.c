/**
 * @file bfb.c
 * @brief Storage for the boot-framebuffer parameters.
 *
 * All fields are zero until the multiboot2 parser fills them in from the
 * framebuffer tag; @c bfb_addr == 0 means "no framebuffer, use VGA text".
 */
#include <bfb.h>

uintptr_t bfb_addr = 0;
uint32_t bfb_width = 0;
uint32_t bfb_height = 0;
uint16_t bfb_bpp = 0;
uint32_t bfb_scanline = 0;

uint8_t bfb_red_pos = 0;
uint8_t bfb_red_size = 0;

uint8_t bfb_green_pos = 0;
uint8_t bfb_green_size = 0;

uint8_t bfb_blue_pos = 0;
uint8_t bfb_blue_size = 0;
