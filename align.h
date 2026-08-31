/**
 * @file align.h
 * @brief Power-of-two alignment helper macros.
 */
#pragma once

/** Align to the nearest lower address.
 *
 * @param s Address or size to be aligned.
 * @param a Size of alignment, must be power of 2.
 */
#define ALIGN_DOWN(s, a)  ((s) & ~((a) - 1))

/** Align to the nearest higher address.
 *
 * @param s Address or size to be aligned.
 * @param a Size of alignment, must be power of 2.
 */
#define ALIGN_UP(s, a)  (((s) + ((a) - 1)) & ~((a) - 1))

/** Check alignment.
 *
 * @param s Address or size to be checked for alignment.
 * @param a Size of alignment, must be a power of 2.
 */
#define IS_ALIGNED(s, a)  (ALIGN_UP((s), (a)) == (s))
