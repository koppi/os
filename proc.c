/**
 * @file proc.c
 * @brief Process lifecycle: address-space creation, ELF load, stack/heap/argv
 *        setup, the initial iret frame, exit and teardown.
 */
#include <proc.h>
#include <io.h>
#include <tss.h>
#include <elf.h>
#include <sched.h>
#include <lib/string.h>
#include <kheap.h>
#include <printf.h>
#include <sched.h>
#include <pit.h>
#include <percpu.h>
#include <spinlock.h>
#include <log.h>
#include <video.h>
#include <keyboard.h>
#include <snd.h>

/*
 * Process in memory
 * |-----------image_start---------------|
 * |                                     |
 * |-------image_start + image_size------|
 * |              padding                |
 * |------------user stack---------------| ---|
 * |               4096B                 |    |
 * |-----------kernel stack--------------|    |
 * |               4096B                 |    | x number of threads
 * |-------------user heap---------------|    |
 * |               4096B                 |    |
 * |-------------------------------------| ---|
 */

/**
 * @brief Build a process from an ELF image and put it on the run queue.
 * @param name      Path to the executable.
 * @param arguments Command line, split on spaces into argv.
 * @return The new main thread's id, or @c PROC_STOPPED on any failure.
 *
 * The steps are ordered by dependency: address space, thread, ELF load,
 * stacks, heap, argv into the heap, then the initial frames onto the stacks.
 * Each needs the previous one's addresses.
 *
 * Failure is reported but not unwound — the page directory, the thread control
 * block and any frames already mapped are leaked. A process that fails to
 * start is rare and fatal to what asked for it, so nothing here tries to be
 * recoverable.
 *
 * The error paths do not touch @ref sched_state. Nothing here ever closes the
 * preemption gate, so re-opening it on the way out would have handed the
 * caller a different gate state than it arrived with. The gate is irrelevant
 * inside this function anyway: @ref proc_lock is held with local interrupts
 * disabled, so no timer tick can preempt this CPU regardless.
 *
 * Caller holds @ref proc_lock; see @ref start_proc.
 */
static int start_proc_locked(char *name, char *arguments) {
    process_t *proc = (process_t *) kmalloc(sizeof(process_t));
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->state = PROC_NEW;
    proc->cpu = -1;
    proc->last_ran = 0;

    // Create a new page directory
    proc->pdir = create_address_space();
    if(!proc->pdir) {
        printf("Failed finding address space\n");
        return PROC_STOPPED;
    }
    
    proc->thread_list = create_thread();
    if(proc->thread_list == 0)
        return PROC_STOPPED;
    proc->thread_list->main = 1;
    proc->thread_list->parent = (void *) proc;

    if(!load_elf(name, proc->thread_list, proc->pdir)) {
        return PROC_STOPPED;
    }
    
    if(!build_stack(proc->thread_list, proc->pdir, 0)) {
        printf("Failed allocating memory, error 1\n");
        return PROC_STOPPED;
    }
    
    if(!build_heap(proc->thread_list, proc->pdir, 0)) {
        printf("Failed allocating memory, error 2\n");
        return PROC_STOPPED;
    }
    
    uint32_t argc, argv;
    
    if(!heap_fill(proc->thread_list, name, arguments, &argc, &argv)) {
        printf("Failed allocating memory, error 3\n");
        return PROC_STOPPED;
    }
    
    if(!stack_fill(proc->thread_list, argc, argv)) {
        printf("Failed allocating memory, error 4\n");
        return PROC_STOPPED;
    }
    
    proc->threads = 1;
    
    proc->thread_list->state = PROC_ACTIVE;
    proc->state = PROC_ACTIVE;

    sched_add_proc(proc);
    return proc->thread_list->pid;
}

/**
 * @brief Load an ELF and add it to the run queue.
 *
 * Serialised with @ref proc_lock: address-space construction hands out physical
 * frames and briefly aliases pages into the kernel directory, which must not
 * race another process being built on a different CPU.
 */
int start_proc(char *name, char *arguments) {
    uint32_t f = spin_lock(&proc_lock);
    int r = start_proc_locked(name, arguments);
    spin_unlock(&proc_lock, f);
    return r;
}

