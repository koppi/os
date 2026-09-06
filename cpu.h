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

/**
 * @brief Execute @c cpuid for @p leaf (sub-leaf 0). Any output pointer may be
 *        NULL. EBX-safe under -fPIE.
 */
void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d);

/** @brief Read model-specific register @p msr (EDX:EAX as one 64-bit value). */
uint64_t rdmsr(uint32_t msr);

/** @brief Write @p lo/@p hi (EAX/EDX) to model-specific register @p msr. */
void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi);
