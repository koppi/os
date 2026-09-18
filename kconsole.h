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

/**
 * @brief Optional extra output sink, fed the same characters as @ref putchar_.
 *
 * Set by ssh.c while a session is bridging the console to a remote shell, so
 * command output (which always goes through the global printf/putchar_ path)
 * is mirrored to the SSH channel as well as the local screen. NULL when no
 * session is active.
 */
extern void (*ssh_output_hook)(char c);