/**
 * @brief Map a run of pages into both the kernel and the target address space.
 * @param pdir  The process's page directory.
 * @param base  First virtual address; the same address is used in both spaces.
 * @param pages How many consecutive pages.
 * @param user  Non-zero to make the pages reachable from ring 3.
 * @return 1 on success, 0 if a mapping failed.
 *
 * The double mapping exists because this runs on the kernel directory: the
 * frames have to be addressable here to be zeroed and seeded with argv and the
 * initial stack frames. The kernel-side aliases are temporary and the caller
 * must drop them once seeding is done — @ref stack_fill and @ref heap_fill do
 * that for the ranges they fill.
 *
 * The @p user flag is what keeps a thread's kernel stack out of ring 3's
 * reach: it holds the saved user context and every frame a syscall builds, so
 * mapped @c PAGE_USER it was writable by the process it belongs to.
 *
 * A failure part-way leaves the pages already mapped in place; the caller
 * abandons the whole process rather than unwinding.
 */
static int map_user_range(page_dir_t *pdir, vmm_addr_t base, int pages, int user) {
    uint32_t flags = PAGE_PRESENT | PAGE_RW | (user ? PAGE_USER : 0u);
    for(int i = 0; i < pages; i++) {
        vmm_addr_t va = base + (uint32_t) i * PAGE_SIZE;
        if(!vmm_map(get_kern_directory(), va, PAGE_PRESENT | PAGE_RW) ||
           !vmm_map_phys(pdir, va,
                         (uint32_t) get_phys_addr(get_kern_directory(), va),
                         flags))
            return 0;
        memset((void *) va, 0, PAGE_SIZE);
    }
    return 1;
}

/**
 * @brief Map a thread's user and kernel stacks above its image.
 * @param thread   The thread; its stack pointers and limits are filled in.
 * @param pdir     The process's page directory.
 * @param nthreads Index of this thread within the process, 0 for the main one.
 * @return 1 on success, 0 if a mapping failed.
 *
 * The main thread is laid out immediately above the image; every later thread
 * is offset by a fixed per-thread span so threads miss the image and each
 * other.
 *
 * An unmapped guard page separates the two stacks. They used to be adjacent,
 * so a kernel stack that overran its bottom walked into the top of the user
 * stack with nothing faulting — the damage only surfaced later as a return
 * through a wrecked frame. The kernel stack is also mapped ring-0 only, since
 * it holds the saved user context.
 */
int build_stack(thread_t *thread, page_dir_t *pdir, int nthreads) {
    /* Per-thread footprint: user stack + kernel stack + heap, plus slack. Only
     * the main thread (nthreads == 0) is laid out exactly; forked threads are
     * offset by this much so they miss the image and each other. */
    uint32_t span = (uint32_t) nthreads * PAGE_SIZE *
                    (PROC_USER_STACK_PAGES + PROC_KERNEL_STACK_PAGES + PROC_HEAP_PAGES + 8);

    uint32_t ustack_base = thread->image_base + thread->image_size + span;
    if(!map_user_range(pdir, ustack_base, PROC_USER_STACK_PAGES, 1))
        return 0;
    thread->esp = ustack_base;   /* real SP set by stack_fill() */
    thread->stack_limit = ustack_base + PROC_USER_STACK_PAGES * PAGE_SIZE;

    /* One unmapped page between the two stacks. They used to be adjacent, so a
     * kernel stack that ran past its bottom walked straight into the top of the
     * user stack — both mapped, so nothing faulted and the damage only surfaced
     * later as a return into a wrecked frame. Now it faults on the guard. */
    thread->esp_kernel = thread->stack_limit + PAGE_SIZE;
    thread->stack_kernel_limit = thread->esp_kernel + PROC_KERNEL_STACK_PAGES * PAGE_SIZE;
    /* Ring 0 only: this stack holds the saved user context and every kernel
     * frame a syscall builds. Mapped PAGE_USER it was writable from ring 3. */
    if(!map_user_range(pdir, thread->esp_kernel, PROC_KERNEL_STACK_PAGES, 0))
        return 0;

    return 1;
}

