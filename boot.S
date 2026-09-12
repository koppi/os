/**
 * @file boot.S
 * @brief Multiboot headers and the kernel entry point.
 *
 * Carries both a Multiboot 1 and a Multiboot 2 header so the same image boots
 * from a BIOS GRUB and from a UEFI one, and asks the loader for a linear
 * framebuffer without pinning a mode — a laptop panel's GOP offers only its own
 * native size, and the kernel reads whatever width and pitch it is handed.
 * @c _start sets up the boot stack and calls @c kernel_main with the Multiboot
 * magic and info pointer.
 */
#define ASM_FILE        1

#include <multiboot.h>
#include <multiboot2.h>

    /* 0 = "no preference": take whatever mode the loader / firmware GOP is
     * already in. A fixed 1280x1024 is not offered by a laptop panel's GOP
     * (the X250 is 1366x768), and the kernel handles any width/pitch. */
    .set CONFIG_BFB_WIDTH, 0
    .set CONFIG_BFB_HEIGHT, 0
    .set CONFIG_BFB_BPP, 32
    
    .set ALIGN,    1 << 0
    .set MEMINFO,  1 << 1
    .set VIDINFO,  1 << 2
    
    .set FLAGS,    ALIGN | MEMINFO | VIDINFO
    .set MAGIC,    MULTIBOOT_HEADER_MAGIC
    .set CHECKSUM, -(MAGIC + FLAGS)

    /* Declare Multiboot 1 header. */
    .section .multiboot
    .align 4
multiboot_header:
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

    .long multiboot_header, _start, stack_bottom, stack_bottom, _start

    .long 0 # 0 = set graphics mode
    .long CONFIG_BFB_WIDTH, CONFIG_BFB_HEIGHT, CONFIG_BFB_BPP /* Width, height, depth */
multiboot_header_end:

    /*  Align 64 bits boundary. */
    .section .multiboot2
    .align  8
        
    /*  Declare Multiboot2 header. */
multiboot2_header:
    /*  magic */
    .long   MULTIBOOT2_HEADER_MAGIC
    /*  ISA: i386 */
    .long   MULTIBOOT2_HEADER_ARCH_I386
    /*  Header length. */
    .long   multiboot2_header_end - multiboot2_header
    /*  checksum */
    .long   -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_HEADER_ARCH_I386 + (multiboot2_header_end - multiboot2_header))

    /* Information request tag */
    .align 8
tag_info_req_start:
    .word MULTIBOOT2_TAG_INFO_REQ
    .word MULTIBOOT2_FLAGS_REQUIRED
    .long tag_info_req_end - tag_info_req_start
    .long MULTIBOOT2_TAG_CMDLINE
    .long MULTIBOOT2_TAG_MODULE
    .long MULTIBOOT2_TAG_MEMMAP
tag_info_req_end:

    /* Entry address tag */
	.align 8
tag_entry_address_start:
	.word MULTIBOOT2_TAG_ENTRY_ADDRESS
	.word MULTIBOOT2_FLAGS_REQUIRED
	.long tag_entry_address_end - tag_entry_address_start
	.long _start
tag_entry_address_end:

   	/* Flags tag */
	.align 8
tag_flags_start:
	.word MULTIBOOT2_TAG_FLAGS
	.word MULTIBOOT2_FLAGS_REQUIRED
	.long tag_flags_end - tag_flags_start
	.long MULTIBOOT2_FLAGS_CONSOLE
tag_flags_end:

    /* Framebuffer tag */
	.align 8
tag_framebuffer_start:
    .word MULTIBOOT2_TAG_FRAMEBUFFER
    .word MULTIBOOT2_FLAGS_REQUIRED
    .long tag_framebuffer_end - tag_framebuffer_start
    .long CONFIG_BFB_WIDTH
    .long CONFIG_BFB_HEIGHT
    .long CONFIG_BFB_BPP
tag_framebuffer_end:

	/* Module alignment tag */
	.align 8
tag_module_align_start:
	.word MULTIBOOT2_TAG_MODULE_ALIGN
	.word MULTIBOOT2_FLAGS_REQUIRED
	.long tag_module_align_end - tag_module_align_start
	.long 0
tag_module_align_end:

    /* Tag terminator */
    .align 8
tag_terminator_start:
    .word MULTIBOOT2_TAG_TERMINATOR
    .word MULTIBOOT2_FLAGS_REQUIRED
    .long tag_terminator_end - tag_terminator_start
tag_terminator_end:
multiboot2_header_end:

    /* The kernel entry point. */
    .section .text
    .global _start
    .type _start, @function
_start:
    movl $stack_top, %esp

    /* Make sure our stack is 16-byte aligned */
    and $-16, %esp

    /* Call global constructors. */
    call _init

    pushl %ebx
    pushl %eax

    cli

	/* Transfer control to the main kernel. */
	call kernel_main

	/* Hang if kernel_main unexpectedly returns. */
	cli
1:
	hlt
	jmp 1b
.size _start, . - _start

    /* Reserve a stack for the initial thread. */
    .section .bss, "aw", @nobits
    .align 4
stack_bottom:
    .skip 0x4000
stack_top:

