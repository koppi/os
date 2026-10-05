/**
 * @file keyboard.h
 * @brief PS/2 keyboard driver: IRQ-driven scancode decode into a ring buffer.
 */
#pragma once

#include <io.h>
#include <types.h>

/** @brief Install the keyboard IRQ handler and enable the PS/2 port. */
void keyboard_init();
/** @brief Re-assert the keyboard's 8042 config (translation + IRQ 1) after an
 *         EHCI BIOS->OS handoff may have disturbed it. Safe to call live. */
void keyboard_reinit(void);
/** @brief (declared, unused) whether the keyboard is enabled. */
uint8_t keyboard_enabled();
/** @brief IRQ-context handler: decode one scancode into the ring buffer. */
void keyboard_read_key();
/** @brief Inject a decoded character (used by the USB HID keyboard driver). */
void keyboard_push_char(char c);
/**
 * @brief Inject a character typed on this machine's own keyboard.
 *
 * Same ring, except that it is dropped while a program window has the keyboard
 * (wm.h). The serial line uses @ref keyboard_push_char instead and is never
 * dropped -- see keyboard.c.
 */
void keyboard_push_local_char(char c);

/**
 * @name Cursor keys in the character stream
 *
 * The arrow cluster has no ASCII of its own, which is why it used to be
 * dropped by every path into the console input ring: the PS/2 decoder threw
 * the 0xE0-prefixed codes away, @ref usb_hid.c had no table entry for the
 * usages, and a terminal's escape sequence arrived as its raw bytes -- so the
 * shell's line editor saw `[A` as two keystrokes to insert, or nothing at all.
 *
 * Each path now normalises a cursor key to one of these control characters,
 * the same ones a reader of this ring already has to understand (they are the
 * readline/emacs bindings the shell's editor was written around, so Up and ^P
 * are deliberately the same keystroke to it). One byte rather than an escape
 * sequence keeps the ring's single-character atom intact: a consumer that does
 * not know a code simply ignores it instead of inserting the tail of a
 * sequence it failed to parse. A full-screen program that needs to tell Up
 * from ^P reads the raw scancode ring instead (@ref keyboard_raw_get).
 *
 * The rest of the cluster -- Insert, Delete, Page Up/Down -- has no
 * conventional control character and is not carried here; it is in the raw
 * ring, which the `keys` console command prints.
 */
///@{
#define KBD_CH_UP     0x10   /**< Up    == ^P (previous). */
#define KBD_CH_DOWN   0x0E   /**< Down  == ^N (next).     */
#define KBD_CH_LEFT   0x02   /**< Left  == ^B (back).     */
#define KBD_CH_RIGHT  0x06   /**< Right == ^F (forward).  */
#define KBD_CH_HOME   0x01   /**< Home  == ^A.            */
#define KBD_CH_END    0x05   /**< End   == ^E.            */
///@}

/** @brief Peek the oldest buffered key without consuming it (0 if empty). */
char keyboard_get_lastkey();
/** @brief Consume the key last returned by @ref keyboard_get_lastkey. */
void keyboard_invalidate_lastkey();
/** @brief Block until one key is available, then return and consume it. */
char getchar();
/** @brief Like @ref getchar but silent (no log, no `sti`): backs the getkey
 *         syscall for the userspace line editor. */
char keyboard_getkey(void);
/**
 * @name Raw scancode stream
 *
 * A full-screen program (apps/doom) needs what the ASCII ring cannot carry:
 * key *releases*, and the keys the ASCII map has no entry for (arrows, Ctrl,
 * Alt). The IRQ handler therefore also pushes every scancode onto a second,
 * independent ring. Two rings rather than one consumer switching modes: the
 * ASCII ring has exactly one consumer and a second reader would split the
 * keystrokes between them (see README), so the raw stream is kept apart.
 */
///@{
/** @brief Start/stop recording raw scancodes; both rings are flushed either
 *         way so no keystroke leaks across the mode change. */
void keyboard_raw_mode(int on);
/**
 * @brief Pop one raw key event.
 * @return 0 when the ring is empty, otherwise
 *         @c KBD_RAW_VALID | (extended ? @c KBD_RAW_E0 : 0) |
 *         (released ? @c KBD_RAW_BREAK : 0) | make code.
 */
int keyboard_raw_get(void);
/** @brief Inject a raw key event from a keyboard that is not on the 8042
 *         (the USB HID driver). Ignored unless raw mode is on. */
void keyboard_push_scan(uint8_t code, int e0, int release);
///@}

#define KBD_RAW_BREAK 0x0080   /**< Event is a release, not a press. */
#define KBD_RAW_E0    0x0100   /**< Key arrived with the 0xE0 prefix. */
#define KBD_RAW_VALID 0x10000  /**< Set on a real event (0 means "ring empty"). */
/**
 * @brief Read a line into @p str (used by the scanf syscall).
 * @param str  Destination buffer.
 * @param size Buffer size in bytes.
 */
void gets(char *str, size_t size);