/**
 * @brief Map and initialise a thread's user heap.
 * @param thread   The thread; its @c heap and @c heap_limit are filled in.
 * @param pdir     The process's page directory.
 * @param nthreads Index of this thread within the process, 0 for the main one.
 * @return 1 on success, 0 if the mapping failed.
 *
 * Placed just above the thread's kernel stack, offset by the same per-thread
 * span @ref build_stack uses so threads of one process do not overlap. The
 * kernel-side aliases are left in place for @ref heap_fill to seed argv
 * through, and dropped there.
 */
int build_heap(thread_t *thread, page_dir_t *pdir, int nthreads) {
    uint32_t span = (uint32_t) nthreads * PAGE_SIZE *
                    (PROC_USER_STACK_PAGES + PROC_KERNEL_STACK_PAGES + PROC_HEAP_PAGES + 8);
    vmm_addr_t heap = thread->stack_kernel_limit + span;

    if(!map_user_range(pdir, heap, PROC_HEAP_PAGES, 1))
        return 0;

    thread->heap = heap;
    thread->heap_limit = heap + PROC_HEAP_PAGES * PAGE_SIZE;

    heap_init((vmm_addr_t *) heap, PROC_HEAP_PAGES * PAGE_SIZE);

    return 1;
}

/** Most arguments a process can be given, argv[0] included. */
#define PROC_MAX_ARGV 15

/**
 * @brief Build argv in the process's own heap.
 * @param thread    The thread whose heap to allocate from.
 * @param name      Becomes argv[0].
 * @param arguments The rest of the command line.
 * @param argc      Receives the argument count.
 * @param argv1     Receives the address of the argv array, in process space.
 * @return 1 always.
 *
 * Splits on single spaces with no quoting and no escape handling, so an
 * argument cannot contain one; arguments past @ref PROC_MAX_ARGV are dropped
 * silently. The array is NULL-terminated as C requires.
 *
 * This is the last thing the kernel writes into the process heap, so it drops
 * the kernel-directory aliases @ref build_heap left behind on the way out.
 * Anything that seeded the heap after this would fault.
 */
int heap_fill(thread_t *thread, char *name, char *arguments, uint32_t *argc, uint32_t *argv1) {
    *argc = 1;
    char **argv = (char **) umalloc((PROC_MAX_ARGV + 1) * sizeof(char *),
                                    (vmm_addr_t *) thread->heap);
    argv[0] = (char *) umalloc(strlen(name) + 1, (vmm_addr_t *) thread->heap);
    strcpy(argv[0], name);

    while(*arguments && *argc < PROC_MAX_ARGV) {
        char *p = strchr(arguments, ' ');
        if(p == 0) {
            argv[*argc] = (char *) umalloc(strlen(arguments) + 1, (vmm_addr_t *) thread->heap);
            strcpy(argv[*argc], arguments);
            (*argc)++;
            break;
        }
        int strl = strlen(arguments) - strlen(p);
        argv[*argc] = (char *) umalloc(strl + 1, (vmm_addr_t *) thread->heap);
        strncpy(argv[*argc], arguments, strl);
        (*argc)++;
        while(strl--) {
            arguments++;
        }
        arguments++;
    }
    argv[*argc] = 0;   /* C requires argv[argc] == NULL */

    *argv1 = (uint32_t) argv;
    for(int i = 0; i < PROC_HEAP_PAGES; i++)
        vmm_unmap_phys(get_kern_directory(), thread->heap + (uint32_t) i * PAGE_SIZE);

    return 1;
}

/**
 * @brief Lay out the initial user stack and the frame that enters the process.
 * @param thread The thread; its @c esp and @c esp_kernel are set to the frames.
 * @param argc   Argument count, pushed for main().
 * @param argv   Address of the argv array in process space.
 * @return 1 always.
 *
 * The user stack gets argv, argc and a return address pointing at the
 * end-of-process trampoline, so a process that returns from main() lands
 * somewhere that can call exit for it. The kernel stack gets an `iret` frame
 * with ring-3 selectors, which is how the process is first entered: the
 * scheduler switches to this thread like any other and the stub `iret`s
 * straight into user mode.
 *
 * Drops the kernel-directory aliases for both stacks on the way out, so this
 * must be the last thing the kernel writes into them.
 */
