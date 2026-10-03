/**
 * @file lib/pthread_glibc.c
 * @brief A second pthread implementation, deliberately separate from
 *        lib/pthread.c: that one defines its OWN pthread_t/pthread_mutex_t/
 *        etc. types (see include/lib/pthread.h), which is fine for an app
 *        that only ever calls pthread_* itself, but breaks the moment a
 *        translation unit also pulls in real libstdc++ headers
 *        (<mutex>/<condition_variable>/<atomic>) -- those are compiled
 *        against glibc's REAL pthread ABI and, once both headers are in
 *        the same TU, the two conflicting struct definitions simply fail
 *        to compile. That is exactly what happens building real Qt6
 *        source (qthread_unix.cpp / qwaitcondition_unix.cpp): Qt itself
 *        calls pthread_create() directly AND transitively drags in
 *        std::mutex through <functional>/<unordered_map>.
 *
 *        The fix here is to stop inventing our own struct layouts and
 *        instead build against the real, unmodified system <pthread.h> /
 *        <sched.h> / <unistd.h> / <time.h> for every TYPE and PROTOTYPE
 *        (glibc's pthread_t/pthread_mutex_t/pthread_cond_t/pthread_attr_t
 *        are all opaque -- a plain integer or a fixed-size byte array the
 *        docs explicitly say never to inspect), and provide REAL
 *        definitions for the handful of functions actually needed, backed
 *        by this kernel's own thread_create/thread_join/thread_yield/
 *        thread_self syscalls -- never linked against actual glibc/
 *        libpthread (this whole target is -nostdlib), so nothing here
 *        needs to match glibc's internal algorithms, only its type sizes
 *        (checked with _Static_assert below) and its documented behavior.
 *
 *        Scoped to exactly what real qthread_unix.cpp / qwaitcondition_unix.cpp
 *        and libstdc++'s own <mutex>/<condition_variable>/<atomic> call.
 *        Same simplifications as lib/pthread.c, ported to this ABI:
 *        whole-second timedwait resolution (this kernel's time() syscall
 *        has no finer clock), cooperative/deferred-only cancellation, and
 *        attribute setters that accept but do not act on scheduling
 *        policy/priority (no such kernel knobs are syscall-exposed yet).
 */
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* Quoted, not <lib/...>: this file must not expose include/'s top level
 * as a bare -I search dir (that directory also holds this project's own
 * freestanding stddef.h/stdio.h, which would then shadow the real
 * system <stddef.h>/<stdio.h> that <pthread.h>/<time.h>/... above
 * transitively need -- see the file comment for the general problem
 * this file exists to avoid). */
#include "../include/lib/system_calls.h"
#include "../include/lib/mutex.h"

/* -------------------------------------------------------------------- */
/* Layouts overlaid onto glibc's opaque storage. Every one of these is   */
/* smaller than the real type it is cast onto (checked below), so it     */
/* never reads or writes past what glibc itself allocated for it.        */
/* -------------------------------------------------------------------- */

typedef struct {
    volatile int locked;
    int owner_tid;   /* only meaningful when kind == PTHREAD_MUTEX_RECURSIVE */
    int recursion;
    int kind;
} km_mutex_t;

typedef struct {
    volatile unsigned int seq;
} km_cond_t;

typedef struct {
    int detachstate;
    long stacksize;
    int schedpolicy;
    int schedpriority;
} km_attr_t;

_Static_assert(sizeof(km_mutex_t) <= sizeof(pthread_mutex_t), "km_mutex_t overflows real pthread_mutex_t");
_Static_assert(sizeof(km_cond_t) <= sizeof(pthread_cond_t), "km_cond_t overflows real pthread_cond_t");
_Static_assert(sizeof(km_attr_t) <= sizeof(pthread_attr_t), "km_attr_t overflows real pthread_attr_t");
_Static_assert(sizeof(int) <= sizeof(pthread_mutexattr_t), "int overflows real pthread_mutexattr_t");

#define KM(mutex) ((km_mutex_t *) (void *) (mutex))
#define KC(cond) ((km_cond_t *) (void *) (cond))
#define KA(attr) ((km_attr_t *) (void *) (attr))
#define KMA(attr) ((int *) (void *) (attr))

