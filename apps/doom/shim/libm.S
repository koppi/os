/*
 * x87 <math.h> subset for the koppi-os Doom port.
 *
 * Hand-written: GCC's inline x87 asm with "t"/"u" constraints miscompiles
 * multi-step FPU sequences, so the register stack is managed explicitly here.
 *
 * cdecl / i386: double args at 4(%esp) and 12(%esp); double result in st0;
 * the x87 stack is left holding only the result.
 */
	.text

	.globl fabs
fabs:
	fldl   4(%esp)
	fabs
	ret

	.globl sqrt
sqrt:
	fldl   4(%esp)
	fsqrt
	ret

/* floor / ceil / trunc share a rounding-mode trampoline. */
	.globl floor
floor:
	movw   $0x0400, %cx        /* RC = 01: toward -inf */
	jmp    __rndint
	.globl ceil
ceil:
	movw   $0x0800, %cx        /* RC = 10: toward +inf */
	jmp    __rndint
	.globl trunc
trunc:
	movw   $0x0c00, %cx        /* RC = 11: toward zero */
__rndint:
	fldl   4(%esp)
	subl   $8, %esp
	fnstcw (%esp)
	movzwl (%esp), %eax
	andw   $0xf3ff, %ax
	orw    %cx, %ax
	movw   %ax, 4(%esp)
	fldcw  4(%esp)
	frndint
	fldcw  (%esp)
	addl   $8, %esp
	ret

	.globl fmod
fmod:
	fldl   12(%esp)            /* y */
	fldl   4(%esp)             /* x, y */
1:	fprem
	fnstsw %ax
	testw  $0x0400, %ax        /* C2 set: partial remainder */
	jnz    1b
	fstp   %st(1)              /* drop y */
	ret

	.globl ldexp
ldexp:
	fildl  12(%esp)           /* (double)e */
	fldl   4(%esp)            /* x, e */
	fscale                    /* x * 2^trunc(e) */
	fstp   %st(1)
	ret

/*
 * double frexp(double x, int *e)
 *   x at 4(%esp), e at 12(%esp)
 */
	.globl frexp
frexp:
	movl   8(%esp), %edx       /* high word of x */
	movl   %edx, %ecx
	andl   $0x7ff00000, %ecx
	jz     .Lfr_special         /* +-0 or subnormal */
	cmpl   $0x7ff00000, %ecx
	je     .Lfr_special         /* inf / nan */
	fldl   4(%esp)
	fxtract                    /* st0 = mant [1,2), st1 = exp */
	fxch   %st(1)              /* st0 = exp, st1 = mant */
	subl   $8, %esp
	fistpl (%esp)              /* store exp, pop */
	movl   (%esp), %eax
	addl   $8, %esp
	incl   %eax                /* mant scaled to [0.5,1): exp + 1 */
	movl   12(%esp), %ecx
	movl   %eax, (%ecx)
	pushl  $0x3f000000         /* 0.5f */
	flds   (%esp)
	addl   $4, %esp
	fmulp  %st, %st(1)         /* mant * 0.5 */
	ret
.Lfr_special:
	movl   12(%esp), %ecx
	movl   $0, (%ecx)
	fldl   4(%esp)
	ret

/*
 * __exp2: st0 (finite y) -> st0 = 2**y.   Internal; x87 stack must be otherwise
 * empty on entry.
 */
__exp2:
	fld    %st(0)              /* y, y */
	frndint                    /* i, y     (default CW: round to nearest) */
	fxch   %st(1)              /* y, i */
	fsub   %st(1), %st(0)      /* f = y - i ; st1 = i */
	f2xm1                      /* 2^f - 1 ; i */
	fld1
	faddp  %st, %st(1)         /* 2^f ; i */
	fscale                     /* 2^f * 2^trunc(i) ; i */
	fstp   %st(1)
	ret

	.globl exp
exp:
	fldl   4(%esp)
	fldl2e
	fmulp  %st, %st(1)         /* x * log2(e) */
	call   __exp2
	ret

	.globl log
log:
	fldln2                     /* ln2 */
	fldl   4(%esp)             /* x, ln2 */
	fyl2x                      /* ln2 * log2(x) */
	ret

	.globl log10
log10:
	fldlg2                     /* log10(2) */
	fldl   4(%esp)
	fyl2x
	ret

/*
 * double __pow_pos(double x, double y)  -- requires x > 0.
 *   x**y = 2**(y * log2 x)
 */
	.globl __pow_pos
__pow_pos:
	fldl   12(%esp)           /* y */
	fldl   4(%esp)            /* x, y */
	fyl2x                     /* y * log2(x) */
	call   __exp2
	ret

	.globl sin
sin:
	fldl   4(%esp)
	fsin
	ret

	.globl cos
cos:
	fldl   4(%esp)
	fcos
	ret

	.globl tan
tan:
	fldl   4(%esp)
	fptan                     /* 1.0, tan(x) */
	fstp   %st(0)             /* drop 1.0 */
	ret

/* double atan2(double y, double x) : y at 4(%esp), x at 12(%esp) */
	.globl atan2
atan2:
	fldl   4(%esp)            /* y */
	fldl   12(%esp)           /* x, y */
	fpatan                    /* atan2(y, x) */
	ret

	.section .note.GNU-stack,"",@progbits
