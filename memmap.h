/**
 * @file memmap.h
 * @brief BIOS E820 memory-map range types and the parsed E820 table.
 *
 * Populated from the multiboot memory-map tag; consumed by the physical
 * memory manager to learn which regions are usable RAM.
 */
#pragma once

#include <memmap_struct.h>

/* E820h memory range types */

#define MEMMAP_MEMORY_AVAILABLE  1  /**< Free RAM. */
#define MEMMAP_MEMORY_RESERVED   2  /**< Reserved, not for OS use. */
#define MEMMAP_MEMORY_ACPI       3  /**< ACPI reclaimable after the tables are read. */
#define MEMMAP_MEMORY_NVS        4  /**< ACPI NVS — preserve across sleep. */
#define MEMMAP_MEMORY_UNUSABLE   5  /**< Detected as bad memory. */

#define MEMMAP_E820_RECORD_SIZE  20 /**< Size of one raw E820 entry, in bytes. */
#define MEMMAP_E820_MAX_RECORDS  32 /**< Capacity of @ref e820table. */

#ifndef __ASSEMBLER__

#include <types.h>

extern e820memmap_t e820table[MEMMAP_E820_MAX_RECORDS]; /**< Parsed E820 entries. */
extern uint8_t e820counter;                              /**< Number of valid entries. */

#endif