int stack_fill(thread_t *thread, uint32_t argc, uint32_t argv) {
    // Fill user stack
    uint32_t *stackp = (uint32_t *) thread->stack_limit;
    *--stackp = argv;
    *--stackp = argc;
    *--stackp = (uint32_t) RETURN_ADDR;     // The process needs to know where to return
    thread->esp = (uint32_t) stackp;
    
    // Fill kernel stack
    stackp = (uint32_t *) thread->stack_kernel_limit;
    *--stackp = 0x23;                                       // ss
    *--stackp = thread->esp;                                // esp
    *--stackp = 0x202;                                      // eflags
    *--stackp = 0x1B;                                       // cs
    *--stackp = thread->eip;                                // eip
    *--stackp = 0;                                          // eax
    *--stackp = 0;                                          // ebx
    *--stackp = 0;                                          // ecx
    *--stackp = 0;                                          // edx
    *--stackp = 0;                                          // esi
    *--stackp = 0;                                          // edi
    *--stackp = thread->stack_limit;                        // ebp
    *--stackp = 0x23;                                       // ds
    *--stackp = 0x23;                                       // es
    *--stackp = 0x23;                                       // fs
    *--stackp = 0x23;                                       // gs
    thread->esp_kernel = (uint32_t) stackp;

    /* Drop the kernel-directory aliases map_user_range() left behind; the
     * frames stay mapped in the process directory. */
    vmm_addr_t ustk = thread->stack_limit - PROC_USER_STACK_PAGES * PAGE_SIZE;
    vmm_addr_t kstk = thread->stack_kernel_limit - PROC_KERNEL_STACK_PAGES * PAGE_SIZE;
    for(int p = 0; p < PROC_USER_STACK_PAGES; p++)
        vmm_unmap_phys(get_kern_directory(), ustk + (uint32_t) p * PAGE_SIZE);
    for(int p = 0; p < PROC_KERNEL_STACK_PAGES; p++)
        vmm_unmap_phys(get_kern_directory(), kstk + (uint32_t) p * PAGE_SIZE);

    return 1;
}

/**
 * @brief Mark the calling process stopped and stop running it. Never returns.
 * @param ret The process's exit code, reported to the console.
 *
 * Despite the name it frees nothing: it flags the process and then spins with
 * interrupts enabled, waiting to be scheduled away for the last time. The
 * memory is reclaimed later by @ref remove_proc, running on another process's
 * stack — which is the point, since a process cannot free the stack it is
 * standing on.
 */
void end_proc(int ret) {
    sched_state(0);

    process_t *cur = get_cur_proc();
    if(cur == 0) {
        printf("Process not found\n");
        sched_state(1);
        enable_int();
        while(1);
    }
    
    if(ret)
        printf("Process %d returned with error: %d\n",
               cur->thread_list->pid, ret);
    else
        printf("Process returned with exit code 0.\n");
    
    cur->state = PROC_STOPPED;
    
    sched_state(1);
    enable_int();
    while(1);
}

/**
 * @brief Reclaim a stopped process: unmap its memory and free its structures.
 * @param pid The process's main-thread id; unknown ids are ignored.
 *
 * Ordering is the whole difficulty, because another CPU may still be running
 * in this address space:
 *
 *   1. Unlink from the run queue, so no CPU can pick it up again.
 *   2. Spin until no CPU reports it as current.
 *   3. Flush the TLB across all CPUs — not for the TLB, but as a barrier. A
 *      CPU only services the IPI between instructions with interrupts on, so
 *      by the time the flush completes every CPU has finished its
 *      context-switch stub (which runs with interrupts off) and left this
 *      address space. Step 2 alone is not enough: the scheduler clears its
 *      current-process fields a few instructions before the stub reloads CR3.
 *   4. Only then unmap and free.
 *
 * Kernel-stack frames are deliberately leaked. Unmapping them here corrupts a
 * live kernel-heap allocation once a few sizeable processes have run in
 * sequence; at four pages per process the leak is bounded and, for an
 * interactive workload, benign. See apps/lua/PORTING.md.
 */
