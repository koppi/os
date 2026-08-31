/**
 * @file rand.h
 * @brief Tiny linear-congruential pseudo-random generator.
 */
#pragma once

#include <types.h>

/** @return The next value in the LCG sequence. */
uint32_t rand();
/** @brief Perturb the state with @p seed and return a value in [0, @p max]. */
uint32_t maxrand(uint32_t seed, uint32_t max);

