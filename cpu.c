/**
 * @file cpu.c
 * @brief CPU introspection helpers.
 */
#include <cpu.h>

/**
 * @brief Read the CPU time-stamp counter via the @c rdtsc instruction.
 *
 * The @c "=A" constraint returns the EDX:EAX pair as a single 64-bit value.
 *
 * @return The 64-bit cycle count since reset.
 */
uint64_t rdtsc() {
    uint64_t result;
    asm volatile("rdtsc" : "=A"(result));
    return result;
}
