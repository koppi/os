/**
 * @file crti.S
 * @brief Prologue halves of @c _init and @c _fini.
 *
 * Linked before crtbegin.o so the compiler's own .init/.fini fragments land
 * between this and crtn.S, which closes the frames. Together they give the
 * kernel working global constructors.
 */
.section .init
.global _init
.type _init, @function
_init:
	push %ebp
	movl %esp, %ebp
	/* gcc will nicely put the contents of crtbegin.o's .init section here. */

.section .fini
.global _fini
.type _fini, @function
_fini:
	push %ebp
	movl %esp, %ebp
	/* gcc will nicely put the contents of crtbegin.o's .fini section here. */
