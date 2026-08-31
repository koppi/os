/**
 * @file cpu.h
 * @brief CPU introspection helpers.
 */
#pragma once

#include <types.h>

/**
 * @brief Read the CPU time-stamp counter.
 * @return The 64-bit count of CPU cycles since reset.
 */
uint64_t rdtsc();
