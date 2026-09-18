/**
 * @file smp_asm.h
 * @brief Assembly stubs for the Local APIC interrupt handlers (smp_asm.asm).
 */
#pragma once

/** LAPIC timer tick handler (asm). */
void lapic_timer_int(void);
/** TLB-shootdown IPI handler (asm). */
void ipi_tlb_int(void);
/** Reschedule IPI handler (asm). */
void ipi_resched_int(void);
/** LAPIC spurious-interrupt handler (asm). */
void lapic_spurious_int(void);
/** LAPIC error-interrupt handler (asm). */
void lapic_error_int(void);
