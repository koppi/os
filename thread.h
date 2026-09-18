/**
 * @file thread.h
 * @brief Thread control block. Threads form a ring per process; the scheduler
 *        switches between the head thread of each process.
 */
#pragma once

#include <types.h>

/** One thread of execution within a @ref process_t. */
typedef struct thread {
    pid_t pid;                      /**< Thread id. */
    int time;                       /**< Round-robin quantum, in PIT ticks. */
    int weight;                     /**< Scheduler weight: quantum = @c WEIGHT_BASE * @c weight. */
    int priority;                   /**< Scheduling priority, higher = more urgent (@c SCHED_PRIO_* in sched.h). */
    int policy;                     /**< @c SCHED_OTHER / @c SCHED_RR / @c SCHED_FIFO (see sched.h). */
    int yield;                      /**< Set by @ref sched_yield to drop the CPU on the next tick. */
    int main;                       /**< Non-zero for a process's main thread. */
    int state;                      /**< @c PROC_NEW / @c PROC_ACTIVE / @c PROC_STOPPED. */
    void *parent;                   /**< Owning @ref process_t. */
    uint32_t eip;                   /**< Entry / resume instruction pointer. */
    uint32_t esp;                   /**< User stack pointer. */
    uint32_t stack_limit;           /**< Top of the user stack. */
    uint32_t esp_kernel;            /**< Kernel stack pointer (saved on switch). */
    uint32_t stack_kernel_limit;    /**< Top of the kernel stack. */
    uint32_t heap;                  /**< Base of the user heap arena. */
    uint32_t heap_limit;            /**< Top of the user heap arena. */
    uint32_t image_base;            /**< Lowest vaddr of the loaded image. */
    uint32_t image_size;            /**< Image span, page-rounded. */
    uint8_t *fpu_state;            /**< 16-byte-aligned FXSAVE area (@ref FPU_STATE_SIZE). */
    void *fpu_state_raw;          /**< Unaligned base of @c fpu_state (for @c kfree). */
    struct thread *next;            /**< Next thread in the ring. */
    struct thread *prec;            /**< Previous thread in the ring. */
} thread_t;

/** @brief Allocate a zeroed thread control block with a fresh pid.
 *  @return The thread, or 0 if the heap is exhausted. Every field not listed
 *          in @ref thread_t as set here reads 0; the caller fills in the entry
 *          point, stacks and heap. */
thread_t *create_thread();
/** @brief Attach a clean, aligned FXSAVE area to @p thread. @return non-zero on success. */
int thread_alloc_fpu_state(thread_t *thread);
/** @brief `fork` syscall — unimplemented. @return -1, always. See thread.c
 *         for what a working version would need. */
int start_thread();
/** @brief `exit` syscall — stop the current thread with status @p code. */
void stop_thread(int code);
