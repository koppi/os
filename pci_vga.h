/**
 * @file pci_vga.h
 * @brief QEMU / Bochs standard VGA (1234:1111): identify the adapter, report
 *        its linear framebuffer and drive the Bochs DISPI mode registers.
 *
 * The boot framebuffer set up in [video.c](video.c) from the multiboot tag is
 * left alone; this is an identification handler plus a mode-setting hook for
 * later use.
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief Record the framebuffer / MMIO BARs and log the current DISPI mode. */
void bochs_vga_probe(struct pci_device *d);

/** @return Non-zero once @ref bochs_vga_probe has run. */
int bochs_vga_present(void);

/** @return Physical base of the linear framebuffer (BAR0), or 0. */
uint32_t bochs_vga_lfb(void);

/** @brief Read the active DISPI mode into @p w / @p h / @p bpp (any may be NULL). */
void bochs_vga_get_mode(uint32_t *w, uint32_t *h, uint32_t *bpp);

/**
 * @brief Program a linear-framebuffer DISPI mode.
 * @return 1 on success, 0 if no Bochs VGA was found.
 * @note Does not touch the kernel's framebuffer mapping — the caller must remap
 *       @ref bochs_vga_lfb() for the new geometry before drawing.
 */
int bochs_vga_set_mode(uint32_t w, uint32_t h, uint32_t bpp);
