/**
 * @file cmdline.h
 * @brief The kernel command line passed by the bootloader (GRUB), and simple
 *        space-delimited flag lookups.
 *
 * Escape hatches for real-hardware bring-up: append a word to the GRUB
 * `multiboot2` line to skip a driver that misbehaves. Recognised words:
 *   noxhci  nousb  noahci  nonvme  nonet  nosmp  nofb  nohda / nosound  nosyn
 *
 * Opt-in: `ehci` enables the USB 2.0 (EHCI) driver — off by default because the
 * BIOS→OS handoff can disturb the PS/2 keyboard on a real ThinkPad (X220).
 */
#pragma once

/** The raw command line (empty string if none). */
extern char kernel_cmdline[256];

/** @return non-zero if @p word appears as a space-delimited token. */
int cmdline_has(const char *word);
