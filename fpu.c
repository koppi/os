/**
 * @file fpu.c
 * @brief x87 FPU / SSE bring-up.
 */
#include <fpu.h>

#include <types.h>

/**
 * @brief Turn on the x87 FPU and SSE so their instructions don't fault.
 *
 * - @c clts clears the task-switched flag.
 * - CR0: clear EM (bit 2) so x87 ops execute natively, set MP (bit 1).
 * - CR4: set OSFXSR and OSXMMEXCPT (bits 9,10) to enable SSE and its
 *   exception reporting.
 * - @c fninit resets the FPU to a known state.
 */
void fpu_init() {
    size_t t;

    asm("clts");
    asm("mov %%cr0, %0" : "=r"(t));
    t &= ~(1 << 2);
    t |= (1 << 1);
    asm("mov %0, %%cr0" :: "r"(t));
    asm("mov %%cr4, %0" : "=r"(t));
    t |= 3 << 9;
    asm("mov %0, %%cr4" :: "r"(t));
    asm("fninit");
}

/**
 * @brief Fill @p dst with the FXSAVE image of a freshly initialised FPU.
 *
 * Saves and restores the caller's own FPU state around the probe so it is safe
 * to call at any time (the scheduler calls it once per thread creation).
 *
 * @p dst must be @ref FPU_STATE_ALIGN aligned. The scratch area is @c static and
 * aligned by the linker: @c fxsave / @c fxrstor #GP on an unaligned operand and
 * the kernel does not keep its stacks 16-byte aligned.
 */
void fpu_default_state(void *dst) {
    static uint8_t saved[FPU_STATE_SIZE] __attribute__((aligned(FPU_STATE_ALIGN)));

    asm volatile("fxsave (%0)" :: "r"(saved) : "memory");
    asm volatile("fninit");
    asm volatile("fxsave (%0)" :: "r"((uint8_t *) dst) : "memory");
    asm volatile("fxrstor (%0)" :: "r"(saved) : "memory");
}
