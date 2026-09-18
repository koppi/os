/**
 * @file multiboot.h
 * @brief Multiboot 1 header magic/flags and the boot information structures.
 * @see https://www.gnu.org/software/grub/manual/multiboot/
 */
#pragma once

#include <multiboot_memmap_struct.h>
#include <multiboot_info_struct.h>

#define MULTIBOOT_HEADER_MAGIC  0x1badb002
#define MULTIBOOT_HEADER_FLAGS  0x00010003

#define MULTIBOOT_LOADER_MAGIC  0x2badb002

#define MULTIBOOT_INFO_FLAGS_MEM	0x01
#define MULTIBOOT_INFO_FLAGS_BOOT	0x02
#define MULTIBOOT_INFO_FLAGS_CMDLINE	0x04
#define MULTIBOOT_INFO_FLAGS_MODS	0x08
#define MULTIBOOT_INFO_FLAGS_SYMS1	0x10
#define MULTIBOOT_INFO_FLAGS_SYMS2	0x20
#define MULTIBOOT_INFO_FLAGS_MMAP	0x40
#define MULTIBOOT_INFO_FLAGS_FB		0x1000  /**< framebuffer_* fields are valid. */

#ifndef __ASSEMBLER__

//#include <stddef.h>
#include <types.h>
#include <memmap.h>

/** Convert 32-bit multiboot address to a pointer. */
#define MULTIBOOT_PTR(mba)  ((void *) (uintptr_t) (mba))

/** Multiboot 32-bit address. */
typedef uint32_t mbaddr_t;

/** Multiboot module structure */
typedef struct {
	mbaddr_t start;
	mbaddr_t end;
	mbaddr_t string;
	uint32_t reserved;
} __attribute__((packed)) multiboot_module_t;

extern void multiboot_cmdline(const char *);

extern void multiboot_extract_command(char *, size_t, const char *);
extern void multiboot_extract_argument(char *, size_t, const char *);
extern void multiboot_info_parse(const multiboot_info_t *);

#endif /* __ASSEMBLER__ */
