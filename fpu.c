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
