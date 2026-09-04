/**
 * @file sched.h
 * @brief Preemptive fixed-priority round-robin scheduler with real-time
 *        policies, driven from the PIT tick.
 *
 * Every runnable thread carries a @ref thread_t::priority and a
 * @ref thread_t::policy. On each tick @ref schedule always runs a thread of the
 * highest priority that is ready; threads that share the top priority take turns
 * round-robin. A thread of higher priority becoming ready preempts a lower one
 * within a tick (~1 ms), which gives the scheduler its soft real-time property.
 *
 * Policies:
 *  - @c SCHED_OTHER — time-shared. Round-robin with a quantum; may be starved
 *    by real-time threads. Every thread starts here.
 *  - @c SCHED_RR    — real-time round-robin: like @c SCHED_OTHER but in the
 *    real-time band, so it preempts every @c SCHED_OTHER thread.
 *  - @c SCHED_FIFO  — real-time, run-to-completion: keeps the CPU against
 *    equal-priority threads until it blocks, calls @ref sched_yield or is
 *    preempted by a *higher* priority thread.
 */
#pragma once

#include <proc.h>

/** @name Scheduling policies (@ref thread_t::policy) */
///@{
#define SCHED_OTHER  0  /**< Time-shared round-robin (default, non real-time). */
#define SCHED_RR     1  /**< Real-time round-robin with a quantum. */
#define SCHED_FIFO   2  /**< Real-time, runs until it blocks/yields/is preempted. */
///@}

/** @name Priority band (@ref thread_t::priority) */
///@{
#define SCHED_PRIO_MIN      0   /**< Lowest priority. */
#define SCHED_PRIO_MAX      31  /**< Highest priority. */
#define SCHED_PRIO_DEFAULT  8   /**< Priority every thread starts at. */
#define SCHED_PRIO_RT_MIN   16  /**< A priority >= this is a real-time priority. */
///@}

/** Base number of PIT ticks in a default time slice (scaled by @ref thread_t::weight). */
#define WEIGHT_BASE 10

/** @return Non-zero if priority @p p lies in the real-time band. */
#define SCHED_IS_RT(p)  ((p) >= SCHED_PRIO_RT_MIN)

/** @return The currently running process. */
process_t *get_cur_proc();
/** @return The process owning thread id @p id, or NULL. */
process_t *get_proc_by_id(int id);
/**
 * @brief Timer/IPI scheduler tick: save @p esp, pick the next runnable
 *        process/thread and hand the asm stub what it needs to resume it.
 *
 * @param esp Kernel ESP of the interrupted thread.
 * @return A packed value: the low 32 bits are the kernel ESP to resume on; the
 *         high 32 bits are the page-directory (CR3) to load, or 0 to keep the
 *         current one. The caller must load CR3 *after* switching ESP — the
 *         outgoing thread's kernel stack may not be mapped in the new address
 *         space, and this one's may not be mapped in the old one.
 */
uint64_t schedule(uint32_t esp);
/**
 * @brief Voluntarily give up the CPU.
 *
 * Marks the caller's thread as yielding and halts until the next tick runs the
 * scheduler, which then picks another ready thread of the same (or higher)
 * priority. A no-op when preemption is disabled. Mostly useful for
 * @c SCHED_FIFO threads, which are otherwise never rotated by the timer.
 */
void sched_yield(void);
/**
 * @brief Change thread @p pid's scheduling priority.
 * @param pid      Thread id.
 * @param priority Clamped to [@c SCHED_PRIO_MIN, @c SCHED_PRIO_MAX].
 * @return 0 on success, -1 if no such thread.
 */
int sched_set_priority(int pid, int priority);
/**
 * @brief Change thread @p pid's scheduling policy.
 * @param pid    Thread id.
 * @param policy One of @c SCHED_OTHER / @c SCHED_RR / @c SCHED_FIFO.
 * @return 0 on success, -1 on a bad policy or unknown thread.
 */
int sched_set_policy(int pid, int policy);
/**
 * @brief Change thread @p pid's scheduler weight (scales its quantum).
 * @param pid    Thread id.
 * @param weight New weight (must be > 0).
 * @return 0 on success, -1 if no such thread.
 */
int sched_set_weight(int pid, int weight);
/** @brief Insert @p proc into the scheduler ring. */
void sched_add_proc(process_t *proc);
/** @brief Unlink the process owning thread id @p id from the ring. */
void sched_remove_proc(int id);
/** @brief Build the first process and iret into it; does not return. */
void sched_init();
/**
 * @brief Adopt @p t on the calling CPU and `iret` into it; never returns.
 *
 * Used for a CPU's first entry into a thread: @ref sched_init for the console,
 * @ref ap_main for an application processor's idle thread. @p t's kernel stack
 * must already hold the frame @ref schedule expects.
 */
void sched_run_thread(struct thread *t) __attribute__((noreturn));
/** @return Number of processes in the ring. */
int get_nproc();
/** @brief Print the process table (the `ps` command). */
void print_procs();
