/**
 * @file include/lib/pthread.h
 * @brief A pthread-shaped API surface over thread_create()/mutex_t
 *        (system_calls.h / mutex.h), scoped to what real Qt6 source
 *        (qthread_unix.cpp / qwaitcondition_unix.cpp) actually calls.
 *
 * This is not a glibc-ABI-compatible pthread: nothing here needs to match
 * real libpthread's struct layout byte-for-byte (Qt's source uses these
 * types entirely through the public API, never by reinterpreting their
 * internals), it just needs the same type names, function signatures, and
 * observable behavior. Backed by this kernel's own thread_create/
 * thread_join/thread_yield/thread_self syscalls and __sync_* atomics, not a
 * real libpthread.
 *
 * Faithfully implemented: thread create/join/self/equal, mutexes,
 * condition variables (including timedwait, at whole-second resolution --
 * this kernel's `time()` syscall has no finer clock), and
 * pthread_key_t-based thread-local storage (a small fixed table, not
 * compiler-assisted TLS).
 *
 * Deliberately simplified, documented rather than hidden: every
 * pthread_attr_t/pthread_condattr_t setter is a no-op returning success
 * (this kernel has its own scheduling knobs -- sched_set_priority/policy/
 * weight -- not currently syscall-exposed, so there is nothing real to
 * plumb these into yet). pthread_cancel is cooperative/deferred only (a
 * flag a thread must itself observe via pthread_testcancel or a
 * cancellation point), not POSIX's async cancellation -- matches how
 * QThread::terminate() is documented as unsafe/best-effort in real Qt too.
 */
#ifndef LIB_PTHREAD_H
#define LIB_PTHREAD_H

#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int tid;
} pthread_t;

typedef struct {
    int detachstate;
} pthread_attr_t;

typedef struct {
    int _unused;
} pthread_mutexattr_t;

typedef struct {
    int _unused;
} pthread_condattr_t;

typedef struct {
    volatile int locked;
} pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER \
    { 0 }

typedef struct {
    volatile unsigned int seq;
} pthread_cond_t;
#define PTHREAD_COND_INITIALIZER \
    { 0 }

typedef int pthread_key_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2

#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1

#define ETIMEDOUT 110

struct sched_param {
    int sched_priority;
};

struct timespec {
    long tv_sec;
    long tv_nsec;
};

/* Thread create/join/identity */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
pthread_t pthread_self(void);
int pthread_equal(pthread_t a, pthread_t b);
void pthread_exit(void *retval);

/* Attributes -- accepted, not backed by anything (see file comment) */
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit);
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy);
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);
int pthread_attr_setthreadname(pthread_attr_t *attr, const char *name);

/* Mutex */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* Condition variable */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

int pthread_condattr_init(pthread_condattr_t *attr);
int pthread_condattr_destroy(pthread_condattr_t *attr);
int pthread_condattr_setclock(pthread_condattr_t *attr, int clock_id);

/* Thread-local storage */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);
void *pthread_getspecific(pthread_key_t key);

/* Scheduling / naming -- accepted, not backed (see file comment) */
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param);
int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param);
int pthread_setname_np(pthread_t thread, const char *name);

/* Cooperative/deferred cancellation only -- see file comment */
int pthread_cancel(pthread_t thread);
int pthread_setcancelstate(int state, int *oldstate);
void pthread_testcancel(void);

void pthread_cleanup_push(void (*routine)(void *), void *arg);
void pthread_cleanup_pop(int execute);

#ifdef __cplusplus
}
#endif

#endif
