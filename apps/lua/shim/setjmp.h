/*
 * Minimal <setjmp.h> for the freestanding koppi-os Lua port.
 *
 * The host glibc header maps setjmp() to _setjmp() and uses a 156-byte
 * sigmask-aware jmp_buf; this OS has no signals, so a plain 6-word buffer
 * (ebx, esi, edi, ebp, esp, return eip) is enough. Implementation in port_asm.S.
 */
#ifndef LUA_SHIM_SETJMP_H
#define LUA_SHIM_SETJMP_H

typedef unsigned long __jmp_buf[6];
typedef __jmp_buf jmp_buf;

int  setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val) __attribute__((noreturn));

/* POSIX spellings, in case anything reaches for them. */
#define _setjmp(e)      setjmp(e)
#define _longjmp(e, v)  longjmp((e), (v))

#endif
