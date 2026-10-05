/**
 * @file apps/hello-theap/main.cpp
 * @brief Regression test: a secondary thread's stack/heap must never overlap
 *        the main thread's (growable, in-place) malloc heap.
 *
 * The bug this pins down: the main heap grows upward from just above the
 * image, in place, and thread slots used to be laid out at fixed offsets from
 * the image end, ignoring how far that heap had grown. Once the heap passed
 * ~300 KB the first secondary thread's 64-page user stack sat INSIDE it.
 * thread_create then re-mapped (zeroed) live heap pages, and the thread's exit
 * unmapped 256 KiB of the main heap -- the next read of a big buffer faulted.
 * Real Qt6 hit it: a 518 KB QImage followed by a QThreadPool worker.
 *
 * Every phase fills the main heap with a position-dependent pattern and
 * re-checks the *whole* of it afterwards, so a clobbered page shows up as a
 * mismatch (or a page fault, if it was unmapped) rather than as silence:
 *
 *   1. grow the heap by ~1.5 MiB, create + join one thread, re-read it all
 *   2. several concurrent threads, each with its own stack and heap growth
 *   3. grow the main heap *while* a thread is alive: the heap must not run
 *      into the live thread's stack
 *   4. a thread that mallocs a lot gets it from the process-wide heap (one arena for all
 *      threads), without touching a neighbour thread's block or stack
 *   4b. memory a thread allocated outlives it and another thread can free it, and the reverse
 *      (what any threaded program does, Qt's events and strings included)
 *   5. keep creating threads until thread_create refuses: it must fail with -1
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

#define WORKER_HEAP (200u * 1024u)    /* well past the thread's initial heap */

extern "C" void *worker(void *arg) {
    job *j = (job *) arg;
    int ok = 1;
    u32 tag = 5000u + (u32) j->id * 17u;

    /* The thread's own arena has to hold a block bigger than its initial heap
     * (it grows in place) without running into a neighbouring thread. */
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
        free(mine);        /* the arena is unmapped when the thread exits */
    }

    j->ok = ok;
    j->done = 1;
    return 0;
}

/* Phase 4: every thread allocates from the one process-wide heap, which grows in place. `hog`
 * allocates a lot of it; `sentinel`, running at the same time, holds a pattern-filled block
 * and its own stack and must find both untouched, and every one of the hog's chunks must
 * keep its own pattern. */
#define SENTINEL_BYTES (100u * 1024u)
#define HOG_CHUNK      (64u * 1024u)
#define HOG_MAX_CHUNKS 200

struct pair {
    volatile int sentinel_ready;
    volatile int hog_done;
    volatile int sentinel_ok;
    volatile int sentinel_done;
    volatile int hog_chunks;
    volatile int hog_hit_null;
};
static pair g_pair;

extern "C" void *hog_worker(void *arg) {
    pair *p = (pair *) arg;
    while (!p->sentinel_ready)
        thread_yield();
    u8 *chunks[HOG_MAX_CHUNKS];
    int n = 0;
    while (n < HOG_MAX_CHUNKS) {
        u8 *c = (u8 *) malloc(HOG_CHUNK);
        if (!c) {
            p->hog_hit_null = 1;      /* not expected: 200 x 64 KiB fits the shared heap */
            break;
        }
        fill(c, HOG_CHUNK, 900u + (u32) n);
        chunks[n++] = c;
    }
    int good = 1;
    for (int i = 0; i < n; i++) {
        if (verify(chunks[i], HOG_CHUNK, 900u + (u32) i) >= 0)
            good = 0;
        free(chunks[i]);
    }
    p->hog_chunks = good ? n : -n;
    p->hog_done = 1;
    return 0;
}

