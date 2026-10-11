/**
 * @file apps/install/sys.c
 * @brief The ring-3 backends for the installer: see sys.h.
 */
#include "sys.h"

/* Single-asm-statement syscall stubs with explicit register constraints, as in
 * ../microui/mui.c: the two-step "set a register, then int" form only works at -O0. */
static inline unsigned ksys1(int n, unsigned a) {
    unsigned r;
    __asm__ volatile ("int $0x72" : "=a"(r) : "a"(n), "b"(a) : "memory");
    return r;
}
static inline unsigned ksys2(int n, unsigned a, unsigned b) {
    unsigned r;
    __asm__ volatile ("int $0x72" : "=a"(r) : "a"(n), "b"(a), "c"(b) : "memory");
    return r;
}
static inline unsigned ksys4(int n, unsigned a, unsigned b, unsigned c, unsigned d) {
    unsigned r;
    __asm__ volatile ("int $0x72"
                      : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d) : "memory");
    return r;
}

enum {
    SYS_RUN      = 18,
    SYS_TIME     = 14,
    SYS_GETRANDOM = 37,
    SYS_BLK_INFO = 46,
    SYS_BLK_READ = 47,
    SYS_BLK_WRITE = 48,
    SYS_BLK_CTL  = 49
};

int sys_blk_info(unsigned idx, struct blk_info *bi) {
    return ksys2(SYS_BLK_INFO, idx, (unsigned) (unsigned long) bi) == 1;
}

/* The device's context is just its index. The kernel takes at most BLK_MAX_COUNT
 * sectors a call; the engine asks for no more, but split anyway rather than trust that. */
static int blk_read(void *ctx, inst_u32 lba, void *buf, inst_u32 n) {
    unsigned idx = (unsigned) (unsigned long) ctx;
    inst_u8 *p = buf;
    while (n) {
        unsigned c = n > BLK_MAX_COUNT ? BLK_MAX_COUNT : n;
        if (ksys4(SYS_BLK_READ, idx, lba, (unsigned) (unsigned long) p, c) != c)
            return -1;
        lba += c;
        p += c * BLK_SECTOR;
        n -= c;
    }
    return 0;
}

static int blk_write(void *ctx, inst_u32 lba, const void *buf, inst_u32 n) {
    unsigned idx = (unsigned) (unsigned long) ctx;
    const inst_u8 *p = buf;
    while (n) {
        unsigned c = n > BLK_MAX_COUNT ? BLK_MAX_COUNT : n;
        if (ksys4(SYS_BLK_WRITE, idx, lba, (unsigned) (unsigned long) p, c) != c)
            return -1;
        lba += c;
        p += c * BLK_SECTOR;
        n -= c;
    }
    return 0;
}

static int blk_flush(void *ctx) {
    return ksys2(SYS_BLK_CTL, (unsigned) (unsigned long) ctx, BLK_CTL_FLUSH) == 0 ? 0 : -1;
}

void sys_blk_dev(inst_dev *d, unsigned idx, unsigned sectors, int writable) {
    d->ctx = (void *) (unsigned long) idx;
    d->read = blk_read;
    d->write = writable ? blk_write : 0;
    d->flush = writable ? blk_flush : 0;
    d->sectors = sectors;
}

int sys_blk_rescan(unsigned idx) {
    return ksys2(SYS_BLK_CTL, idx, BLK_CTL_RESCAN) == 1;
}

void sys_random(void *buf, unsigned n) {
    inst_u8 *p = buf;
    while (n) {
        unsigned c = n > 256 ? 256 : n;                 /* what getrandom takes at a time */
        if (ksys2(SYS_GETRANDOM, (unsigned) (unsigned long) p, c) != 0) {
            /* No entropy source: fall back on the clock, which is at least different each run. */
            unsigned t = ksys1(SYS_TIME, 0);
            for (unsigned i = 0; i < c; i++)
                p[i] = (inst_u8) (t * 2654435761u >> (i & 15));
        }
        p += c;
        n -= c;
    }
}

unsigned sys_unix(void) {
    return ksys1(SYS_TIME, 0);
}

void sys_reboot(void) {
    ksys1(SYS_RUN, (unsigned) (unsigned long) "reboot");
}
