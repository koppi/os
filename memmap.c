/**
 * @file memmap.c
 * @brief Storage for the parsed BIOS E820 memory map.
 */
#include <memmap.h>

/** Number of valid entries currently in @ref e820table. */
uint8_t e820counter = 0;

/** Parsed E820 memory-map entries, filled by the multiboot parser. */
e820memmap_t e820table[MEMMAP_E820_MAX_RECORDS];
