/* Layout for the Qt apps (image at 8 MiB, same as every other ring-3 program here).
 *
 * The patterns include the ".name.*" forms on purpose: -ffunction-sections/-fdata-sections
 * give every function and variable its own input section, and sections that no pattern
 * names become one *output* section each. Without these patterns the stripped binary carried
 * thousands of output sections and a 1.4 MB section-name table on top of the real 4.7 MB. */
ENTRY(_start)
SECTIONS {
	. = 8M;

	.text BLOCK(4K) : ALIGN(4K) {
		*(.text .text.*)
	}

	.rodata BLOCK(4K) : ALIGN(4K) {
		*(.rodata .rodata.* .rodata1)
		*(.eh_frame .eh_frame.*)
		*(.gcc_except_table .gcc_except_table.*)
	}

	/* Global constructor pointers, walked by _start() in cxx_start.c
	 * before main() runs. */
	.init_array BLOCK(4K) : ALIGN(4K) {
		__init_array_start = .;
		KEEP(*(.init_array .init_array.*))
		KEEP(*(.ctors .ctors.*))
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

	/DISCARD/ : {
		*(.comment)
		*(.note .note.*)
	}
}
