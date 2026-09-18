/**
 * @file acpi.h
 * @brief Minimal ACPI RSDP/RSDT/MADT parsing for CPU discovery.
 *
 * Scans the EBDA and the 0xE0000-0xFFFFF region for the RSDP, walks the RSDT
 * to the MADT, and extracts the LAPIC count, ids and base address. Falls back
 * to a single CPU if no ACPI tables are found.
 */
#pragma once

#include <types.h>

/** @brief Parse ACPI and record CPU count / LAPIC ids. No-op if absent. */
void acpi_init(void);

/** @return The number of application-ready CPUs (>= 1). */
int acpi_cpu_count(void);

/** @return The LAPIC id of CPU @p i (i in [0, acpi_cpu_count())). */
uint32_t acpi_cpu_apicid(int i);

/** @return The memory-mapped LAPIC base (defaults to 0xFEE00000). */
uint32_t acpi_lapic_base(void);

/** @return The BSP's own LAPIC id as reported by ACPI (or 0). */
uint32_t acpi_bsp_apicid(void);

/** @return The mapped MADT ("APIC" table), or 0 if ACPI was not found. */
const void *acpi_madt(void);

/** @return The validated RSDP, or 0. */
const void *acpi_rsdp(void);
