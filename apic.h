/**
 * @file apic.h
 * @brief Local APIC: enable, EOI, timer and inter-processor interrupts.
 *
 * The LAPIC MMIO block is identity-mapped 1:1 on demand (it sits above the low
 * 4 MiB map, like the ACPI tables and e1000 BAR). @ref apic_init runs on the
 * BSP and calibrates the per-CPU LAPIC timer against the PIT; each CPU then
 * calls @ref lapic_enable. IPIs drive rescheduling and TLB shootdown.
 */
#pragma once

#include <types.h>

/** Global LAPIC base, exposed for the asm EOI in smp_asm.asm. */
extern uint32_t lapic_base;

/** @brief Map the LAPIC MMIO, calibrate the timer (BSP only) and enable it. */
void apic_init(void);

/** @brief Enable interrupts on this CPU's LAPIC and unmask its timer. */
void lapic_enable(void);

/** @brief Write the End-Of-Interrupt register. */
void lapic_eoi(void);

/** @return This CPU's local APIC id. */
uint32_t lapic_id(void);

/** @brief Read the raw 32-bit LAPIC register at offset @p off. */
uint32_t lapic_read(uint32_t off);

/** @brief Write a 32-bit value to LAPIC register @p off. */
void lapic_write(uint32_t off, uint32_t val);

/** @brief Send a fixed IPI to every CPU except the local one. */
void lapic_ipi_allbutself(uint8_t vector);

/** @brief Send a fixed IPI to the CPU with LAPIC id @p dest. */
void lapic_ipi_to(uint32_t dest, uint8_t vector);

/** @brief Send an INIT IPI to the CPU with LAPIC id @p dest. */
void lapic_send_init(uint32_t dest);

/** @brief Send a Start-up IPI (SIPI) to the CPU with LAPIC id @p dest. */
void lapic_send_sipi(uint32_t dest);

/** @brief Start this CPU's LAPIC timer at roughly 1 kHz. */
void lapic_timer_start(void);

/** @brief One-shot calibration against the PIT (BSP, called from apic_init). */
void lapic_timer_calibrate(void);

/** @brief Mask/unmask this CPU's LAPIC timer interrupt. */
void lapic_timer_mask(int mask);
