/**
 * @file apps/hello-thread/main.cpp
 * @brief First userspace-threading app: thread_create/thread_join (syscalls
 *        32/33) and the ring-3 mutex (lib/mutex.c) built to support them.
 *        Self-checking like the STL container demos, for the same reason --
 *        a broken mutex or a scheduler-ring bug in create_user_thread()
 *        would show up as an occasionally-wrong number, not a crash, so
 *        trust the check, not the eye.
 *
 * No real STL headers here, so unlike hello-str/hello-map/hello-set this can
 * include our own <stdio.h> without conflict.
 */
#include <stdio.h>
#include <system_calls.h>
#include <mutex.h>

#define NTHREADS 4
#define ITERS 25000

static mutex_t g_mutex;
static int g_counter = 0;
static int g_seen_arg[NTHREADS];

/* thread_create() enters this exactly like main() enters a process: iret
 * straight to eip with one cdecl argument already on the stack. extern "C"
 * only matters for the name in a debugger/symbol table here -- taking its
 * address as void* doesn't care about mangling -- but it also makes the
 * pthread-shaped void*(void*) signature explicit. */
extern "C" void *worker(void *arg) {
    int idx = (int) (long) arg;
    if (idx >= 0 && idx < NTHREADS) {
        g_seen_arg[idx] = idx;
    }

    for (int i = 0; i < ITERS; i++) {
        mutex_lock(&g_mutex);
        g_counter++;
        mutex_unlock(&g_mutex);
    }

    return 0;
}

int main() {
    mutex_init(&g_mutex);
    for (int i = 0; i < NTHREADS; i++) {
        g_seen_arg[i] = -1;
    }

    bool ok = true;
    int tids[NTHREADS];

    for (int i = 0; i < NTHREADS; i++) {
        tids[i] = thread_create((void *) worker, (void *) (long) i);
        if (tids[i] < 0) {
            printf((char *) "FAIL thread_create(%d)\n", i);
            ok = false;
        }
    }

    for (int i = 0; i < NTHREADS; i++) {
        if (tids[i] >= 0) {
            thread_join(tids[i]);
        }
    }

    for (int i = 0; i < NTHREADS; i++) {
        if (g_seen_arg[i] != i) {
            printf((char *) "FAIL arg for thread %d: got %d\n", i, g_seen_arg[i]);
            ok = false;
        }
    }

    int expected = NTHREADS * ITERS;
    if (g_counter != expected) {
        printf((char *) "FAIL counter=%d expected=%d\n", g_counter, expected);
        ok = false;
    }

    printf(ok ? (char *) "PASS\n" : (char *) "OVERALL FAIL\n");
    return ok ? 0 : 1;
}
