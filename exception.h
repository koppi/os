/**
 * @file exception.h
 * @brief CPU-exception handlers (vectors 0-19).
 *
 * The `*_handle` symbols are the assembly stubs in exception_asm.asm that save
 * a register frame and call the matching C handler here. A fault in kernel mode
 * panics; a fault in user mode kills the offending process via
 * @ref return_exception.
 */
#pragma once

#include <proc.h>
#include <type.h>

/** @brief Handler for any vector without a dedicated one. */
void default_ir_handler();

extern void invop_handle();   /**< asm stub for #UD (invalid opcode). */
extern void gpf_handle();     /**< asm stub for #GP (general protection). */
extern void pf_handle();      /**< asm stub for #PF (page fault). */
extern void syscall_handle(); /**< asm stub for the `int 0x72` syscall gate. */

/**
 * @brief Return into userspace at @ref RETURN_ADDR with a non-zero status,
 *        terminating the current process after a fatal user-mode fault.
 */
void return_exception();

void ex_divide_by_zero();                  /**< #DE  vector 0  */
void ex_single_step();                     /**< #DB  vector 1  */
void ex_nmi();                             /**< NMI  vector 2  */
void ex_breakpoint();                      /**< #BP  vector 3  */
void ex_overflow();                        /**< #OF  vector 4  */
void ex_bounds_check();                    /**< #BR  vector 5  */
void ex_invalid_opcode(struct regs *re);   /**< #UD  vector 6  */
void ex_device_not_available();            /**< #NM  vector 7  */
void ex_double_fault();                    /**< #DF  vector 8  */
/* 9 reserved */
void ex_invalid_tss();                     /**< #TS  vector 10 */
void ex_segment_not_present();             /**< #NP  vector 11 */
void ex_stack_fault();                     /**< #SS  vector 12 */
void ex_gpf(struct regs_error *re);        /**< #GP  vector 13 */
void ex_page_fault(struct regs_error *re); /**< #PF  vector 14 */
/* 15 reserved */
void ex_fpu_error();                       /**< #MF  vector 16 */
void ex_alignment_check();                 /**< #AC  vector 17 */
void ex_machine_check();                   /**< #MC  vector 18 */
void ex_simd_fpu();                        /**< #XM  vector 19 */
/* 20 - 31 reserved */