/* -------------------------------------------------------------------- */
/* errno -- one global cell, not per-thread. Only ever set by the        */
/* ENOSYS-returning stubs below (pipe/dup2/read/write/close/access: this */
/* kernel has no anonymous pipes or fd-based I/O -- see qcore_unix_p.h's */
/* self-pipe-trick users, which real Qt itself only reaches from a real  */
/* event-dispatcher backend this tree does not vendor). Racy across      */
/* threads in principle; harmless in practice since nothing here reads   */
/* errno back after a genuinely concurrent failure.                      */
/* -------------------------------------------------------------------- */

static int g_errno;
int *__errno_location(void) {
    return &g_errno;
}

/* -------------------------------------------------------------------- */
/* Thread create/join/identity                                          */
/* -------------------------------------------------------------------- */

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    (void) attr;
    int tid = thread_create((void *) start_routine, arg);
    if (tid < 0) {
        return 1; /* POSIX wants a positive error number; we have no
                    * distinct errno values to report here. */
    }
    *thread = (pthread_t) tid;
    return 0;
}

int pthread_join(pthread_t thread, void **retval) {
    thread_join((int) thread);
    /* thread_create()/thread_join() have no return-value channel (see
     * lib/system_calls.c), so a joined thread's result is never actually
     * recovered here -- always NULL, not whatever pthread_exit() was given. */
    if (retval) {
        *retval = 0;
    }
    return 0;
}

int pthread_detach(pthread_t thread) {
    /* Every thread here is already "detached" in the only observable
     * sense libstdc++/Qt rely on: thread_join() is safe to skip (the
     * kernel reclaims a finished thread's slot regardless), so there is
     * no resource leak to avoid by tracking detach state for real. */
    (void) thread;
    return 0;
}

pthread_t pthread_self(void) {
    return (pthread_t) thread_self();
}

int pthread_equal(pthread_t a, pthread_t b) {
    return a == b;
}

void pthread_exit(void *retval) {
    (void) retval;
    syscall3(4, 0, 0, 0); /* exit / stop_thread(): does not return */
    for (;;) {
    }
}

/* -------------------------------------------------------------------- */
/* Once                                                                  */
/* -------------------------------------------------------------------- */

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    /* 0 = untouched (PTHREAD_ONCE_INIT), 1 = running, 2 = done. */
    if (__sync_bool_compare_and_swap(once_control, 0, 1)) {
        init_routine();
        __sync_lock_test_and_set(once_control, 2);
    } else {
        while (*once_control != 2) {
            thread_yield();
        }
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/* Attributes -- accepted; scheduling knobs are not backed by anything   */
/* real (no such kernel facility is syscall-exposed yet).                */
/* -------------------------------------------------------------------- */

int pthread_attr_init(pthread_attr_t *attr) {
    KA(attr)->detachstate = PTHREAD_CREATE_JOINABLE;
    KA(attr)->stacksize = 0;
    KA(attr)->schedpolicy = SCHED_OTHER;
    KA(attr)->schedpriority = 0;
    return 0;
}
int pthread_attr_destroy(pthread_attr_t *attr) {
    (void) attr;
    return 0;
}
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    KA(attr)->detachstate = detachstate;
    return 0;
}
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate) {
    *detachstate = KA(attr)->detachstate;
    return 0;
}
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    KA(attr)->stacksize = (long) stacksize;
    return 0;
}
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize) {
    *stacksize = (size_t) KA(attr)->stacksize;
    return 0;
}
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit) {
    (void) attr;
    (void) inherit;
    return 0;
}
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy) {
    KA(attr)->schedpolicy = policy;
    return 0;
}
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy) {
    *policy = KA(attr)->schedpolicy;
    return 0;
}
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param) {
    KA(attr)->schedpriority = param->sched_priority;
    return 0;
}
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param) {
    param->sched_priority = KA(attr)->schedpriority;
    return 0;
}

/* -------------------------------------------------------------------- */
/* Mutex -- same test-and-set + pause + yield-after-spins as mutex.c,    */
/* plus real PTHREAD_MUTEX_RECURSIVE support (std::recursive_mutex needs */
/* it): a recursive mutex remembers its owning thread and a recursion    */
/* count instead of just a flag.                                         */
/* -------------------------------------------------------------------- */

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    *KMA(attr) = PTHREAD_MUTEX_DEFAULT;
    return 0;
}
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    (void) attr;
    return 0;
}
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind) {
    *KMA(attr) = kind;
    return 0;
}
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *kind) {
    *kind = *KMA(attr);
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    KM(mutex)->locked = 0;
    KM(mutex)->owner_tid = 0;
    KM(mutex)->recursion = 0;
    KM(mutex)->kind = attr ? *KMA(attr) : PTHREAD_MUTEX_DEFAULT;
    return 0;
}
int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    (void) mutex;
    return 0;
}

