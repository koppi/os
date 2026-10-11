/**
 * @file crtn.S
 * @brief Epilogue halves of @c _init and @c _fini.
 *
 * Linked after crtend.o; closes the stack frames opened in crti.S so the
 * assembled .init/.fini sections return cleanly.
 */
.section .init
	/* gcc will nicely put the contents of crtend.o's .init section here. */
	popl %ebp
	ret

.section .fini
	/* gcc will nicely put the contents of crtend.o's .fini section here. */
	popl %ebp
	ret
