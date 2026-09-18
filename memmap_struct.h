/**
 * @file memmap_struct.h
 * @brief The normalised E820 memory-map entry type plus the range-type enum
 *        and a helper to stringify it for boot logging.
 *
 * The `#define` offsets mirror the raw on-wire E820 layout for the assembly
 * boot code.
 */
#pragma once

#define E820MEMMAP_OFFSET_BASE_ADDRESS  0x00
#define E820MEMMAP_OFFSET_SIZE          0x08
#define E820MEMMAP_OFFSET_TYPE          0x10
#define E820MEMMAP_SIZE                 0x14

#define E820MEMMAP_SIZE_SIZE            0x08

#ifndef __ASSEMBLER__

#include <types.h>

typedef struct e820memmap {
	uint64_t base_address;
	uint64_t size;
	uint32_t type;
} __attribute__((packed)) e820memmap_t;

enum e820_type {
	E820_TYPE_RAM = 1,
	E820_TYPE_RESERVED = 2,
	E820_TYPE_ACPI = 3,
	E820_TYPE_NVS = 4,
	E820_TYPE_UNUSABLE = 5,
	E820_TYPE_DISABLED = 6,
	E820_TYPE_PERSISTENT = 7,
};

// Convert an e820 entry type to string for debug output.
static inline const char *e820_type_to_string(enum e820_type type)
{
	switch (type) {
	case E820_TYPE_RAM:
		return "RAM";
	case E820_TYPE_RESERVED:
		return "reserved";
	case E820_TYPE_ACPI:
		return "ACPI";
	case E820_TYPE_NVS:
		return "NVS";
	case E820_TYPE_UNUSABLE:
		return "unusable";
	case E820_TYPE_DISABLED:
		return "disabled";
	case E820_TYPE_PERSISTENT:
		return "persistent";
	default:
		return "UNKNOWN";
	}
}

#endif
