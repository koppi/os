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

#include <spinlock.h>

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
    /* The kernel heap does not zero what it hands out, and this block is only
     * partly filled in below — the caller sets the rest. Zero it first so a
     * field nobody assigns reads as 0 rather than as whatever the previous
     * owner of this block left there. That is exactly how fork's child ended
     * up with a garbage entry point. */
    memset(thread, 0, sizeof(thread_t));
    if(!thread_alloc_fpu_state(thread)) {
        kfree(thread);
        return 0;
    }
    thread->pid = pid++;
    thread->main = 0;
    thread->time = WEIGHT_BASE;
    thread->weight = 1;
    thread->priority = SCHED_PRIO_DEFAULT;
    thread->policy = SCHED_OTHER;
    thread->yield = 0;
    thread->state = PROC_NEW;
    thread->next = thread;
    thread->prec = thread;
    return thread;
}

/**
 * @brief `fork` syscall — unimplemented; always fails.
 * @return -1, always.
 *
 * There was a partial implementation here that allocated the child's stack and
 * heap, copied the parent's, and spliced the child into the thread ring. It
 * could not work, and the way it failed was silent and destructive:
 * @ref create_thread never set the new thread's @c eip and did not zero the
 * control block, so @ref stack_fill wrote whatever the kernel heap happened to
 * hold into the child's `iret` frame next to a ring-3 selector. The first time
 * the scheduler picked the child it `iret`ed into user mode at an
 * uninitialised address. The child therefore never reached the code that was
 * supposed to make it return 0, and that branch was unreachable.
 *
 * What a working version needs, for whoever picks this up (the removed code is
 * in the history, and most of it — the stack, heap and ring splice — was
 * sound):
 *
 *   - The child's kernel stack must be a copy of the parent's *current* one,
 *     syscall interrupt frame included, with @c esp_kernel pointing at the
 *     matching offset and the saved @c eax forced to 0. That is what makes the
 *     child return 0 through the ordinary syscall exit path, rather than
 *     needing an entry point of its own. @c fork_eip (pit_asm.S) was meant to
 *     be part of this and never was: it is a plain `ret`.
 *   - The two threads share one address space, so copying the parent's user
 *     stack to a different virtual address leaves every saved frame pointer in
 *     the copy aimed at the parent's stack. Either relocate them or accept
 *     that the child cannot return through its caller's frames.
 *
 * Until then this fails cleanly. Nothing in the tree calls fork(), so nothing
 * regresses; a caller gets -1 instead of a process that jumps to a garbage
 * address.
 */
int start_thread() {
    printf("fork: not implemented\n");
    return -1;
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

    uint32_t sf = spin_lock(&sched_lock);
    thread->state = PROC_STOPPED;
    thread->next->prec = thread->prec;
    thread->prec->next = thread->next;
    if(cur->thread_list == thread)
        cur->thread_list = thread->next;
    cur->threads--;
    spin_unlock(&sched_lock, sf);

    for(int p = 0; p < PROC_USER_STACK_PAGES; p++)
        vmm_unmap(cur->pdir, thread->stack_limit - (p + 1) * PAGE_SIZE);
    for(vmm_addr_t va = thread->heap; va < thread->heap_limit; va += PAGE_SIZE)
        vmm_unmap(cur->pdir, va);

    /* The kernel-stack frames and the thread control block are intentionally
     * leaked: this CPU is still executing on this thread's stack and
     * @c this_cpu()->current still points at it until the next tick switches
     * away, so freeing either here is a use-after-free on SMP. */

    sched_state(1);
    enable_int();
    while(1);
}
