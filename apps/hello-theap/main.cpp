/**
 * @file apps/hello-theap/main.cpp
 * @brief Regression test for two thread/heap bugs: thread stacks overlapping
 *        the main heap, and malloc'd memory dying with the thread that
 *        allocated it.
 *
 * Bug 1 -- placement. The main heap grows upward from just above the image,
 * in place, and thread slots used to be laid out at fixed offsets from the
 * image end, ignoring how far that heap had grown. Once the heap passed
 * ~300 KB the first secondary thread's 64-page user stack sat INSIDE it.
 * thread_create then re-mapped (zeroed) live heap pages, and the thread's exit
 * unmapped 256 KiB of the main heap -- the next read of a big buffer faulted.
 * Real Qt6 hit it: a 518 KB QImage followed by a QThreadPool worker.
 *
 * Bug 2 -- arena lifetime. malloc/free used the arena of whichever thread was
 * running, and a thread's exit unmapped its arena. So a block a worker
 * allocated and handed to another thread (a QImage built by a pool worker and
 * shown by the GUI thread) was unmapped when the worker exited. Threads share
 * one address space, so they now share one malloc arena -- the process's.
 *
 * Every phase fills memory with a position-dependent pattern and re-checks
 * *all* of it afterwards, so a clobbered or vanished page shows up as a
 * mismatch or a page fault rather than as silence:
 *
 *   1. grow the heap by ~1.5 MiB, create + join one thread, re-read it all
 *   2. several concurrent threads, each with its own stack and heap use
 *   3. grow the main heap *while* a thread is alive: the heap must not run
 *      into the live thread's stack
 *   4. blocks a worker allocates must still be there after the worker exits
 *   5. blocks main allocates can be freed by a worker, and reused
 *   6. several threads malloc/free concurrently out of the one arena
 *   7. a file handle (malloc'd by fopen) opened in a worker is still good after
 *      the worker exits, and another thread can read and close it
 *   8. keep creating threads until thread_create refuses: it must fail with -1
 *      and leave everything intact (the per-process slot region is finite)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system_calls.h>

typedef unsigned char u8;
typedef unsigned int u32;

static bool g_ok = true;

#define CHECK(cond, ...) \
    do { if (!(cond)) { printf((char *) "FAIL " __VA_ARGS__); g_ok = false; } } while (0)

/* ---- the main thread's heap under test -------------------------------- */

#define BIG_BYTES   (640u * 1024u)    /* a QImage-sized block, > 512 KiB */
#define SMALL_BYTES (64u * 1024u)
#define NSMALL      14                /* BIG + NSMALL*SMALL = 1.5 MiB      */
#define NSMALL2     16                /* grown while a thread is alive     */

static u8 *g_big;
static u8 *g_small[NSMALL + NSMALL2];

static inline u8 pat(u32 tag, u32 i) {
    return (u8) (((i * 2654435761u) >> 24) ^ (tag * 40503u));
}

static void fill(u8 *p, u32 n, u32 tag) {
    for (u32 i = 0; i < n; i++)
        p[i] = pat(tag, i);
}

/** @return the offset of the first mismatch, or -1 if all @p n bytes match. */
static int verify(const u8 *p, u32 n, u32 tag) {
    for (u32 i = 0; i < n; i++)
        if (p[i] != pat(tag, i))
            return (int) i;
    return -1;
}

static void check_main_heap(const char *when, int nsmall) {
    int bad = verify(g_big, BIG_BYTES, 1000);
    CHECK(bad < 0, "%s: big buffer corrupt at +%d\n", (char *) when, bad);
    for (int i = 0; i < nsmall; i++) {
        bad = verify(g_small[i], SMALL_BYTES, (u32) i);
        CHECK(bad < 0, "%s: small[%d] corrupt at +%d\n", (char *) when, i, bad);
    }
}

static bool grow(int from, int to) {
    for (int i = from; i < to; i++) {
        g_small[i] = (u8 *) malloc(SMALL_BYTES);
        if (!g_small[i]) {
            printf((char *) "FAIL malloc small[%d]\n", i);
            return false;
        }
        fill(g_small[i], SMALL_BYTES, (u32) i);
    }
    return true;
}

/* ---- worker threads ---------------------------------------------------- */

struct job {
    int id;
    volatile int go;      /* main raises this to release a waiting worker  */
    volatile int done;
    volatile int ok;
    int wait_for_go;
};

/** Burn some stack with a recognisable pattern; verified again on the way
 *  back up, so a neighbour overwriting it is caught. */
static int deep(int depth, u32 tag) {
    volatile u8 frame[2048];
    for (int i = 0; i < 2048; i++)
        frame[i] = pat(tag + (u32) depth, (u32) i);
    int bad = 0;
    if (depth > 0)
        bad = deep(depth - 1, tag);
    for (int i = 0; i < 2048; i++)
        if (frame[i] != pat(tag + (u32) depth, (u32) i))
            bad++;
    return bad;
}

