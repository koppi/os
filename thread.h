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
    int time;                       /**< Time-slice length in ticks. */
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
    struct thread *next;            /**< Next thread in the ring. */
    struct thread *prec;            /**< Previous thread in the ring. */
} thread_t;

/** @brief Allocate a zeroed thread control block with a fresh pid. */
thread_t *create_thread();
/** @brief `fork` syscall — add a thread to the current process. */
int start_thread();
/** @brief `exit` syscall — stop the current thread with status @p code. */
void stop_thread(int code);