static int km_mutex_trylock(km_mutex_t *m) {
    int self = thread_self();
    if (m->kind == PTHREAD_MUTEX_RECURSIVE && m->locked && m->owner_tid == self) {
        m->recursion++;
        return 0;
    }
    if (__sync_lock_test_and_set(&m->locked, 1) != 0) {
        return 1;
    }
    m->owner_tid = self;
    m->recursion = 1;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    km_mutex_t *m = KM(mutex);
    int spins = 0;
    while (km_mutex_trylock(m) != 0) {
        if (spins++ > 1000) {
            thread_yield();
            spins = 0;
        } else {
            asm volatile("pause");
        }
    }
    return 0;
}
int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    return km_mutex_trylock(KM(mutex)) == 0 ? 0 : EBUSY;
}
int pthread_mutex_timedlock(pthread_mutex_t *__restrict mutex, const struct timespec *__restrict abstime) {
    km_mutex_t *m = KM(mutex);
    while (km_mutex_trylock(m) != 0) {
        /* time() (syscall 14) is whole Unix seconds -- the finest clock
         * this kernel exposes to userspace, so sub-second deadlines round
         * up to the next second rather than being honored exactly. */
        long now = (long) syscall3(14, 0, 0, 0);
        if (now >= abstime->tv_sec) {
            return ETIMEDOUT;
        }
        thread_yield();
    }
    return 0;
}
int pthread_mutex_clocklock(pthread_mutex_t *__restrict mutex, clockid_t clockid, const struct timespec *__restrict abstime) {
    (void) clockid;
    return pthread_mutex_timedlock(mutex, abstime);
}
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    km_mutex_t *m = KM(mutex);
    if (m->kind == PTHREAD_MUTEX_RECURSIVE && m->recursion > 1) {
        m->recursion--;
        return 0;
    }
    m->owner_tid = 0;
    m->recursion = 0;
    __sync_lock_release(&m->locked);
    return 0;
}

/* -------------------------------------------------------------------- */
/* Condition variable -- a sequence counter, exactly like lib/pthread.c: */
/* wait captures the current value while still holding the caller's      */
/* mutex, then polls it after releasing the mutex. signal and broadcast  */
/* are the same operation (every waiter wakes on any increment) -- POSIX */
/* explicitly allows spurious wakeups, so a correct caller re-checks its */
/* own predicate after waking regardless.                                */
/* -------------------------------------------------------------------- */

int pthread_condattr_init(pthread_condattr_t *attr) {
    (void) attr;
    return 0;
}
int pthread_condattr_destroy(pthread_condattr_t *attr) {
    (void) attr;
    return 0;
}
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id) {
    (void) attr;
    (void) clock_id;
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void) attr;
    KC(cond)->seq = 0;
    return 0;
}
int pthread_cond_destroy(pthread_cond_t *cond) {
    (void) cond;
    return 0;
}
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    unsigned int start = KC(cond)->seq;
    pthread_mutex_unlock(mutex);
    while (KC(cond)->seq == start) {
        thread_yield();
    }
    pthread_mutex_lock(mutex);
    return 0;
}
static int km_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    unsigned int start = KC(cond)->seq;
    pthread_mutex_unlock(mutex);
    int timed_out = 0;
    while (KC(cond)->seq == start) {
        long now = (long) syscall3(14, 0, 0, 0);
        if (now >= abstime->tv_sec) {
            timed_out = 1;
            break;
        }
        thread_yield();
    }
    pthread_mutex_lock(mutex);
    return timed_out ? ETIMEDOUT : 0;
}
int pthread_cond_timedwait(pthread_cond_t *__restrict cond, pthread_mutex_t *__restrict mutex, const struct timespec *__restrict abstime) {
    return km_cond_timedwait(cond, mutex, abstime);
}
int pthread_cond_clockwait(pthread_cond_t *__restrict cond, pthread_mutex_t *__restrict mutex, clockid_t clock_id, const struct timespec *__restrict abstime) {
    (void) clock_id;
    return km_cond_timedwait(cond, mutex, abstime);
}
int pthread_cond_signal(pthread_cond_t *cond) {
    __sync_fetch_and_add(&KC(cond)->seq, 1);
    return 0;
}
int pthread_cond_broadcast(pthread_cond_t *cond) {
    __sync_fetch_and_add(&KC(cond)->seq, 1);
    return 0;
}

