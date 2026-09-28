/**
 * @file include/lib/mutex.h
 * @brief Userspace mutex for threads started with thread_create() (see
 *        system_calls.h). Same test-and-set primitive the kernel's own
 *        spinlock.c uses (GCC __sync builtins compile to a plain, unprivileged
 *        xchg -- no cli/sti, so it works unmodified at ring 3), with a spin
 *        count before falling back to thread_yield() so contention does not
 *        just burn the holder's whole quantum against itself.
 */
#ifndef MUTEX_H
#define MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile int locked;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
/** @return Non-zero if the lock was acquired, 0 if it was already held. */
int mutex_trylock(mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif
