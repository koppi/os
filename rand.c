/**
 * @file rand.c
 * @brief Linear-congruential PRNG state and generators.
 */
#include <rand.h>

/** LCG state. */
uint32_t random_seed = 1;

/** @brief Advance the LCG and return a pseudo-random value. */
uint32_t rand() {
	random_seed = random_seed * 1103515245 + 12345;
	return (uint32_t)(random_seed / 63336) % 4294967295;
}

/** @brief Fold @p seed into the state and return a value in [0, @p max]. */
uint32_t maxrand(uint32_t seed, uint32_t max) {
	random_seed = random_seed + seed * 1103515245 + 12345;
	return (uint32_t)(random_seed / 65536) % (max+1);
}
