/**
 * @file assert.h
 * @brief Debug-build assertion macro.
 */
#pragma once

#include <log.h>

#ifdef DEBUG

/**
 * @brief Halt the machine with a log message if @p x is false.
 *
 * Active only when @c DEBUG is defined (it always is in this build). In a
 * non-debug build it expands to a no-op.
 *
 * @param x Expression that must be non-zero.
 */
#define assert(x) \
do { \
	if (!(x)) { \
		klogf(LOG_CRIT, "Assertion failed: " #x "\n"); \
        halt(); \
	} \
} while (0) \

#else
#define assert(x) while(0)
#endif

/** @brief Marker for unreached / unfinished code paths; fails an assert. */
inline void notImplemented() { assert(0); };
