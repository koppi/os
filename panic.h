/**
 * @file panic.h
 * @brief Unrecoverable-error handler.
 */
#pragma once

/**
 * @brief Print a printf-style message, then disable interrupts and halt.
 *
 * Intended for unrecoverable kernel errors; it never returns.
 *
 * @param format printf-style format string.
 * @param ...    Arguments for @p format.
 */
void panic(const char *format, ...);
