/**
 * @file kconsole.h
 * @brief Kernel console output sink.
 */
#pragma once

#include <chardev.h>

/**
 * @brief The character device kernel output is written to.
 *
 * Set during early boot (to the serial UART in main.c).
 */
extern chardev_t *kconsole;

/**
 * @brief Emit one character to every kernel output surface.
 *
 * Called by the printf implementation. Writes to @ref kconsole, the on-screen
 * log buffer and the VGA text console.
 *
 * @param c Character to output.
 */
void putchar_(char c);
