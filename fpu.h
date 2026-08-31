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
