/**
 * @file proc.h
 * @brief Process control block, the saved-register frames the exception stubs
 *        build, and the process lifecycle API.
 */
#pragma once

#include <types.h>
#include <thread.h>
#include <paging.h>

#define PROC_NULL       -1  /**< "no process" sentinel. */

#define PROC_STOPPED    0   /**< Finished; awaiting reaping. */
#define PROC_ACTIVE     1   /**< Runnable. */
#define PROC_NEW        2   /**< Being constructed. */

/** Virtual address of the userspace return stub (see sched.c). */
#define RETURN_ADDR 0x400000

/** Per-process userspace stack, in 4 KiB pages (256 KiB). */
#define PROC_USER_STACK_PAGES   64
/** Per-process ring-0 stack for syscalls / IRQs, in pages (16 KiB). */
#define PROC_KERNEL_STACK_PAGES 4
/** Initial userspace heap arena, in pages. Grows on demand (see heap.c). */
#define PROC_HEAP_PAGES         4
/** Ceiling on the main thread's heap arena (it grows in place; see heap.c). */
#define PROC_HEAP_MAX           (64u * 1024u * 1024u)

/**
 * A secondary thread's heap window, in pages (512 KiB): its arena starts at
 * @ref PROC_HEAP_PAGES and grows in place, but never past this. The window is
 * what keeps one thread's malloc from running up into the next thread's slot.
 */
#define PROC_THREAD_HEAP_PAGES  128
/**
 * Pages in one secondary-thread slot in the @ref UTHREAD_REGION_BASE region,
 * low to high: guard, user stack, guard, kernel stack, guard, heap window.
 * The guards are never mapped, so an overrun of the user stack, the kernel
 * stack or the heap window faults instead of landing in the neighbouring part.
 */
#define PROC_THREAD_SLOT_PAGES  (1 + PROC_USER_STACK_PAGES + 1 + \
                                 PROC_KERNEL_STACK_PAGES + 1 + PROC_THREAD_HEAP_PAGES)
/** Secondary-thread slots one process can ever use (slots are not recycled:
 *  an exited thread's kernel stack is deliberately leaked, see stop_thread()). */
#define PROC_THREAD_SLOTS_MAX   ((int) ((UTHREAD_REGION_END - UTHREAD_REGION_BASE) / \
                                 (PROC_THREAD_SLOT_PAGES * PAGE_SIZE)))

_Static_assert(PROC_THREAD_SLOTS_MAX >= 16,
               "the thread region must hold a useful number of slots");

/** Register frame pushed by an interrupt stub with no error code. */
struct regs {
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eip;
    uint32_t cs;
};

/** Register frame pushed by an interrupt stub that has an error code. */
struct regs_error {
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t error;
    uint32_t eip;
    uint32_t cs;
};

/** A process: an address space plus a ring of threads. Processes form a ring. */
typedef struct proc {
    char name[16];            /**< Program path, truncated. */
    int state;                /**< @c PROC_NEW / @c PROC_ACTIVE / @c PROC_STOPPED. */
    page_dir_t *pdir;         /**< Page directory. */
    int threads;              /**< Thread count. */
    int thread_slots;         /**< Lowest build_stack()/build_heap() nthreads index
                                    create_user_thread() has not handed out yet
                                    (1..PROC_THREAD_SLOTS_MAX; slot 0 is the main
                                    thread). Monotonic, unlike @c threads, so a
                                    thread that exits and one created after it
                                    never share a virtual-address span. */
    thread_t *thread_list;    /**< Current thread (head of the ring). */
    int cpu;                  /**< CPU index running this process now, -1 if none (SMP). */
    uint32_t last_ran;        /**< pit_ms() when last scheduled (round-robin tiebreak). */
    struct proc *next;        /**< Next process in the scheduler ring. */
    struct proc *prec;        /**< Previous process in the scheduler ring. */
} process_t;

/** asm stub at @ref RETURN_ADDR — turns `main`'s return into an exit syscall. */
extern void end_process();

/** @brief Load an ELF, build its stack/heap and add it to the scheduler. */
int start_proc(char *name, char *arguments);
/** @brief Map a user + kernel stack for @p thread. @p nthreads 0 puts them
 *  above the image (main thread); n >= 1 is secondary-thread slot n. */
int build_stack(thread_t *thread, page_dir_t *pdir, int nthreads);
/** @brief Tokenise @p arguments onto @p thread's heap as argv/argc. */
int heap_fill(thread_t *thread, char *name, char *arguments, uint32_t *argc, uint32_t *argv1);
/** @brief Push argv/argc/return-address and the initial iret frame. */
int stack_fill(thread_t *thread, uint32_t argc, uint32_t argv);
/** @brief Map @p thread's 4-page user heap, initialise it and set its
 *  @c heap_ceiling. */
int build_heap(thread_t *thread, page_dir_t *pdir, int nthreads);
/** @brief `thread_create` syscall backend: start a new thread inside @p proc,
 *  sharing its address space, at @p entry with one argument @p arg.
 *  @return The new thread's pid, or -1 on failure (no free slot, out of
 *  memory). A failed call leaves the address space as it found it. */
int create_user_thread(process_t *proc, uint32_t entry, uint32_t arg);
/** @brief `exit`/return handler: mark the current process stopped. */
void end_proc(int ret);
/** @brief Free a stopped process's address space and control block. */
void remove_proc(int pid);
/** @brief Create a kernel-mode process running function @p addr. */
int start_kernel_proc(char *name, void (*thread)(void));
/** @return The state of process @p id (@c PROC_STOPPED if unknown). */
int proc_state(int id);
