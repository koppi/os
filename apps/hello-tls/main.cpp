/**
 * @file apps/hello-tls/main.cpp
 * @brief Self-checking proof of real ELF thread-local storage: `thread_local` variables (from
 *        .tdata and .tbss), a thread_local object with a constructor and a destructor, one
 *        private copy per thread that survives context switches, and thread slots that can be
 *        created and reclaimed over and over.
 *
 * Built like Qt's threaded code is: against the real system <pthread.h> (lib/pthread_glibc.c),
 * with the TLS runtime from lib/tls.c and a linker script that defines the TLS segment symbols.
 * Prints one "tls: <what>: ok|FAIL" line per check and "tls: PASS" / "tls: FAIL" at the end;
 * the exit status says the same.
 */
#include <pthread.h>

extern "C" {
int printf(const char *fmt, ...);
void thread_yield(void);
}

static int g_failures = 0;

static void check(bool cond, const char *what)
{
    printf("tls: %s: %s\n", what, cond ? "ok" : "FAIL");
    if (!cond)
        g_failures++;
}

/* .tdata: a non-zero initializer every new thread must start from. */
static thread_local int tl_init = 7;
/* .tbss: must read zero in every new thread. */
static thread_local int tl_zero;
static thread_local char tl_pattern[64];

static int g_ctor, g_dtor;

/* Dynamic initialization + destructor: the compiler registers the destructor through
 * __cxa_thread_atexit on first use in each thread. */
struct Obj {
    int v;
    Obj() : v(42) { __atomic_fetch_add(&g_ctor, 1, __ATOMIC_SEQ_CST); }
    ~Obj() { __atomic_fetch_add(&g_dtor, 1, __ATOMIC_SEQ_CST); }
};
static thread_local Obj tl_obj;

/* A pthread key with a destructor: POSIX runs it, with the thread's value, when the thread ends
 * (Qt's QThread cleanup -- the thing QThread::wait() waits for -- hangs off exactly this). */
static pthread_key_t g_key;
static int g_key_dtor_calls, g_key_dtor_sum, g_key_dtor_tls_ok;

static void key_dtor(void *v)
{
    __atomic_fetch_add(&g_key_dtor_calls, 1, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&g_key_dtor_sum, (int) (long) v, __ATOMIC_SEQ_CST);
    if (tl_init == 7 + 2000)                 /* this thread's thread_local block is still alive */
        __atomic_fetch_add(&g_key_dtor_tls_ok, 1, __ATOMIC_SEQ_CST);
}

#define NTHREADS 4
static int *g_addr[NTHREADS];
static int g_bad[NTHREADS];

static void *worker(void *arg)
{
    const int id = (int) (long) arg;
    bool ok = tl_init == 7 && tl_zero == 0;
    for (int i = 0; i < 64; i++)
        ok = ok && tl_pattern[i] == 0;
    ok = ok && tl_obj.v == 42;

    g_addr[id] = &tl_init;
    pthread_setspecific(g_key, (void *) (long) (id + 1));

    /* Interleave with every other thread (yields, plus whatever the timer preempts): each
     * thread's private counter and byte pattern must come back exactly as it left them. */
    for (int i = 0; i < 2000; i++) {
        tl_init++;
        tl_zero += 2;
        tl_pattern[i % 64] = (char) (id * 16 + 1 + (i & 7));
        if ((i & 63) == 0)
            thread_yield();
    }
    ok = ok && tl_init == 7 + 2000 && tl_zero == 4000;
    for (int i = 1936; i < 2000; i++)
        ok = ok && tl_pattern[i % 64] == (char) (id * 16 + 1 + (i & 7));

    void *tp;
    asm volatile("movl %%gs:0, %0" : "=r"(tp));
    ok = ok && tp == __builtin_thread_pointer();

    g_bad[id] = !ok;
    return 0;
}

static void *fresh(void *arg)
{
    int *r = (int *) arg;
    *r = (tl_init == 7 && tl_zero == 0 && tl_obj.v == 42) ? 1 : 0;
    return 0;
}

int main()
{
    /* The main thread has its own segment, initialised from the same image. */
    check(tl_init == 7 && tl_zero == 0, "main thread starts from .tdata/.tbss");
    check(g_ctor == 0, "no thread_local object constructed before first use");
    check(tl_obj.v == 42 && g_ctor == 1, "dynamic initializer ran once in the main thread");
    tl_init = 1000;
    tl_zero = -5;

    check(pthread_key_create(&g_key, key_dtor) == 0, "pthread_key_create with a destructor");
    pthread_setspecific(g_key, (void *) 99);     /* the main thread's value: never destroyed at exit() */

    pthread_t t[NTHREADS];
    for (int i = 0; i < NTHREADS; i++)
        check(pthread_create(&t[i], 0, worker, (void *) (long) i) == 0, "pthread_create");
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(t[i], 0);

    bool allgood = true;
    for (int i = 0; i < NTHREADS; i++)
        allgood = allgood && !g_bad[i];
    check(allgood, "each thread saw fresh values and kept its own across yields");

    bool distinct = true;
    for (int i = 0; i < NTHREADS; i++) {
        distinct = distinct && g_addr[i] != &tl_init;
        for (int j = i + 1; j < NTHREADS; j++)
            distinct = distinct && g_addr[i] != g_addr[j];
    }
    check(distinct, "every thread's copy lives at its own address");
    check(tl_init == 1000 && tl_zero == -5, "the main thread's copy is untouched by the others");
    printf("tls: constructors run: %d, destructors run: %d\n", g_ctor, g_dtor);
    check(g_ctor == 1 + NTHREADS, "one constructor per thread");
    check(g_dtor == NTHREADS, "destructors ran at each thread's exit (main's is still pending)");

    printf("tls: key destructor calls: %d, value sum: %d\n", g_key_dtor_calls, g_key_dtor_sum);
    check(g_key_dtor_calls == NTHREADS && g_key_dtor_sum == 1 + 2 + 3 + 4,
          "each thread's pthread key destructor ran once with that thread's value");
    check(g_key_dtor_tls_ok == NTHREADS, "...while the thread's thread_local block was still alive");

    /* 24 threads one after another: GDT slots, TLS blocks and thread slots all get recycled. */
    int fresh_ok = 0, runs = 0;
    for (int i = 0; i < 24; i++) {
        int r = 0;
        pthread_t th;
        if (pthread_create(&th, 0, fresh, &r) != 0)
            break;
        pthread_join(th, 0);
        runs++;
        fresh_ok += r;
    }
    check(runs == 24 && fresh_ok == 24, "24 sequential threads, each starting from a clean image");

    printf("tls: %s\n", g_failures ? "FAIL" : "PASS");
    return g_failures ? 1 : 0;
}
