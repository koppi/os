/*
 * i386 System V setjmp/longjmp for the koppi-os Lua port.
 *
 * jmp_buf layout (6 x 32-bit words): ebx, esi, edi, ebp, esp, return-eip.
 * Callee-saved registers only; eax/ecx/edx are caller-saved so Lua does not
 * expect them preserved across the longjmp.
 */
	.text

	.globl setjmp
	.globl _setjmp
	.type  setjmp, @function
setjmp:
_setjmp:
	movl  4(%esp), %eax        /* env */
	movl  %ebx,   0(%eax)
	movl  %esi,   4(%eax)
	movl  %edi,   8(%eax)
	movl  %ebp,  12(%eax)
	leal  4(%esp), %ecx        /* esp as it will be after `ret` */
	movl  %ecx,  16(%eax)
	movl  (%esp), %ecx         /* return address */
	movl  %ecx,  20(%eax)
	xorl  %eax, %eax
	ret
	.size setjmp, .-setjmp

	.globl longjmp
	.globl _longjmp
	.type  longjmp, @function
longjmp:
_longjmp:
	movl  4(%esp), %edx        /* env */
	movl  8(%esp), %eax        /* val */
	testl %eax, %eax
	jnz   1f
	incl  %eax                 /* longjmp(env, 0) must return 1 */
1:
	movl   0(%edx), %ebx
	movl   4(%edx), %esi
	movl   8(%edx), %edi
	movl  12(%edx), %ebp
	movl  16(%edx), %esp
	movl  20(%edx), %ecx
	jmp   *%ecx
	.size longjmp, .-longjmp

	.section .note.GNU-stack,"",@progbits
