/**
 * @file multiboot_memmap_struct.h
 * @brief One entry of the Multiboot 1 memory map (a size-prefixed E820 record).
 */
#pragma once

#include <memmap_struct.h>

#define MULTIBOOT_MEMMAP_OFFSET_SIZE     0x00
#define MULTIBOOT_MEMMAP_OFFSET_MM_INFO  0x04
#define MULTIBOOT_MEMMAP_SIZE            0x18

#define MULTIBOOT_MEMMAP_SIZE_SIZE       0x04

#ifndef __ASSEMBLER__

#include <types.h>

typedef struct multiboot_memmap {
	uint32_t size;
	e820memmap_t mm_info;
} __attribute__((packed)) multiboot_memmap_t;

#endif /* __ASSEMBLER__ */

