ENTRY(_start)
SECTIONS {
	. = 8M;

	.text BLOCK(4K) : ALIGN(4K) {
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K) {
		*(.rodata)
	}

	/* Global constructor pointers, walked by _start() in cxx_start.c
	 * before main() runs. */
	.init_array BLOCK(4K) : ALIGN(4K) {
		__init_array_start = .;
		KEEP(*(.init_array))
		KEEP(*(.ctors))
		__init_array_end = .;
	}

	.data BLOCK(4K) : ALIGN(4K) {
		*(.data)
	}

	.bss BLOCK(4K) : ALIGN(4K) {
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}
}
