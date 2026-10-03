/*
 * Direct `int 0x72` syscall stubs, same form and ABI as apps/doom/shim/ksys.h:
 * one asm statement with explicit constraints, because the two-step "set ebx,
 * then call syscall_call" idiom in lib/ only survives at -O0.
 *
 * eax = call number, ebx/ecx/edx = arguments, result in eax.
 */
#ifndef CHIPNOMAD_SHIM_KSYS_H
#define CHIPNOMAD_SHIM_KSYS_H

#ifdef __cplusplus
extern "C" {
#endif

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
    SYS_GETS        = 1,
    SYS_EXIT        = 4,
    SYS_FOPEN       = 6,
    SYS_FCLOSE      = 7,
    SYS_WRITE       = 12,
    SYS_FREAD       = 13,
    SYS_TIME        = 14,
    SYS_CLOCK       = 15,
    SYS_SPIT        = 16,
    SYS_RUN         = 18,
    SYS_GETCWD      = 19,
    SYS_LISTDIR     = 20,
    SYS_GFX_OPEN    = 22,
    SYS_GFX_CLOSE   = 23,
    SYS_GFX_PALETTE = 24,
    SYS_GFX_BLIT    = 25,
    SYS_GETSCAN     = 26,
    SYS_MSLEEP      = 27,
    SYS_SND_OPEN    = 28,
    SYS_SND_CLOSE   = 29,
    SYS_SND_WRITE   = 30,
    SYS_SND_AVAIL   = 31,
};

/* getscan (#26) result bits; mirrors KBD_RAW_* in the kernel's keyboard.h. */
#define KSCAN_BREAK 0x0080
#define KSCAN_E0    0x0100
#define KSCAN_VALID 0x10000

/* The kernel's `file` handle (vfs.h). Only len and type are read here. */
typedef struct {
    char     name[32];
    unsigned flags, len, eof, dev, current_cluster, type;
} kfile_t;

#define KFS_FILE 0
#define KFS_DIR  1

#ifdef __cplusplus
}
#endif

#endif