/* -------------------------------------------------------------------- */
/* Thread-local storage: a small fixed (tid, key) -> value table, not    */
/* compiler-assisted TLS -- see lib/pthread.c's matching comment, which  */
/* applies unchanged here. A separate table from lib/pthread.c's own:    */
/* the two files' pthread_key_t values are not interchangeable (real     */
/* glibc's is unsigned int; lib/pthread.h's is a bare int), so sharing a */
/* table between them would risk key collisions between two unrelated    */
/* ABIs' callers.                                                        */
/* -------------------------------------------------------------------- */

#define PT_MAX_TLS 256

typedef struct {
    int used;
    int tid;
    pthread_key_t key;
    const void *value;
} tls_slot_t;

static tls_slot_t g_tls[PT_MAX_TLS];
static mutex_t g_tls_lock;
static unsigned int g_next_key = 1;
static int g_tls_lock_init = 0;

static void tls_lock_ensure_init(void) {
    if (!g_tls_lock_init) {
        mutex_init(&g_tls_lock);
        g_tls_lock_init = 1;
    }
}

static void tls_set(int tid, pthread_key_t key, const void *value) {
    tls_lock_ensure_init();
    mutex_lock(&g_tls_lock);
    int free_slot = -1;
    for (int i = 0; i < PT_MAX_TLS; i++) {
        if (g_tls[i].used && g_tls[i].tid == tid && g_tls[i].key == key) {
            g_tls[i].value = value;
            mutex_unlock(&g_tls_lock);
            return;
        }
        if (!g_tls[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot >= 0) {
        g_tls[free_slot].used = 1;
        g_tls[free_slot].tid = tid;
        g_tls[free_slot].key = key;
        g_tls[free_slot].value = value;
    }
    mutex_unlock(&g_tls_lock);
}

static void *tls_get(int tid, pthread_key_t key) {
    tls_lock_ensure_init();
    mutex_lock(&g_tls_lock);
    void *result = 0;
    for (int i = 0; i < PT_MAX_TLS; i++) {
        if (g_tls[i].used && g_tls[i].tid == tid && g_tls[i].key == key) {
            result = (void *) g_tls[i].value;
            break;
        }
    }
    mutex_unlock(&g_tls_lock);
    return result;
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    (void) destructor;
    tls_lock_ensure_init();
    mutex_lock(&g_tls_lock);
    *key = g_next_key++;
    mutex_unlock(&g_tls_lock);
    return 0;
}

int pthread_key_delete(pthread_key_t key) {
    tls_lock_ensure_init();
    mutex_lock(&g_tls_lock);
    for (int i = 0; i < PT_MAX_TLS; i++) {
        if (g_tls[i].used && g_tls[i].key == key) {
            g_tls[i].used = 0;
        }
    }
    mutex_unlock(&g_tls_lock);
    return 0;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    tls_set(thread_self(), key, value);
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    return tls_get(thread_self(), key);
}

/* -------------------------------------------------------------------- */
/* Scheduling / naming -- accepted, not backed by anything real (see     */
/* file comment).                                                        */
/* -------------------------------------------------------------------- */

int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param) {
    (void) thread;
    (void) policy;
    (void) param;
    return 0;
}
int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param) {
    (void) thread;
    *policy = SCHED_OTHER;
    param->sched_priority = 0;
    return 0;
}
int pthread_setname_np(pthread_t thread, const char *name) {
    (void) thread;
    (void) name;
    return 0;
}

int sched_yield(void) {
    thread_yield();
    return 0;
}

