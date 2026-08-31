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
    thread_t *thread_list;    /**< Current thread (head of the ring). */
    struct proc *next;        /**< Next process in the scheduler ring. */
    struct proc *prec;        /**< Previous process in the scheduler ring. */
} process_t;

/** asm stub at @ref RETURN_ADDR — turns `main`'s return into an exit syscall. */
extern void end_process();

/** @brief Load an ELF, build its stack/heap and add it to the scheduler. */
int start_proc(char *name, char *arguments);
/** @brief Map a user + kernel stack for @p thread. */
int build_stack(thread_t *thread, page_dir_t *pdir, int nthreads);
/** @brief Tokenise @p arguments onto @p thread's heap as argv/argc. */
int heap_fill(thread_t *thread, char *name, char *arguments, uint32_t *argc, uint32_t *argv1);
/** @brief Push argv/argc/return-address and the initial iret frame. */
int stack_fill(thread_t *thread, uint32_t argc, uint32_t argv);
/** @brief Map @p thread's 4-page user heap and initialise it. */
int build_heap(thread_t *thread, page_dir_t *pdir, int nthreads);
/** @brief `exit`/return handler: mark the current process stopped. */
void end_proc(int ret);
/** @brief Free a stopped process's address space and control block. */
void remove_proc(int pid);
/** @brief Create a kernel-mode process running function @p addr. */
int start_kernel_proc(char *name, void *addr);
/** @return The state of process @p id (@c PROC_STOPPED if unknown). */
int proc_state(int id);
