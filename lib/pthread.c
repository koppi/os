#include <lib/pthread.h>
#include <lib/system_calls.h>
#include <lib/mutex.h>

/* -------------------------------------------------------------------- */
/* Thread create/join/identity                                          */
/* -------------------------------------------------------------------- */

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    (void) attr;
    int tid = thread_create((void *) start_routine, arg);
    if (tid < 0) {
        return 1; /* POSIX wants a positive error number, not -1; we do not
                    * have distinct errno values to report, so any nonzero
                    * value signals failure. */
    }
    thread->tid = tid;
    return 0;
}

int pthread_join(pthread_t thread, void **retval) {
    thread_join(thread.tid);
    /* thread_create()/thread_join() have no return-value channel (see
     * lib/system_calls.c), so a joined thread's result is never actually
     * recovered here -- always NULL, not whatever pthread_exit() was given. */
    if (retval) {
        *retval = 0;
    }
    return 0;
}

pthread_t pthread_self(void) {
    pthread_t self;
    self.tid = thread_self();
    return self;
}

int pthread_equal(pthread_t a, pthread_t b) {
    return a.tid == b.tid;
}

void pthread_exit(void *retval) {
    (void) retval;
    syscall3(4, 0, 0, 0); /* exit / stop_thread(): does not return */
    for (;;) {
    }
}

/* -------------------------------------------------------------------- */
/* Attributes -- accepted, not backed by anything real (see pthread.h)  */
/* -------------------------------------------------------------------- */

int pthread_attr_init(pthread_attr_t *attr) {
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}
int pthread_attr_destroy(pthread_attr_t *attr) {
    (void) attr;
    return 0;
}
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    attr->detachstate = detachstate;
    return 0;
}
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate) {
    *detachstate = attr->detachstate;
    return 0;
}
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    (void) attr;
    (void) stacksize;
    return 0;
}
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit) {
    (void) attr;
    (void) inherit;
    return 0;
}
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy) {
    (void) attr;
    (void) policy;
    return 0;
}
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy) {
    (void) attr;
    *policy = SCHED_OTHER;
    return 0;
}
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param) {
    (void) attr;
    (void) param;
    return 0;
}
int pthread_attr_setthreadname(pthread_attr_t *attr, const char *name) {
    (void) attr;
    (void) name;
    return 0;
}

/* -------------------------------------------------------------------- */
/* Mutex -- same test-and-set + pause + yield-after-spins as mutex.c    */
/* -------------------------------------------------------------------- */

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void) attr;
    mutex->locked = 0;
    return 0;
}
int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    (void) mutex;
    return 0;
}
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    int spins = 0;
    while (__sync_lock_test_and_set(&mutex->locked, 1) != 0) {
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
    return __sync_lock_test_and_set(&mutex->locked, 1) == 0 ? 0 : 1;
}
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    __sync_lock_release(&mutex->locked);
    return 0;
}

