/**
 * @file multiboot_info_struct.h
 * @brief The Multiboot 1 information block passed by the loader in EBX, and
 *        the field offsets used by the assembly boot stub.
 */
#pragma once

#define MULTIBOOT_INFO_OFFSET_FLAGS        0x00
#define MULTIBOOT_INFO_OFFSET_MEM_LOWER    0x04
#define MULTIBOOT_INFO_OFFSET_MEM_UPPER    0x08
#define MULTIBOOT_INFO_OFFSET_BOOT_DEVICE  0x0c
#define MULTIBOOT_INFO_OFFSET_CMD_LINE     0x10
#define MULTIBOOT_INFO_OFFSET_MODS_COUNT   0x14
#define MULTIBOOT_INFO_OFFSET_MODS_ADDR    0x18
#define MULTIBOOT_INFO_OFFSET_SYMS         0x1c
#define MULTIBOOT_INFO_OFFSET_MMAP_LENGTH  0x2c
#define MULTIBOOT_INFO_OFFSET_MMAP_ADDR    0x30
#define MULTIBOOT_INFO_SIZE                0x34

#ifndef __ASSEMBLER__

#include <types.h>

typedef struct multiboot_info {
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmd_line;
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t syms[4];
	uint32_t mmap_length;
	uint32_t mmap_addr;

    /* Drive Info buffer */
    uint32_t drives_length;
    uint32_t drives_addr;
    
    /* ROM configuration table */
    uint32_t config_table;
    
    /* Boot Loader Name */
    uint32_t boot_loader_name;
    
    /* APM table */
    uint32_t apm_table;
    
    /* Video */
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED      0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB          1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT     2
    uint8_t framebuffer_type;

} __attribute__((packed)) multiboot_info_t;

#endif /* __ASSEMBLER__ */

