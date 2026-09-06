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

/**
 * @brief Execute @c cpuid for @p leaf (sub-leaf 0).
 *
 * The @c xchg pair keeps this correct whether or not the compiler reserves
 * @c ebx for the PIC base (it does under @c -fPIE, GCC's default on many
 * toolchains), which a plain @c "=b" clobber would clash with.
 */
void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    uint32_t ra, rb, rc, rd;
    asm volatile("xchg %%ebx, %1\n\t"
                 "cpuid\n\t"
                 "xchg %%ebx, %1"
                 : "=a"(ra), "=&r"(rb), "=c"(rc), "=d"(rd)
                 : "0"(leaf), "2"(0));
    if (a) *a = ra;
    if (b) *b = rb;
    if (c) *c = rc;
    if (d) *d = rd;
}

/** @brief Read model-specific register @p msr. */
uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t) hi << 32) | lo;
}

/** @brief Write @p lo/@p hi to model-specific register @p msr. */
void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi) {
    asm volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}
