/**
 * @file sched.h
 * @brief Cooperative round-robin scheduler over the process ring.
 */
#pragma once

#include <proc.h>

/** @return The currently running process. */
process_t *get_cur_proc();
/** @return The process owning thread id @p id, or NULL. */
process_t *get_proc_by_id(int id);
/**
 * @brief Timer-IRQ scheduler tick: save @p esp, pick the next runnable
 *        process/thread and return its kernel stack pointer.
 * @param esp Kernel ESP of the interrupted thread.
 * @return Kernel ESP to resume on.
 */
uint32_t schedule(uint32_t esp);
/** @brief Insert @p proc into the scheduler ring. */
void sched_add_proc(process_t *proc);
/** @brief Unlink the process owning thread id @p id from the ring. */
void sched_remove_proc(int id);
/** @brief Build the first process and iret into it; does not return. */
void sched_init();
/** @return Number of processes in the ring. */
int get_nproc();
/** @brief Print the process table (the `ps` command). */
void print_procs();
