/**
 * @file cmdline.h
 * @brief The kernel command line passed by the bootloader (GRUB), and simple
 *        space-delimited flag lookups.
 *
 * Escape hatches for real-hardware bring-up: append a word to the GRUB
 * `multiboot2` line to skip a driver that misbehaves. Recognised words:
 *   noxhci  noehci  nousb  noahci  nonvme  nonet  nosmp  nofb
 *   nohda / nosound  nosyn
 */
#pragma once

/** The raw command line (empty string if none). */
extern char kernel_cmdline[256];

/** @return non-zero if @p word appears as a space-delimited token. */
int cmdline_has(const char *word);
