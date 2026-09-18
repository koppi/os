/*
 * Direct `int 0x72` syscall stubs for the koppi-os Lua port.
 *
 * Single-asm-statement form with explicit register constraints: the two-step
 * "set ebx, then call syscall_call" pattern in lib sources only survives
 * because those files are built at -O0; the shim is built at -Os.
 *
 * ABI: eax = call number, ebx/ecx/edx = args, result in eax.
 */
#ifndef LUA_SHIM_KSYS_H
#define LUA_SHIM_KSYS_H

#ifdef LUA_PORT_HOST      /* native build for testing: syscalls are stubbed */
unsigned long __host_ksys(int n, unsigned long a, unsigned long b, unsigned long c);
static inline unsigned long ksys0(int n) { return __host_ksys(n, 0, 0, 0); }
static inline unsigned long ksys1(int n, unsigned long a) { return __host_ksys(n, a, 0, 0); }
static inline unsigned long ksys2(int n, unsigned long a, unsigned long b) { return __host_ksys(n, a, b, 0); }
static inline unsigned long ksys3(int n, unsigned long a, unsigned long b, unsigned long c) { return __host_ksys(n, a, b, c); }
#else
static inline unsigned long ksys0(int n) {
    unsigned long r;
    __asm__ volatile ("int $0x72" : "=a"(r) : "a"(n) : "memory");
    return r;
}
static inline unsigned long ksys1(int n, unsigned long a) {
    unsigned long r;
    __asm__ volatile ("int $0x72" : "=a"(r) : "a"(n), "b"(a) : "memory");
    return r;
}
static inline unsigned long ksys2(int n, unsigned long a, unsigned long b) {
    unsigned long r;
    __asm__ volatile ("int $0x72" : "=a"(r) : "a"(n), "b"(a), "c"(b) : "memory");
    return r;
}
static inline unsigned long ksys3(int n, unsigned long a, unsigned long b,
                                  unsigned long c) {
    unsigned long r;
    __asm__ volatile ("int $0x72"
                      : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}
#endif

enum {
    SYS_GETS   = 1,
    SYS_EXIT   = 4,
    SYS_FOPEN  = 6,
    SYS_FCLOSE = 7,
    SYS_MALLOC = 9,
    SYS_FREE   = 10,
    SYS_REALLOC= 11,
    SYS_WRITE  = 12,
    SYS_FREAD  = 13,
    SYS_TIME   = 14,
    SYS_CLOCK  = 15,
};

#endif
