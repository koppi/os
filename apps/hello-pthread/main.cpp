/**
 * @file apps/hello-pthread/main.cpp
 * @brief lib/pthread.c: self-checking coverage of the parts hello-thread
 *        did not exercise -- condition variables and thread-local storage
 *        are new code, not just a pthread-shaped wrapper over the already-
 *        verified thread_create/mutex primitives, so they need their own
 *        proof, same reasoning as every other self-checking demo this
 *        session. The mutex test re-runs hello-thread's proven scenario
 *        through pthread_mutex_t specifically, to confirm the wrapper
 *        itself introduced no bugs.
 *
 * No real STL headers here, so (unlike hello-str/hello-map/hello-set) this
 * can include our own <stdio.h> without conflict.
 */
#include <stdio.h>
#include <pthread.h>
#include <system_calls.h> /* thread_yield() -- pthread.h has no sched_yield() equivalent */

#define NTHREADS 4
#define ITERS 25000
#define PRODUCE_COUNT 5000

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_counter = 0;

extern "C" void *mutex_worker(void *arg) {
    (void) arg;
    for (int i = 0; i < ITERS; i++) {
        pthread_mutex_lock(&g_mutex);
        g_counter++;
        pthread_mutex_unlock(&g_mutex);
    }
    return 0;
}

static pthread_mutex_t g_pc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_pc_cond = PTHREAD_COND_INITIALIZER;
static int g_produced = 0;
static int g_consumed = 0;
static int g_producer_done = 0;

extern "C" void *producer(void *arg) {
    (void) arg;
    for (int i = 0; i < PRODUCE_COUNT; i++) {
        pthread_mutex_lock(&g_pc_mutex);
        g_produced++;
        pthread_cond_signal(&g_pc_cond);
        pthread_mutex_unlock(&g_pc_mutex);
    }
    pthread_mutex_lock(&g_pc_mutex);
    g_producer_done = 1;
    pthread_cond_broadcast(&g_pc_cond);
    pthread_mutex_unlock(&g_pc_mutex);
    return 0;
}

extern "C" void *consumer(void *arg) {
    (void) arg;
    for (;;) {
        pthread_mutex_lock(&g_pc_mutex);
        while (g_consumed >= g_produced && !g_producer_done) {
            pthread_cond_wait(&g_pc_cond, &g_pc_mutex);
        }
        if (g_consumed >= g_produced) {
            /* producer is done and there is nothing left to consume */
            pthread_mutex_unlock(&g_pc_mutex);
            break;
        }
        g_consumed++;
        pthread_mutex_unlock(&g_pc_mutex);
    }
    return 0;
}

static pthread_key_t g_key;
static int g_tls_seen[NTHREADS];

extern "C" void *tls_worker(void *arg) {
    int idx = (int) (long) arg;
    pthread_setspecific(g_key, (void *) (long) (idx * 1000 + 7));
    /* Yield a few times so, if TLS were accidentally shared across
     * threads instead of per-thread, a sibling's setspecific() call in
     * between would corrupt what we read back here. */
    for (int i = 0; i < 50; i++) {
        thread_yield();
    }
    void *v = pthread_getspecific(g_key);
    g_tls_seen[idx] = (int) (long) v;
    return 0;
}

int main() {
    bool ok = true;

    /* -- mutex, through the pthread_mutex_t wrapper specifically -- */
    pthread_t tids[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        if (pthread_create(&tids[i], 0, mutex_worker, 0) != 0) {
            printf((char *) "FAIL pthread_create (mutex) %d\n", i);
            ok = false;
        }
    }
    for (int i = 0; i < NTHREADS; i++) {
        pthread_join(tids[i], 0);
    }
    int expected = NTHREADS * ITERS;
    if (g_counter != expected) {
        printf((char *) "FAIL mutex counter=%d expected=%d\n", g_counter, expected);
        ok = false;
    }

    /* -- condition variable: single producer, single consumer -- */
    pthread_t prod, cons;
    pthread_create(&cons, 0, consumer, 0);
    pthread_create(&prod, 0, producer, 0);
    pthread_join(prod, 0);
    pthread_join(cons, 0);
    if (g_consumed != PRODUCE_COUNT || g_produced != PRODUCE_COUNT) {
        printf((char *) "FAIL condvar produced=%d consumed=%d expected=%d\n", g_produced, g_consumed, PRODUCE_COUNT);
        ok = false;
    }

    /* -- thread-local storage: each thread must see only its own value -- */
    pthread_key_create(&g_key, 0);
    pthread_t tls_tids[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        g_tls_seen[i] = -1;
        pthread_create(&tls_tids[i], 0, tls_worker, (void *) (long) i);
    }
    for (int i = 0; i < NTHREADS; i++) {
        pthread_join(tls_tids[i], 0);
    }
    for (int i = 0; i < NTHREADS; i++) {
        int want = i * 1000 + 7;
        if (g_tls_seen[i] != want) {
            printf((char *) "FAIL tls thread %d: got %d want %d\n", i, g_tls_seen[i], want);
            ok = false;
        }
    }

    /* -- identity -- */
    pthread_t self_a = pthread_self();
    pthread_t self_b = pthread_self();
    if (!pthread_equal(self_a, self_b)) {
        printf((char *) "FAIL pthread_equal(self, self)\n");
        ok = false;
    }
    if (pthread_equal(self_a, tls_tids[0])) {
        printf((char *) "FAIL pthread_equal(self, other) should differ\n");
        ok = false;
    }

    printf(ok ? (char *) "PASS\n" : (char *) "OVERALL FAIL\n");
    return ok ? 0 : 1;
}
