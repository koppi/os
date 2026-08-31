/**
 * @file thread.c
 * @brief Threads within a process: allocation, `fork` and `exit`.
 */
#include <proc.h>
#include <thread.h>
#include <io.h>
#include <sched.h>
#include <lib/string.h>
#include <kheap.h>
#include <pit.h>
#include <printf.h>
#include <fpu.h>

/** asm helper (thread_asm) that returns twice, once in each thread. */
extern void fork_eip();

/** Next thread id to hand out (1 is the console's main thread). */
static int pid = 2;

/**
 * @brief Attach a fresh, 16-byte-aligned FXSAVE area seeded with a clean FPU
 *        state to @p thread. @return non-zero on success.
 *
 * Also used by @ref sched_init, which builds the console thread by hand.
 */
int thread_alloc_fpu_state(thread_t *thread) {
    thread->fpu_state_raw = kmalloc(FPU_STATE_SIZE + FPU_STATE_ALIGN);
    if(thread->fpu_state_raw == 0)
        return 0;
    thread->fpu_state = (uint8_t *) (((uintptr_t) thread->fpu_state_raw +
                                     (FPU_STATE_ALIGN - 1)) & ~(uintptr_t) (FPU_STATE_ALIGN - 1));
    fpu_default_state(thread->fpu_state);
    return 1;
}

/**
 * @brief Allocate a zeroed thread control block, self-linked into a ring.
 * @return The new thread, or 0 on allocation failure.
 */
thread_t *create_thread() {
    thread_t *thread = (thread_t *) kmalloc(sizeof(thread_t));
    if(thread == 0)
        return 0;
    if(!thread_alloc_fpu_state(thread)) {
        kfree(thread);
        return 0;
    }
    thread->pid = pid++;
    thread->main = 0;
    thread->time = 10;
    thread->priority = SCHED_PRIO_DEFAULT;
    thread->policy = SCHED_OTHER;
    thread->yield = 0;
    thread->state = PROC_NEW;
    thread->next = thread;
    thread->prec = thread;
    return thread;
}

/**
 * @brief `fork` syscall — clone the current thread within its process.
 *
 * Builds a fresh stack and heap for the child, copies the parent's stack and
 * first heap page, links the child into the thread ring, then forks the EIP:
 * the parent returns the child's pid, the child returns 0.
 *
 * @return Child pid in the parent, 0 in the child, -1 on failure.
 */
int start_thread() {
    sched_state(0);
    disable_int();
    process_t *cur = get_cur_proc();
    
    thread_t *thread = create_thread();
    if(!thread)
        return -1;
    
    thread_t *parent = cur->thread_list;
    
    thread->image_base = cur->thread_list->image_base;
    thread->image_size = cur->thread_list->image_size;
    thread->parent = (void *) cur;
    
    if(!build_stack(thread, cur->pdir, cur->threads + 1)) {
        kfree(thread);
        sched_state(1);
        enable_int();
        return -1;
    }
    
    if(!stack_fill(thread, 0, 0)) {
        kfree(thread);
        sched_state(1);
        enable_int();
        return -1;
    }
    
    memcpy((void *) thread->stack_limit - PAGE_SIZE, (void *) cur->thread_list->stack_limit - PAGE_SIZE, PAGE_SIZE);

    if(!build_heap(thread, cur->pdir, cur->threads + 1)) {
        kfree(thread);
        sched_state(1);
        enable_int();
        return -1;
    }
    memcpy((void *) thread->heap, (void *) cur->thread_list->heap, PAGE_SIZE);
    
    cur->threads++;
    
    thread->prec = cur->thread_list;
    thread->next = cur->thread_list->next;
    cur->thread_list->next->prec = thread;
    cur->thread_list->next = thread;
    
    // TODO fix splitting
    fork_eip();
    if(cur->thread_list == parent) {
        thread->state = PROC_ACTIVE;
        sched_state(1);
        enable_int();
        return thread->pid;
    } else {
        return 0;
    }
}

/**
 * @brief `exit` syscall — terminate the calling thread.
 *
 * Stopping a process's main thread stops the whole process (@ref end_proc);
 * otherwise the thread is unlinked, its stack/heap unmapped and its control
 * block freed. Spins until the scheduler switches away.
 *
 * @param code Exit status (passed to @ref end_proc for the main thread).
 */
void stop_thread(int code) {
    sched_state(0);

    process_t *cur = get_cur_proc();
    if(cur == 0) {
        printf("Process not found\n");
        sched_state(1);
        enable_int();
        while(1);
    }
    
    // Terminating the main thread will terminate the process
    if(cur->thread_list->main == 1)
        end_proc(code);
    
    thread_t *thread = cur->thread_list;
    
    cur->thread_list->next->prec = cur->thread_list->prec;
    cur->thread_list->prec->next = cur->thread_list->next;
    
    for(int p = 0; p < PROC_USER_STACK_PAGES; p++)
        vmm_unmap(cur->pdir, thread->stack_limit - (p + 1) * PAGE_SIZE);
    for(vmm_addr_t va = thread->heap; va < thread->heap_limit; va += PAGE_SIZE)
        vmm_unmap(cur->pdir, va);
    /* Kernel-stack frames are intentionally leaked - see remove_proc(). */

    kfree(thread->fpu_state_raw);
    kfree(thread);
    
    sched_state(1);
    enable_int();
    while(1);
}
