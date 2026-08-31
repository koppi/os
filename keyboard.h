/**
 * @file keyboard.h
 * @brief PS/2 keyboard driver: IRQ-driven scancode decode into a ring buffer.
 */
#pragma once

#include <io.h>
#include <types.h>

/** @brief Install the keyboard IRQ handler and enable the PS/2 port. */
void keyboard_init();
/** @brief (declared, unused) whether the keyboard is enabled. */
uint8_t keyboard_enabled();
/** @brief IRQ-context handler: decode one scancode into the ring buffer. */
void keyboard_read_key();
/** @brief Peek the oldest buffered key without consuming it (0 if empty). */
char keyboard_get_lastkey();
/** @brief Consume the key last returned by @ref keyboard_get_lastkey. */
void keyboard_invalidate_lastkey();
/** @brief Block until one key is available, then return and consume it. */
char getchar();
/**
 * @brief Read a line into @p str (used by the scanf syscall).
 * @param str  Destination buffer.
 * @param size Buffer size in bytes.
 */
void gets(char *str, size_t size);