#define WORKER_HEAP (200u * 1024u)    /* a sizeable block, malloc'd from a worker */

extern "C" void *worker(void *arg) {
    job *j = (job *) arg;
    int ok = 1;
    u32 tag = 5000u + (u32) j->id * 17u;

    /* A worker can allocate a block bigger than any per-thread arena would
     * have started with, and freeing it from the same thread is the easy case. */
    u8 *mine = (u8 *) malloc(WORKER_HEAP);
    if (!mine) {
        ok = 0;
    } else {
        fill(mine, WORKER_HEAP, tag);
        if (deep(30, tag) != 0)
            ok = 0;
        if (j->wait_for_go)
            while (!j->go)
                thread_yield();
        for (int k = 0; k < 4; k++)
            thread_yield();
        if (verify(mine, WORKER_HEAP, tag) >= 0)
            ok = 0;
        if (deep(30, tag + 1) != 0)
            ok = 0;
        free(mine);
    }

    j->ok = ok;
    j->done = 1;
    return 0;
}

/* Phase 4: a worker allocates, exits without freeing, and main uses the memory
 * afterwards. Sizes straddle what a per-thread arena would start with (16 KiB). */
#define NHAND 3
struct handoff {
    u8 *blk;
    u32 n;
    u32 tag;
};
static handoff g_hand[NHAND];
static const u32 g_hand_size[NHAND] = { 4096u, 40u * 1024u, 300u * 1024u };

extern "C" void *producer(void *arg) {
    handoff *h = (handoff *) arg;
    h->blk = (u8 *) malloc(h->n);
    if (h->blk)
        fill(h->blk, h->n, h->tag);
    return 0;                      /* deliberately not freed */
}

/* Phase 5: main hands blocks to a worker, which verifies and frees them. */
#define NFREE 6
static u8 *g_tofree[NFREE];
static const u32 g_free_size = 24u * 1024u;
static volatile int g_free_ok;

extern "C" void *freer(void *arg) {
    (void) arg;
    int ok = 1;
    for (int i = 0; i < NFREE; i++) {
        if (verify(g_tofree[i], g_free_size, 600u + (u32) i) >= 0)
            ok = 0;
        free(g_tofree[i]);
    }
    g_free_ok = ok;
    return 0;
}

/* Phase 7: fopen() mallocs the FILE handle in the kernel (vfs.c) on the calling
 * process's behalf; open it in a worker, use and close it from main. */
static FILE *g_fp;
static char *g_fp_name;

extern "C" void *opener(void *arg) {
    (void) arg;
    g_fp = fopen(g_fp_name, (char *) "r");
    return 0;
}

/* Phase 6: concurrent malloc/free of mixed sizes out of the shared arena. */
#define NSTRESS 4
#define STRESS_ITERS 1500
#define STRESS_LIVE 12
static volatile int g_stress_ok[NSTRESS];
static volatile int g_stress_done[NSTRESS];