/* -------------------------------------------------------------------- */
/* Condition variable                                                   */
/*                                                                      */
/* A sequence counter: wait captures the current value while still      */
/* holding the caller's mutex (so a signal cannot land in the gap       */
/* between checking the wait predicate and starting to wait -- whoever  */
/* signals must also hold that same mutex, the standard POSIX usage     */
/* pattern, which is exactly what keeps this race-free), then polls it  */
/* after releasing the mutex. signal and broadcast are the same         */
/* operation here (every waiter, not just one, wakes on any increment)  */
/* -- POSIX explicitly allows spurious wakeups, so a correct caller      */
/* re-checks its own predicate after waking regardless, and this is a   */
/* documented simplification, not a silent behavior gap.                */
/* -------------------------------------------------------------------- */

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void) attr;
    cond->seq = 0;
    return 0;
}
int pthread_cond_destroy(pthread_cond_t *cond) {
    (void) cond;
    return 0;
}
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    unsigned int start = cond->seq;
    pthread_mutex_unlock(mutex);
    while (cond->seq == start) {
        thread_yield();
    }
    pthread_mutex_lock(mutex);
    return 0;
}
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    unsigned int start = cond->seq;
    pthread_mutex_unlock(mutex);
    int timed_out = 0;
    while (cond->seq == start) {
        /* time() (syscall 14) is whole Unix seconds -- the finest clock
         * this kernel exposes to userspace, so sub-second deadlines round
         * up to the next second rather than being honored exactly. */
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
int pthread_cond_signal(pthread_cond_t *cond) {
    __sync_fetch_and_add(&cond->seq, 1);
    return 0;
}
int pthread_cond_broadcast(pthread_cond_t *cond) {
    __sync_fetch_and_add(&cond->seq, 1);
    return 0;
}

int pthread_condattr_init(pthread_condattr_t *attr) {
    (void) attr;
    return 0;
}
int pthread_condattr_destroy(pthread_condattr_t *attr) {
    (void) attr;
    return 0;
}
int pthread_condattr_setclock(pthread_condattr_t *attr, int clock_id) {
    (void) attr;
    (void) clock_id;
    return 0;
}

/* -------------------------------------------------------------------- */
/* Thread-local storage: a small fixed (tid, key) -> value table, not    */
/* compiler-assisted TLS. Linear search, not a hashed/modulo table,      */
/* deliberately -- thread ids are handed out from one global counter     */
/* shared by every process and never reused, so a modulo-indexed table   */
/* could alias two live, unrelated threads onto the same slot.           */
/* Destructors (2nd arg to pthread_key_create) are accepted but never    */
/* invoked: there is no thread-exit hook to call them from.              */
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
static int g_next_key = 1;
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
/* Scheduling / naming -- accepted, not backed by anything real          */
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

/* -------------------------------------------------------------------- */
/* Cancellation: cooperative/deferred only. A target thread's own next   */
/* pthread_testcancel() (or nothing, if it never calls one) is what      */
/* actually ends it -- there is no asynchronous/signal-based cancellation */
/* here. Reuses the TLS table with a reserved key no real                */
/* pthread_key_create() call can hand out (those start at 1).            */
/* -------------------------------------------------------------------- */

#define PT_KEY_CANCEL_REQUESTED (-1)
#define PT_KEY_CANCEL_STATE (-2)

int pthread_cancel(pthread_t thread) {
    tls_set(thread.tid, PT_KEY_CANCEL_REQUESTED, (const void *) 1);
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
/* Cleanup handlers: a real stack, per thread (POSIX cleanup handlers    */
/* are per-thread -- a single shared stack would let two threads         */
/* pushing/popping concurrently corrupt each other's handlers). Bounded, */
/* like the TLS table above, and for the same reason: linear search      */
/* keyed by tid rather than a modulo-indexed table, since tids are never */
/* reused. NOT unwound automatically by pthread_exit()/cancellation --   */
/* only an explicit pthread_cleanup_pop(1) runs a handler. That is a     */
/* real, documented gap from POSIX (which unwinds pending handlers on    */
/* cancellation too), traded for not having to reason about running      */
/* arbitrary caller code from inside pthread_exit()'s own control flow.  */
/* -------------------------------------------------------------------- */

#define PT_MAX_CLEANUP_THREADS 32
#define PT_MAX_CLEANUP_DEPTH 8

typedef struct {
    void (*routine)(void *);
    void *arg;
} cleanup_entry_t;

typedef struct {
    int tid; /* 0 == unused slot */
    int depth;
    cleanup_entry_t stack[PT_MAX_CLEANUP_DEPTH];
} cleanup_thread_t;

static cleanup_thread_t g_cleanup[PT_MAX_CLEANUP_THREADS];
static mutex_t g_cleanup_lock;
static int g_cleanup_lock_init = 0;

static cleanup_thread_t *cleanup_for(int tid, int create) {
    int free_slot = -1;
    for (int i = 0; i < PT_MAX_CLEANUP_THREADS; i++) {
        if (g_cleanup[i].tid == tid) {
            return &g_cleanup[i];
        }
        if (create && g_cleanup[i].tid == 0 && free_slot < 0) {
            free_slot = i;
        }
    }
    if (create && free_slot >= 0) {
        g_cleanup[free_slot].tid = tid;
        g_cleanup[free_slot].depth = 0;
        return &g_cleanup[free_slot];
    }
    return 0;
}

void pthread_cleanup_push(void (*routine)(void *), void *arg) {
    if (!g_cleanup_lock_init) {
        mutex_init(&g_cleanup_lock);
        g_cleanup_lock_init = 1;
    }
    mutex_lock(&g_cleanup_lock);
    cleanup_thread_t *ct = cleanup_for(thread_self(), 1);
    if (ct && ct->depth < PT_MAX_CLEANUP_DEPTH) {
        ct->stack[ct->depth].routine = routine;
        ct->stack[ct->depth].arg = arg;
        ct->depth++;
    }
    mutex_unlock(&g_cleanup_lock);
}

void pthread_cleanup_pop(int execute) {
    if (!g_cleanup_lock_init) {
        return;
    }
    mutex_lock(&g_cleanup_lock);
    cleanup_entry_t entry = {0, 0};
    int have = 0;
    cleanup_thread_t *ct = cleanup_for(thread_self(), 0);
    if (ct && ct->depth > 0) {
        ct->depth--;
        entry = ct->stack[ct->depth];
        have = 1;
        if (ct->depth == 0) {
            ct->tid = 0; /* free the slot rather than leaking it across
                          * many short-lived threads */
        }
    }
    mutex_unlock(&g_cleanup_lock);
    if (have && execute && entry.routine) {
        entry.routine(entry.arg);
    }
}
