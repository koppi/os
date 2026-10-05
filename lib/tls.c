/**
 * @file lib/tls.c
 * @brief ELF thread-local storage (the i386 local-exec model) for ring-3 programs.
 *
 * GCC has no emulated-TLS switch for this target, so `thread_local` / `__thread` compile to
 * the real thing: `%gs:-N` for a variable, `%gs:0` for the thread pointer. What the platform
 * has to provide is
 *
 *  - an image of the program's TLS segment (linker script: `.tdata`, `.tbss` and the symbols
 *    `__tdata_start`, `__tdata_end`, `__tbss_end`, see third_party/qt6-gui/koppios/qt_app.lds),
 *  - one copy of it per thread, laid out as [ .tdata | .tbss ][ TCB ] with the thread pointer at
 *    the TCB (variant II: the linker computes variable offsets *below* the thread pointer from
 *    the segment size rounded up to its alignment, which this file fixes at 16), and
 *  - a %gs whose base is that thread pointer: syscall 38 (set_thread_area) hands out a per-thread
 *    GDT descriptor and returns its selector, which is loaded here.
 *
 * Everything is weak-referenced from its callers (cxx_start.c, pthread_glibc.c): a program whose
 * linker script has no TLS symbols never calls into any of this and keeps %gs flat.
 *
 * `__cxa_thread_atexit` registers the destructors of thread_local objects that have one; they
 * run, newest first, when the thread ends (__tls_thread_exit) -- including the main thread, from
 * _start after main() returns.
 */
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/system_calls.h>

extern char __tdata_start[] __attribute__((weak));
extern char __tdata_end[] __attribute__((weak));
extern char __tbss_end[] __attribute__((weak));
/* lib/pthread_glibc.c, weak: POSIX key destructors of the ending thread. */
extern void __pthread_run_key_dtors(void) __attribute__((weak));

#define TLS_ALIGN 16u
/** The flat ring-3 data selector every thread starts with (GDT slot 4, RPL 3). */
#define USER_DS 0x23

struct tls_dtor {
    void (*fn)(void *);
    void *obj;
    struct tls_dtor *next;
};

/** Thread control block: `%gs:0` is its own address (the ABI's thread pointer). */
struct tcb {
    struct tcb *self;
    void *raw;               /**< What malloc returned, for free(). */
    struct tls_dtor *dtors;  /**< Pending thread_local destructors, newest first. */
    unsigned pad;
};

/** @return This thread's TCB, or 0 if it has not set one up (%gs still flat). */
static struct tcb *tcb_self(void) {
    unsigned short gs;
    asm volatile("movw %%gs, %0" : "=r"(gs));
    if(gs == USER_DS || gs == 0)
        return 0;
    struct tcb *t;
    asm volatile("movl %%gs:0, %0" : "=r"(t));
    return t;
}

int __tls_thread_init(void) {
    if(!__tdata_start || !__tbss_end)
        return 0;                                   /* no TLS in this program */
    unsigned memsz = (unsigned) (__tbss_end - __tdata_start);
    if(memsz == 0)
        return 0;
    unsigned aligned = (memsz + TLS_ALIGN - 1) & ~(TLS_ALIGN - 1);

    unsigned char *raw = malloc(aligned + sizeof(struct tcb) + TLS_ALIGN);
    if(!raw)
        return -1;
    unsigned char *block = (unsigned char *) (((unsigned) raw + TLS_ALIGN - 1) & ~(TLS_ALIGN - 1));
    unsigned tdata = (unsigned) (__tdata_end - __tdata_start);
    memcpy(block, __tdata_start, tdata);
    memset(block + tdata, 0, aligned - tdata);

    struct tcb *t = (struct tcb *) (block + aligned);
    t->self = t;
    t->raw = raw;
    t->dtors = 0;

    int sel = (int) syscall3(38, (unsigned) t, 0, 0);
    if(sel < 0) {
        free(raw);
        return -1;
    }
    asm volatile("movw %w0, %%gs" :: "r"(sel) : "memory");
    return 0;
}

/** Run this thread's thread_local destructors, newest first. A destructor may itself touch (or
 *  register more) thread_local objects, so keep going until the list is empty. */
static void run_cxx_dtors(struct tcb *t) {
    while(t->dtors) {
        struct tls_dtor *d = t->dtors;
        t->dtors = d->next;
        d->fn(d->obj);
        free(d);
    }
}

void __tls_main_exit(void) {
    struct tcb *t = tcb_self();
    if(t)
        run_cxx_dtors(t);       /* like exit(): the main thread's thread_local destructors only */
}

void __tls_thread_exit(void) {
    struct tcb *t = tcb_self();
    if(t)
        run_cxx_dtors(t);
    /* Key destructors (they may read thread_local variables, so the block must still be there),
     * as glibc does after the C++ destructors. */
    if(__pthread_run_key_dtors)
        __pthread_run_key_dtors();
    if(!t)
        return;
    void *raw = t->raw;
    asm volatile("movw %w0, %%gs" :: "r"(USER_DS) : "memory");
    free(raw);
}

int __cxa_thread_atexit(void (*fn)(void *), void *obj, void *dso) {
    (void) dso;
    struct tcb *t = tcb_self();
    if(!t)
        return -1;
    struct tls_dtor *d = malloc(sizeof(*d));
    if(!d)
        return -1;
    d->fn = fn;
    d->obj = obj;
    d->next = t->dtors;
    t->dtors = d;
    return 0;
}
