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
