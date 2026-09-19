/*
 * Direct `int 0x72` syscall stubs for the koppi-os Doom port.
 *
 * Single-asm-statement form with explicit register constraints: the two-step
 * "set ebx, then call syscall_call" pattern in lib/ sources only survives
 * because those files are built at -O0; the shim is built at -O2.
 *
 * ABI: eax = call number, ebx/ecx/edx = args, result in eax.
 */
#ifndef DOOM_SHIM_KSYS_H
#define DOOM_SHIM_KSYS_H

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

enum {
    SYS_GETS    = 1,
    SYS_EXIT    = 4,
    SYS_FOPEN   = 6,
    SYS_FCLOSE  = 7,
    SYS_MALLOC  = 9,
    SYS_FREE    = 10,
    SYS_REALLOC = 11,
    SYS_WRITE   = 12,
    SYS_FREAD   = 13,
    SYS_TIME    = 14,
    SYS_CLOCK   = 15,
    SYS_SPIT    = 16,
    /* Added for this port -- see syscall.c and video.c. */
    SYS_GFX_OPEN    = 22,
    SYS_GFX_CLOSE   = 23,
    SYS_GFX_PALETTE = 24,
    SYS_GFX_BLIT    = 25,
    SYS_GETSCAN     = 26,
    SYS_MSLEEP      = 27,
};

/* getscan (#26) result bits; mirrors KBD_RAW_* in the kernel's keyboard.h. */
#define KSCAN_BREAK 0x0080
#define KSCAN_E0    0x0100
#define KSCAN_VALID 0x10000

#endif
