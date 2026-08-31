/**
 * @file fpu.h
 * @brief x87 FPU / SSE bring-up.
 */
#pragma once

/**
 * @brief Enable the x87 FPU and SSE and issue @c fninit.
 *
 * Clears CR0.EM, sets CR0.MP and CR4.OSFXSR|OSXMMEXCPT so FPU and SSE
 * instructions do not trap.
 */
void fpu_init();

/** Size of an FXSAVE image, and the alignment it requires. */
#define FPU_STATE_SIZE   512
#define FPU_STATE_ALIGN  16

/**
 * @brief Copy a clean (post-@c fninit) FXSAVE image into @p dst.
 *
 * The scheduler seeds every thread's saved FPU area with this so the first
 * @c fxrstor on a context switch loads a valid state. @p dst must be
 * @ref FPU_STATE_SIZE bytes and 16-byte aligned.
 */
void fpu_default_state(void *dst);