void remove_proc(int pid) {
    process_t *cur = get_proc_by_id(pid);
    if(cur == 0)
        return;

    /*
     * Drop any full-screen grab the process was holding. A program that
     * exits through gfx_close (syscall 23) has already released it, but one
     * that faults -- or is killed between taking the screen and giving it
     * back -- would otherwise leave the compositor parked and the display
     * frozen on its last frame, with no way back short of a reboot. Only one
     * grab exists at a time and only a user process can take it, so reaping
     * is the right place to be sure it is gone.
     */
    if(video_grabbed()) {
        keyboard_raw_mode(0);
        video_ungrab();
    }
    /* Same for the PCM output (snd.c): left open, it would keep the module
     * silenced and the card draining an empty ring forever. */
    if(snd_user_active())
        snd_user_close();

    uint32_t plf = spin_lock(&proc_lock);

    /* Unlink it from the run queue first, then wait until whichever CPU was
     * running it has switched away (its next LAPIC tick sees state != ACTIVE),
     * so we never free page tables that are still live in some CPU's CR3. */
    if(cur->thread_list && cur->thread_list->main)
        sched_remove_proc(cur->thread_list->pid);
    for(;;) {
        int busy = 0;
        for(int i = 0; i < ncpu; i++)
            if(cpus[i].current_proc == cur || cpus[i].current_dir == cur->pdir)
                busy = 1;
        if(!busy)
            break;
        asm volatile("pause");
    }
    /* current_proc/current_dir are cleared inside schedule() a few instructions
     * before the tick stub actually reloads CR3, so a CPU can still be running
     * on cur->pdir here. A full cross-CPU TLB shootdown is a barrier: another
     * CPU only services the IPI once it is between instructions with IF set,
     * i.e. its context-switch stub (which runs with IF clear) has completed and
     * it has left cur's address space. After this it is safe to free. */
    flush_tlb(0);

    // Remove the executable
    for(uint32_t page = 0; page < cur->thread_list->image_size / PAGE_SIZE; page++) {
        vmm_unmap(cur->pdir, cur->thread_list->image_base + (page * PAGE_SIZE));
    }

    for(int i = 0; i < cur->threads; i++) {
        thread_t *thread = cur->thread_list;

        for(int p = 0; p < PROC_USER_STACK_PAGES; p++)
            vmm_unmap(cur->pdir, thread->stack_limit - (p + 1) * PAGE_SIZE);
        for(vmm_addr_t va = thread->heap; va < thread->heap_limit; va += PAGE_SIZE)
            vmm_unmap(cur->pdir, va);
        /* Kernel-stack frames are intentionally leaked: unmapping them here (or
         * in stop_thread) corrupts a still-in-use allocation somewhere in the
         * kernel heap once a few sizeable processes have run in sequence. The
         * per-process kstack is 4 pages; the leak is bounded and benign for the
         * interactive workload. See apps/lua/PORTING.md. */

        cur->thread_list = thread->next;
        kfree(thread->fpu_state_raw);
        kfree(thread);
    }

    if(get_page_directory() == cur->pdir)
        change_page_directory(get_kern_directory());
    delete_address_space(cur->pdir);
    kfree(cur);

    spin_unlock(&proc_lock, plf);
}

/**
 * @brief Start a kernel thread running @p thread in the kernel address space.
 * @param name   Name for the process table.
 * @param thread The function to run; it should never return.
 * @return The new thread's id, or @c PROC_STOPPED if no stack window is free.
 *
 * Much simpler than @ref start_proc_locked: no ELF, no user stack, no heap,
 * and it shares the kernel page directory rather than getting its own. The
 * initial frame carries ring-0 selectors, so the thread starts in kernel mode.
 *
 * Stacks come from a fixed window below the user-image base, handed out by a
 * bump allocator that never reclaims — a kernel thread is expected to run for
 * the life of the machine. The window's placement matters: every `link.lds` in
 * apps/ links at 8 MiB and @ref load_elf_relocate copies the image over that
 * range, so stacks that used to sit just above it were being overwritten by
 * the first program to run. Running out of window is refused and logged rather
 * than allowed to grow into the console thread's stacks.
 */
