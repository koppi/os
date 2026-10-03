ENTRY(_start)
SECTIONS {
	. = 8M;

	.text BLOCK(4K) : ALIGN(4K) {
		*(.text .text.*)
	}

	.rodata BLOCK(4K) : ALIGN(4K) {
		*(.rodata .rodata.*)
	}

	/* Global constructor pointers, walked by _start() in lib/cxx_start.c
	 * before main() runs. The tracker has real ones: the FontManager and
	 * the screens' static tables. */
	.init_array BLOCK(4K) : ALIGN(4K) {
		__init_array_start = .;
		KEEP(*(.init_array))
		KEEP(*(.ctors))
		__init_array_end = .;
	}

	.data BLOCK(4K) : ALIGN(4K) {
		*(.data .data.*)
	}

	.bss BLOCK(4K) : ALIGN(4K) {
		*(COMMON)
		*(.bss .bss.*)
		*(.bootstrap_stack)
	}
}
