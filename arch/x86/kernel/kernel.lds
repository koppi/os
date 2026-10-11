ENTRY(_start)

SECTIONS {
  . = 0x100000;
    
  kernel_start = .;

  /* Multiboot header plus code. */
  .text : {
    *(.multiboot)
    *(.multiboot2)
    code = .;
    *(.text)
  }

  /* Read-write data (initialized). */
  .data ALIGN(0x1000) : {
    data = .;
    *(.data)
  }

  /* Read-only data. */
  .rodata ALIGN(0x1000) : {
    rodata = .;
    *(.rodata)
  }

  /* Read-write data (uninitialized) and stack. */
  .bss ALIGN(0x1000) : {
    bss = .;
    *(COMMON)
    *(.bss)
  }

  /DISCARD/ : {
    *(.comment)
    *(.eh_frame)
    *(.note.gnu.build-id)
  }

  end = .;
  kernel_end = .;
}

/*
 * The image must stay clear of everything the kernel maps at a fixed address
 * inside the identity-mapped low region. It is checked here, at link time,
 * because the one time it was not the failure was a boot that stopped dead at
 * the scheduler with nothing on the serial line -- the page-directory /
 * RETURN_ADDR collision described in proc.h, which appeared when the image
 * grew by a few kilobytes and would have reappeared the next time it did.
 *
 * The first thing above the image is the page-table storage window
 * (paging.c): it starts at kernel_end and wants MAX_BLOCKS * 4 KiB = 512 KiB,
 * and paging_init() clamps it at KPROC_STACK_BASE (mm.h, 0x4C0000) where the
 * kernel threads' stacks begin. Leaving it less than its full length still
 * boots -- it warns -- so the hard limit asserted here is the stacks
 * themselves.
 */
ASSERT(kernel_end < 0x4C0000,
       "kernel image has grown into the kernel-thread stacks (KPROC_STACK_BASE, mm.h)")
ASSERT(kernel_end < 0x7FF000,
       "kernel image has grown into the RETURN_ADDR trampoline page (proc.h)")