int start_kernel_proc(char *name, void (*thread)(void)) {
    /* Each kernel process gets its own stack window below the user-image base.
     * The old base (KERNEL_SPACE_END + 0x5000 = 0x805000) sat INSIDE the
     * programs' address space: every `link.lds` in apps/ links at `. = 8M`, and
     * load_elf_relocate() maps and copies the image (e.g. /rd/zsh, 27 KB) right
     * over these live stacks, corrupting the running kernel thread. Place them
     * in the free identity-mapped window 0x440000..0x600000, just past the
     * page-table storage window (page_start .. 0x43c000). Bump the base so a
     * second (third, ...) kernel thread does not land on the previous one. */
    static uint32_t kproc_stack_base = KPROC_STACK_BASE;

    uint32_t plf = spin_lock(&proc_lock);

    /* Stop where the console thread's stacks begin, rather than growing into
     * them: that overlap is the one this window was moved here to escape. */
    if (kproc_stack_base + 0x4000 > KPROC_STACK_END) {
        klogf(LOG_ERR, "proc: kernel stack window full, '%s' not started\n", name);
        spin_unlock(&proc_lock, plf);
        return PROC_STOPPED;
    }

    process_t *proc = (process_t *) kmalloc(sizeof(process_t));
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->state = PROC_NEW;
    proc->cpu = -1;
    proc->last_ran = 0;
    proc->pdir = get_kern_directory();
    proc->thread_list = create_thread();
    if(proc->thread_list == 0) {
        spin_unlock(&proc_lock, plf);
        return PROC_STOPPED;
    }
    proc->thread_list->main = 1;
    proc->thread_list->parent = (void *) proc;
    proc->thread_list->eip = (uint32_t) (uintptr_t) thread;

    uint32_t stack = kproc_stack_base;
    kproc_stack_base += 0x4000;   /* user page + kernel page + guard pages */

    vmm_map(proc->pdir, (vmm_addr_t) stack, PAGE_PRESENT | PAGE_RW);

    proc->thread_list->esp = stack;
    proc->thread_list->stack_limit = ((uint32_t) proc->thread_list->esp + PAGE_SIZE);
    
    proc->thread_list->esp_kernel = proc->thread_list->stack_limit;
    proc->thread_list->stack_kernel_limit = proc->thread_list->esp_kernel + PAGE_SIZE;
    
    vmm_map(proc->pdir, proc->thread_list->esp_kernel, PAGE_PRESENT | PAGE_RW);
    
    uint32_t *stackp = (uint32_t *) proc->thread_list->stack_kernel_limit;
    *--stackp = 0x10;                     // ss
    *--stackp = proc->thread_list->esp;   // esp
    *--stackp = 0x202;                    // eflags
    *--stackp = 0x8;                      // cs
    *--stackp = proc->thread_list->eip;   // eip
    *--stackp = 0;                        // eax
    *--stackp = 0;                        // ebx
    *--stackp = 0;                        // ecx
    *--stackp = 0;                        // edx
    *--stackp = 0;                        // esi
    *--stackp = 0;                        // edi
    *--stackp = proc->thread_list->stack_limit;// ebp
    *--stackp = 0x10;                     // ds
    *--stackp = 0x10;                     // es
    *--stackp = 0x10;                     // fs
    *--stackp = 0x10;                     // gs
    proc->thread_list->esp_kernel = (uint32_t) stackp;
    
    proc->threads = 1;
    proc->thread_list->state = PROC_ACTIVE;
    proc->state = PROC_ACTIVE;

    sched_add_proc(proc);
    int pid = proc->thread_list->pid;
    spin_unlock(&proc_lock, plf);
    return pid;
}

/**
 * @brief Look up a process's state.
 * @param id The process's main-thread id.
 * @return Its state, or @c PROC_STOPPED if there is no such process — the two
 *         are indistinguishable, which suits the one caller that matters:
 *         @ref main_proc waits for the shell to stop, and the shell having
 *         been reaped already means the same thing to it.
 */
int proc_state(int id) {
    process_t *cur = get_proc_by_id(id);
    if(cur == 0)
        return PROC_STOPPED;
    return cur->state;
}