extern "C" void *sentinel_worker(void *arg) {
    pair *p = (pair *) arg;
    u8 *mine = (u8 *) malloc(SENTINEL_BYTES);
    int ok = mine != 0;
    if (mine)
        fill(mine, SENTINEL_BYTES, 4242);
    p->sentinel_ready = 1;
    while (!p->hog_done)
        thread_yield();
    if (mine) {
        if (verify(mine, SENTINEL_BYTES, 4242) >= 0)
            ok = 0;
        if (deep(30, 4243) != 0)
            ok = 0;
        free(mine);
    }
    p->sentinel_ok = ok;
    p->sentinel_done = 1;
    return 0;
}

#define OWNER_BYTES (300u * 1024u)
static u8 *volatile g_owner_block;
static u8 *volatile g_handed;
static volatile int g_freed;

/* Allocates a block, fills it and exits: the block must outlive the thread. */
extern "C" void *owner_worker(void *arg) {
    (void) arg;
    u8 *b = (u8 *) malloc(OWNER_BYTES);
    if (b)
        fill(b, OWNER_BYTES, 555);
    g_owner_block = b;
    return 0;
}

/* Checks and frees a block the main thread allocated. */
extern "C" void *free_worker(void *arg) {
    (void) arg;
    if (g_handed && verify(g_handed, OWNER_BYTES, 556) < 0) {
        free(g_handed);
        g_freed = 1;
    }
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

    /* ---- phase 4: threads allocate from the shared process heap ----------- */
    int hog = thread_create((void *) hog_worker, (void *) &g_pair);
    int sen = thread_create((void *) sentinel_worker, (void *) &g_pair);
    CHECK(hog >= 0 && sen >= 0, "phase4 thread_create hog=%d sentinel=%d\n", hog, sen);
    if (hog >= 0)
        thread_join(hog);
    if (sen >= 0)
        thread_join(sen);
    CHECK(g_pair.hog_done && g_pair.sentinel_done, "phase4 hog_done=%d sentinel_done=%d\n",
          g_pair.hog_done, g_pair.sentinel_done);
    CHECK(g_pair.hog_chunks == HOG_MAX_CHUNKS, "phase4 hog got %d of %d chunks (own data corrupt if < 0)\n",
          g_pair.hog_chunks, HOG_MAX_CHUNKS);
    CHECK(!g_pair.hog_hit_null, "phase4 hog ran out of heap\n");
    CHECK(g_pair.sentinel_ok, "phase4 sentinel's block or stack was overrun\n");
    check_main_heap("after the hog", NSMALL + NSMALL2);
    printf((char *) "phase 4 (shared process heap): hog got %d x 64 KiB, malloc -> %s: %s\n",
           g_pair.hog_chunks, g_pair.hog_hit_null ? (char *) "NULL" : (char *) "never failed",
           g_ok ? (char *) "ok" : (char *) "BAD");

    /* ---- phase 4b: memory crosses threads in both directions -------------- */
    {
        g_owner_block = 0;
        int t1 = thread_create((void *) owner_worker, 0);
        if (t1 >= 0)
            thread_join(t1);
        CHECK(t1 >= 0 && g_owner_block != 0, "phase4b owner thread=%d block=%p\n", t1, (void *) g_owner_block);
        /* The thread that allocated it has exited; its block must still be there and intact. */
        CHECK(g_owner_block && verify(g_owner_block, OWNER_BYTES, 555) < 0, "phase4b block did not survive its thread\n");
        free(g_owner_block);                       /* freed by a different thread than the allocator */

        g_handed = (u8 *) malloc(OWNER_BYTES);
        if (g_handed)
            fill(g_handed, OWNER_BYTES, 556);
        g_freed = 0;
        int t2 = thread_create((void *) free_worker, 0);
        if (t2 >= 0)
            thread_join(t2);
        CHECK(g_handed && t2 >= 0 && g_freed == 1, "phase4b free_worker verified+freed=%d\n", g_freed);
        check_main_heap("after cross-thread free", NSMALL + NSMALL2);
        printf((char *) "phase 4b (memory crosses threads): %s\n", g_ok ? (char *) "ok" : (char *) "BAD");
    }

    /* ---- phase 5: run the slot region dry -------------------------------- */
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
    printf((char *) "phase 5: %d more threads created, then thread_create -> %d\n", made, refused);
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
