#include <lib/mutex.h>
#include <lib/system_calls.h>

void mutex_init(mutex_t *m) {
    m->locked = 0;
}

int mutex_trylock(mutex_t *m) {
    return __sync_lock_test_and_set(&m->locked, 1) == 0;
}

void mutex_lock(mutex_t *m) {
    int spins = 0;
    while (__sync_lock_test_and_set(&m->locked, 1) != 0) {
        if (spins++ > 1000) {
            thread_yield();
            spins = 0;
        } else {
            asm volatile("pause");
        }
    }
}

void mutex_unlock(mutex_t *m) {
    __sync_lock_release(&m->locked);
}
