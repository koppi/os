ENTRY(_start)
SECTIONS {
	. = 8M;

	.text BLOCK(4K) : ALIGN(4K) {
		*(.text .text.*)
	}

	.rodata BLOCK(4K) : ALIGN(4K) {
		*(.rodata .rodata.*)
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
		*(.data .data.*)
	}

	/* The TLS segment image (lib/tls.c copies it for every thread). Alignment must stay 16:
	 * that is what lib/tls.c lays the per-thread block out for. .tbss takes no space here. */
	.tdata : ALIGN(16) {
		__tdata_start = .;
		*(.tdata .tdata.* .gnu.linkonce.td.*)
		__tdata_end = .;
	}
	.tbss : ALIGN(16) {
		*(.tbss .tbss.* .gnu.linkonce.tb.*)
		*(.tcommon)
		__tbss_end = .;
	}
	ASSERT(ALIGNOF(.tdata) <= 16 && ALIGNOF(.tbss) <= 16, "thread_local alignment above 16 is not supported")

	.bss BLOCK(4K) : ALIGN(4K) {
		*(COMMON)
		*(.bss .bss.*)
		*(.bootstrap_stack)
	}
}
