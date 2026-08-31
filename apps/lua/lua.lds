ENTRY(main)

SECTIONS {
	. = 8M;

	.text BLOCK(4K) : ALIGN(4K) {
		*(.text .text.*)
		*(.init .fini)
		*(.gnu.linkonce.t.*)
	}

	.rodata BLOCK(4K) : ALIGN(4K) {
		*(.rodata .rodata.*)
		*(.gnu.linkonce.r.*)
	}

	.data BLOCK(4K) : ALIGN(4K) {
		*(.data .data.*)
		*(.got .got.plt)
		*(.data.rel .data.rel.* .data.rel.ro .data.rel.ro.*)
		*(.gnu.linkonce.d.*)
	}

	.bss BLOCK(4K) : ALIGN(4K) {
		*(COMMON)
		*(.bss .bss.*)
		*(.gnu.linkonce.b.*)
		*(.bootstrap_stack)
	}

	/DISCARD/ : {
		*(.eh_frame .eh_frame_hdr)
		*(.note .note.*)
		*(.comment)
		*(.debug_*)
	}
}