/* -------------------------------------------------------------------- */
/* clock_gettime(): CLOCK_REALTIME is the RTC's whole-second time() (14).   */
/* Every other clock (MONOTONIC, BOOTTIME, ...) is millisecond uptime from   */
/* the PIT via the clock syscall (15), so Qt's timers and ppoll() timeouts   */
/* get real sub-second resolution. REALTIME keeps tv_nsec = 0 on purpose:    */
/* mixing RTC seconds with PIT milliseconds would let it step backwards.     */
/* -------------------------------------------------------------------- */

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    if (clk_id == CLOCK_REALTIME) {
        tp->tv_sec = (long) syscall3(14, 0, 0, 0);
        tp->tv_nsec = 0;
    } else {
        unsigned int ms = (unsigned int) syscall3(15, 0, 0, 0);
        tp->tv_sec = (long) (ms / 1000u);
        tp->tv_nsec = (long) (ms % 1000u) * 1000000L;
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/* Cancellation: cooperative/deferred only, same as lib/pthread.c. A     */
/* separate TLS table above means cancellation state/requests recorded   */
/* by lib/pthread.c's pthread_cancel() are invisible here and vice       */
/* versa -- not a real gap in practice, since a given translation unit   */
/* links against exactly one of the two pthread implementations.         */
/* -------------------------------------------------------------------- */

#define PT_KEY_CANCEL_REQUESTED ((pthread_key_t) -1)
#define PT_KEY_CANCEL_STATE ((pthread_key_t) -2)

int pthread_cancel(pthread_t thread) {
    tls_set((int) thread, PT_KEY_CANCEL_REQUESTED, (const void *) 1);
    return 0;
}

int pthread_setcancelstate(int state, int *oldstate) {
    int tid = thread_self();
    void *old = tls_get(tid, PT_KEY_CANCEL_STATE);
    if (oldstate) {
        *oldstate = old ? (int) (long) old : PTHREAD_CANCEL_ENABLE;
    }
    tls_set(tid, PT_KEY_CANCEL_STATE, (const void *) (long) state);
    return 0;
}

void pthread_testcancel(void) {
    int tid = thread_self();
    void *state = tls_get(tid, PT_KEY_CANCEL_STATE);
    if (state && (int) (long) state == PTHREAD_CANCEL_DISABLE) {
        return;
    }
    if (tls_get(tid, PT_KEY_CANCEL_REQUESTED)) {
        pthread_exit(0);
    }
}

/* -------------------------------------------------------------------- */
/* POSIX fd-based I/O this kernel does not have: no anonymous pipes, no  */
/* dup(), no fd table, no byte-range read()/write() (see fopen()/        */
/* fread()/write_file() in lib/stdio.c and lib/system_calls.c for what   */
/* this kernel's real, whole-file I/O model looks like instead). Real Qt */
/* only reaches these from qcore_unix_p.h's self-pipe-trick helpers,     */
/* used by a real Unix event-dispatcher backend -- not vendored here.    */
/* Declared (matching real <unistd.h>) so files that merely PARSE those  */
/* inline helpers still compile; defined as honest ENOSYS failures       */
/* rather than left undefined, so linking only breaks if something       */
/* actually calls one of these at runtime.                               */
/* -------------------------------------------------------------------- */

int pipe(int pipefd[2]) {
    (void) pipefd;
    g_errno = ENOSYS;
    return -1;
}
int dup2(int oldfd, int newfd) {
    (void) oldfd;
    (void) newfd;
    g_errno = ENOSYS;
    return -1;
}
extern long simfd_read(int fd, void *buf, unsigned long count);
extern long simfd_write(int fd, const void *buf, unsigned long count);
extern int simfd_close(int fd);

ssize_t read(int fd, void *buf, size_t count) {
    long r = simfd_read(fd, buf, count); /* in-process eventfds, libc_ext.c */
    if (r != -2)
        return r;
    g_errno = ENOSYS;
    return -1;
}
ssize_t write(int fd, const void *buf, size_t count) {
    long r = simfd_write(fd, buf, count);
    if (r != -2)
        return r;
    g_errno = ENOSYS;
    return -1;
}
int close(int fd) {
    int r = simfd_close(fd);
    if (r != -2)
        return r;
    g_errno = ENOSYS;
    return -1;
}
int access(const char *pathname, int mode) {
    (void) pathname;
    (void) mode;
    g_errno = ENOSYS;
    return -1;
}

/* libstdc++'s <bits/atomic_wait.h> uses a raw syscall(SYS_futex, ...) as
 * a fast path when it believes real Linux futexes are available; it
 * always has a portable fallback (a plain spin/yield loop) for when that
 * fast path fails. Returning -1/ENOSYS unconditionally forces every
 * caller onto that fallback, which is exactly the busy-wait behavior
 * lib/pthread.c's own condition variables already use throughout this
 * tree. */
long syscall(long number, ...) {
    (void) number;
    g_errno = ENOSYS;
    return -1;
}