static inline u32 xorshift(u32 *s) {
    u32 x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

extern "C" void *stress(void *arg) {
    int id = (int) (long) arg;
    u32 rng = 0x9E3779B9u * (u32) (id + 1);
    u8 *blk[STRESS_LIVE];
    u32 len[STRESS_LIVE];
    u32 tag[STRESS_LIVE];
    for (int i = 0; i < STRESS_LIVE; i++)
        blk[i] = 0;
    int ok = 1;
    for (int it = 0; it < STRESS_ITERS; it++) {
        int slot = (int) (xorshift(&rng) % STRESS_LIVE);
        if (blk[slot]) {
            if (verify(blk[slot], len[slot], tag[slot]) >= 0)
                ok = 0;
            free(blk[slot]);
            blk[slot] = 0;
        } else {
            len[slot] = 16u + xorshift(&rng) % 6000u;
            tag[slot] = xorshift(&rng) & 0xFFFFu;
            blk[slot] = (u8 *) malloc(len[slot]);
            if (!blk[slot]) {
                ok = 0;
                break;
            }
            fill(blk[slot], len[slot], tag[slot]);
        }
        if ((it & 63) == 0)
            thread_yield();
    }
    for (int i = 0; i < STRESS_LIVE; i++) {
        if (blk[i]) {
            if (verify(blk[i], len[i], tag[i]) >= 0)
                ok = 0;
            free(blk[i]);
        }
    }
    g_stress_ok[id] = ok;
    g_stress_done[id] = 1;
    return 0;
}

extern "C" void *noop(void *arg) {
    (void) arg;
    return 0;
}

#define MAXJOBS 4
static job g_jobs[MAXJOBS];

int main() {
    /* ---- phase 1: grow past the first slot's old position, one thread ---- */
    g_big = (u8 *) malloc(BIG_BYTES);
    if (!g_big) {
        printf((char *) "FAIL malloc big\n");
        return 1;
    }
    fill(g_big, BIG_BYTES, 1000);
    if (!grow(0, NSMALL))
        return 1;
    check_main_heap("before thread", NSMALL);

    g_jobs[0].id = 0;
    int tid = thread_create((void *) worker, (void *) &g_jobs[0]);
    CHECK(tid >= 0, "phase1 thread_create=%d\n", tid);
    if (tid >= 0)
        thread_join(tid);
    CHECK(g_jobs[0].done && g_jobs[0].ok, "phase1 worker ok=%d done=%d\n",
          g_jobs[0].ok, g_jobs[0].done);
    check_main_heap("after 1 thread", NSMALL);
    printf((char *) "phase 1 (grow, 1 thread, re-read): %s\n", g_ok ? (char *) "ok" : (char *) "BAD");

    /* ---- phase 2: three threads alive at once ---------------------------- */
    int tids[MAXJOBS];
    for (int i = 1; i < MAXJOBS; i++) {
        g_jobs[i].id = i;
        tids[i] = thread_create((void *) worker, (void *) &g_jobs[i]);
        CHECK(tids[i] >= 0, "phase2 thread_create(%d)=%d\n", i, tids[i]);
    }
    for (int i = 1; i < MAXJOBS; i++)
        if (tids[i] >= 0)
            thread_join(tids[i]);
    for (int i = 1; i < MAXJOBS; i++)
        CHECK(g_jobs[i].done && g_jobs[i].ok, "phase2 worker %d ok=%d done=%d\n",
              i, g_jobs[i].ok, g_jobs[i].done);
    check_main_heap("after 3 threads", NSMALL);
    printf((char *) "phase 2 (3 concurrent threads): %s\n", g_ok ? (char *) "ok" : (char *) "BAD");

    /* ---- phase 3: grow the main heap while a thread is alive ------------- */
    g_jobs[0].id = 9;
    g_jobs[0].go = 0;
    g_jobs[0].done = 0;
    g_jobs[0].ok = 0;
    g_jobs[0].wait_for_go = 1;
    tid = thread_create((void *) worker, (void *) &g_jobs[0]);
    CHECK(tid >= 0, "phase3 thread_create=%d\n", tid);
    if (grow(NSMALL, NSMALL + NSMALL2)) {
        check_main_heap("grown beside a live thread", NSMALL + NSMALL2);
    }
    g_jobs[0].go = 1;
    if (tid >= 0)
        thread_join(tid);
    CHECK(g_jobs[0].done && g_jobs[0].ok, "phase3 worker ok=%d done=%d\n",
          g_jobs[0].ok, g_jobs[0].done);
    check_main_heap("after the live thread exited", NSMALL + NSMALL2);
    printf((char *) "phase 3 (heap grows beside a live thread): %s\n", g_ok ? (char *) "ok" : (char *) "BAD");

    /* ---- phase 4: a worker's blocks outlive the worker ------------------- */
    for (int i = 0; i < NHAND; i++) {
        g_hand[i].n = g_hand_size[i];
        g_hand[i].tag = 300u + (u32) i;
        int t = thread_create((void *) producer, (void *) &g_hand[i]);
        CHECK(t >= 0, "phase4 thread_create(%d)=%d\n", i, t);
        if (t >= 0)
            thread_join(t);       /* the producer has exited by the time this returns */
    }
    for (int i = 0; i < NHAND; i++) {
        CHECK(g_hand[i].blk != 0, "phase4 producer %d got no memory\n", i);
        if (g_hand[i].blk) {
            int bad = verify(g_hand[i].blk, g_hand[i].n, g_hand[i].tag);
            CHECK(bad < 0, "phase4 block from exited thread %d corrupt at +%d\n", i, bad);
        }
    }
    for (int i = 0; i < NHAND; i++)
        free(g_hand[i].blk);
    check_main_heap("after producers", NSMALL + NSMALL2);
    printf((char *) "phase 4 (worker blocks outlive the worker): %s\n", g_ok ? (char *) "ok" : (char *) "BAD");

    /* ---- phase 5: a worker frees main's blocks ----------------------------- */
    for (int i = 0; i < NFREE; i++) {
        g_tofree[i] = (u8 *) malloc(g_free_size);
        if (!g_tofree[i]) {
            printf((char *) "FAIL phase5 malloc %d\n", i);
            return 1;
        }
        fill(g_tofree[i], g_free_size, 600u + (u32) i);
    }
    g_free_ok = 0;
    tid = thread_create((void *) freer, 0);
    CHECK(tid >= 0, "phase5 thread_create=%d\n", tid);
    if (tid >= 0)
        thread_join(tid);
    CHECK(g_free_ok, "phase5 worker did not see main's blocks intact\n");
    /* The space the worker released must be reusable without trouble. */
    u8 *again[NFREE];
    for (int i = 0; i < NFREE; i++) {
        again[i] = (u8 *) malloc(g_free_size);
        CHECK(again[i] != 0, "phase5 re-malloc %d\n", i);
        if (again[i])
            fill(again[i], g_free_size, 700u + (u32) i);
    }
    for (int i = 0; i < NFREE; i++) {
        if (again[i]) {
            CHECK(verify(again[i], g_free_size, 700u + (u32) i) < 0, "phase5 reused block %d\n", i);
            free(again[i]);
        }
    }
    check_main_heap("after the worker freed", NSMALL + NSMALL2);
    printf((char *) "phase 5 (worker frees main's blocks): %s\n", g_ok ? (char *) "ok" : (char *) "BAD");

    /* ---- phase 6: concurrent malloc/free ----------------------------------- */
    int stids[NSTRESS];
    for (int i = 0; i < NSTRESS; i++) {
        stids[i] = thread_create((void *) stress, (void *) (long) i);
        CHECK(stids[i] >= 0, "phase6 thread_create(%d)=%d\n", i, stids[i]);
    }
    for (int i = 0; i < NSTRESS; i++)
        if (stids[i] >= 0)
            thread_join(stids[i]);
    for (int i = 0; i < NSTRESS; i++)
        CHECK(g_stress_done[i] && g_stress_ok[i], "phase6 stress %d ok=%d done=%d\n",
              i, g_stress_ok[i], g_stress_done[i]);
    check_main_heap("after the stress run", NSMALL + NSMALL2);
    printf((char *) "phase 6 (%d threads x %d malloc/free): %s\n", NSTRESS, STRESS_ITERS,
           g_ok ? (char *) "ok" : (char *) "BAD");

    /* ---- phase 7: a file handle opened by a worker ------------------------ */
    {
        /* Control: the same open from main. If the RAM disk has no such file
         * the phase is meaningless, so say so rather than fail. */
        char buf[512];
        /* fopen() resolves a relative name against the shell's working
         * directory, so try the spellings that reach /rd/zshrc from there. */
        static const char *const names[] = { "zshrc", "rd/zshrc", "/rd/zshrc" };
        FILE *ctl = 0;
        for (unsigned i = 0; i < sizeof names / sizeof names[0] && !ctl; i++) {
            g_fp_name = (char *) names[i];
            ctl = fopen(g_fp_name, (char *) "r");
        }
        if (!ctl) {
            printf((char *) "phase 7 (file handle across threads): skipped, could not open /rd/zshrc from here\n");
        } else {
            int want = fread(buf, 1, ctl);
            fclose(ctl);
            g_fp = 0;
            tid = thread_create((void *) opener, 0);
            CHECK(tid >= 0, "phase7 thread_create=%d\n", tid);
            if (tid >= 0)
                thread_join(tid);
            CHECK(g_fp != 0, "phase7 worker's fopen failed\n");
            if (g_fp) {
                int got = fread(buf, 1, g_fp);
                CHECK(got == want && got > 0, "phase7 read %d from the worker's handle, main's own read gave %d\n",
                      got, want);
                fclose(g_fp);
            }
            check_main_heap("after the file handle", NSMALL + NSMALL2);
            printf((char *) "phase 7 (file handle across threads): %s\n", g_ok ? (char *) "ok" : (char *) "BAD");
        }
    }

    /* ---- phase 8: run the slot region dry -------------------------------- */
    int made = 0, refused = 0;
    for (int i = 0; i < 400 && !refused; i++) {
        int t = thread_create((void *) noop, 0);
        if (t < 0) {
            refused = t;
            break;
        }
        thread_join(t);
        made++;
    }
    printf((char *) "phase 8: %d more threads created, then thread_create -> %d\n", made, refused);
    CHECK(made >= 8, "only %d threads could be created\n", made);
    CHECK(refused == -1 || refused == 0, "refusal was %d, expected -1\n", refused);
    if (refused) {
        /* A refused create must be repeatable and must not have hurt anything. */
        CHECK(thread_create((void *) noop, 0) == -1, "second refusal differs\n");
    }
    check_main_heap("after exhausting thread slots", NSMALL + NSMALL2);

    /* The main heap must still grow after all that. */
    u8 *more = (u8 *) malloc(256u * 1024u);
    CHECK(more != 0, "malloc after exhaustion\n");
    if (more) {
        fill(more, 256u * 1024u, 77);
        CHECK(verify(more, 256u * 1024u, 77) < 0, "post-exhaustion block\n");
        free(more);
    }
    check_main_heap("final", NSMALL + NSMALL2);

    printf(g_ok ? (char *) "PASS\n" : (char *) "OVERALL FAIL\n");
    return g_ok ? 0 : 1;
}
