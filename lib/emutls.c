/**
 * @file lib/emutls.c
 * @brief GCC's "emulated TLS" runtime (__emutls_get_address /
 *        __emutls_register_common), for -fno-tls-model code that still
 *        declares a real `thread_local` C++ variable -- real Qt6 source
 *        does this for QBindingStorage (its property-binding system).
 *
 * Real ELF/x86 `thread_local` codegen accesses storage through the %gs
 * segment register, assuming the OS has set up a per-thread Thread
 * Control Block %gs points at (arch_prctl(ARCH_SET_GS) on real Linux).
 * This kernel has no such mechanism -- accessing %gs:0 here reads
 * whatever garbage (or nothing at all) happens to be at physical
 * address 0, which is exactly the page fault this file exists to fix.
 *
 * GCC has a second, portable code generation model for `thread_local`
 * specifically for targets without real ELF TLS support: "emulated
 * TLS" (-femulated-tls), which routes every access through this
 * well-documented two-function runtime interface instead of %gs. It is
 * normally provided by libgcc's own emutls.o, but this host toolchain's
 * -m32 libgcc does not build with it (a straight `nm` on
 * `$(gcc -m32 -print-libgcc-file-name)` turns up no __emutls_* symbols
 * at all -- most Linux-hosted GCCs simply assume real ELF TLS is always
 * available and never build this fallback), so it has to be provided
 * here instead. Genuinely real, backed by this kernel's own
 * pthread_key_create/pthread_getspecific/pthread_setspecific (already
 * real, already glibc-ABI-shaped, from lib/pthread_glibc.c) -- one
 * pthread key per distinct thread_local variable, lazily created on
 * first access and protected by a single global lock against the race
 * of two threads initializing the same variable's key simultaneously.
 */
#include <string.h>

extern void *malloc(size_t size);
extern void mutex_init(void *m);
extern void mutex_lock(void *m);
extern void mutex_unlock(void *m);

typedef unsigned int pthread_key_t;
extern int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
extern int pthread_setspecific(pthread_key_t key, const void *value);
extern void *pthread_getspecific(pthread_key_t key);

/* Matches GCC's own documented layout (gcc/gcc/emutls.c upstream) --
 * every __emutls_* call site GCC generates references a
 * `struct __emutls_object` with exactly this shape. */
struct __emutls_object {
    unsigned int size;
    unsigned int align;
    union {
        unsigned int index;
        void *address;
    } loc;
    void *templ;
};

/* This kernel's real mutex_t (include/lib/mutex.h) is just
 * `{ volatile int locked; }` -- one int, matched here directly rather
 * than pulling in that header (its extern "C" wrapping is meant for
 * C++ consumers, not a plain .c file like this one). */
static int g_emutls_lock_storage;
static int g_emutls_lock_ready = 0;

static void ensure_lock_ready(void) {
    if (!g_emutls_lock_ready) {
        mutex_init(&g_emutls_lock_storage);
        g_emutls_lock_ready = 1;
    }
}

void *__emutls_get_address(struct __emutls_object *obj) {
    ensure_lock_ready();
    mutex_lock(&g_emutls_lock_storage);
    if (obj->loc.index == 0) {
        pthread_key_t key;
        pthread_key_create(&key, 0);
        /* Real pthread_key_t values start at 0 on this target, which
         * this object's own "not yet initialized" sentinel also uses
         * -- store the key biased by one so 0 unambiguously means
         * "no key yet" to every thread's first access. */
        obj->loc.index = key + 1;
    }
    pthread_key_t key = (pthread_key_t) (obj->loc.index - 1);
    mutex_unlock(&g_emutls_lock_storage);

    void *p = pthread_getspecific(key);
    if (!p) {
        p = malloc(obj->size);
        if (obj->templ)
            memcpy(p, obj->templ, obj->size);
        else
            memset(p, 0, obj->size);
        pthread_setspecific(key, p);
    }
    return p;
}

/* Only reached for a `thread_local` variable GCC could not size at
 * compile time (a VLA-shaped one) -- real Qt6/C++ source this closure
 * vendors never declares one, so this is provided for ABI completeness
 * (any TU that references __emutls_get_address must find this symbol
 * too, since real libgcc.a always ships them as a pair) rather than
 * because it is genuinely reachable here. */
void __emutls_register_common(struct __emutls_object *obj, unsigned int size,
                               unsigned int align, void *templ) {
    if (obj->size < size) {
        obj->size = size;
        obj->align = align;
        obj->templ = templ;
    }
}
