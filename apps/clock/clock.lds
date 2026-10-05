/*
 * clock (ring 3). Same layout as every other program here: linked at 8 MiB,
 * which is KERNEL_SPACE_END (mm.h) -- the ELF loader copies each PT_LOAD
 * segment to its linked address and the main thread's stack goes immediately
 * above the image, so .bss is free to be large (this one holds a 250 KiB
 * microui context and a 256 KiB frame).
 */
ENTRY(main)

SECTIONS {
	. = 8M;

	.text BLOCK(4K) : ALIGN(4K) {
		*(.text .text.*)
		*(.gnu.linkonce.t.*)
	}

	.rodata BLOCK(4K) : ALIGN(4K) {
		*(.rodata .rodata.*)
		*(.gnu.linkonce.r.*)
	}

	.data BLOCK(4K) : ALIGN(4K) {
		*(.data .data.*)
		*(.got .got.plt)
		*(.gnu.linkonce.d.*)
	}

	.bss BLOCK(4K) : ALIGN(4K) {
		*(COMMON)
		*(.bss .bss.*)
		*(.bootstrap_stack)
	}
}
