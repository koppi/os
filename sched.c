/**
 * @file sched.c
 * @brief Preemptive fixed-priority round-robin scheduler with real-time
 *        policies, extended for SMP.
 *
 * The process ring (@ref list) is a single global run queue protected by
 * @ref sched_lock. Each CPU tracks its own running thread/process in its
 * @ref cpu_t; a @ref process_t carries a @c cpu field so no process is ever run
 * on two CPUs at once. Every CPU has an idle thread it falls back to when the
 * run queue holds nothing it may run.
 *
 * @ref sched_init builds process 1 (the console) by hand, builds one idle
 * thread per CPU, releases the parked APs (@ref smp_release) and `iret`s into
 * @ref main_proc. Each CPU's LAPIC timer then calls @ref schedule on every tick
 * while @c sched_on is set and its per-CPU preemption gate is open.
 */
#include <sched.h>

#include <io.h>
#include <kheap.h>
#include <ssh.h>
#include <paging.h>
#include <pit.h>
#include <proc.h>
#include <tss.h>
#include <lib/string.h>
#include <lib/system_calls.h>
#include <apic.h>
#include <percpu.h>
#include <spinlock.h>
#include <panic.h>

#include <commands.h>
#include <graphics.h>
#include <kconsole.h>
#include <pcspk.h>
#include <printf.h>
#include <video.h>

#include <floppy.h>
#include <ata.h>
#include <ahci.h>
#include <nvme.h>
#include <initrd.h>
#include <usb.h>
#include <e1000.h>
#include <net.h>
#include <sb16.h>
#include <hda.h>
#include <virtio_gpu.h>

/** Master switch (pit.c): non-zero once the scheduler is live. */
extern uint8_t sched_on;

/** The global run queue: a ring of processes. Protected by @ref sched_lock. */
process_t *list;
static int n_proc = 1;

/** Shared dummy process the per-CPU idle threads hang off. */
static process_t idle_proc;

/**
 * @brief The process this CPU is running.
 * @return The current process; never NULL once @ref sched_init has run.
 *
 * Reads per-CPU state without taking @ref sched_lock, so the answer is only
 * meaningful on the calling CPU and only until it is preempted. A caller that
 * needs it to stay true across a few statements must close the preemption gate
 * first.
 */
process_t *get_cur_proc() {
    return this_cpu()->current_proc;
}

/**
 * @brief Find the process owning the thread with id @p id.
 * @param id A thread id, not a process index — every thread of a process has
 *           its own, and the process's own id is its main thread's.
 * @return The owning process, or NULL.
 *
 * The lock is dropped before returning, so the pointer is only as good as the
 * caller's certainty that nothing is tearing that process down.
 */
process_t *get_proc_by_id(int id) {
    uint32_t f = spin_lock(&sched_lock);
    process_t *found = 0;
    process_t *app = list;
    for (int i = 0; i < n_proc && !found; i++, app = app->next) {
        thread_t *t = app->thread_list;
        for (int j = 0; j < app->threads; j++, t = t->next) {
            if (t->pid == id) { found = app; break; }
        }
    }
    spin_unlock(&sched_lock, f);
    return found;
}

/**
 * @brief Find the process whose address space is @p cr3.
 * @param cr3 A page-directory physical address, as read from CR3.
 * @return The process, or NULL.
 *
 * The kernel directory deliberately returns NULL rather than a process: the
 * fault handler uses that to tell "this trap happened in kernel context" from
 * "this trap belongs to a user process", and so must anything else that calls
 * this from an exception path.
 */
process_t *proc_by_cr3(uint32_t cr3) {
    page_dir_t *kd = get_kern_directory();
    if ((page_dir_t *) cr3 == kd)
        return 0;
    uint32_t f = spin_lock(&sched_lock);
    process_t *found = 0;
    process_t *p = list;
    for (int i = 0; i < n_proc; i++, p = p->next) {
        if ((uint32_t) (uintptr_t) p->pdir == cr3) { found = p; break; }
    }
    spin_unlock(&sched_lock, f);
    return found;
}

/**
 * @brief Debug thread: echo bytes from the serial console as decimal.
 *
 * Not started — the call in @ref main_proc is commented out. Kept because it
 * is the quickest way to prove the UART receive path works on a new machine.
 */
void uart_read_proc() {
    char ch[2];

    while(1){
        ch[0] = 0; ch[1] = 0;
        kconsole->read(kconsole, &ch[0], 1);
        printf("%d", ch[0]);
    }
}

/**
 * @brief Demo thread: one short beep through the PC speaker, then halt forever.
 *
 * Not started; the call in @ref main_proc is commented out.
 */
void demo_thread() {
    beep_note(0, 0);
    sleep(100);
    beep_off();
    while(1) halt();
}

/**
 * @brief The body of process 1, entered by @ref sched_init's final `iret`.
 *
 * Everything that needs a running scheduler and enabled interrupts happens
 * here rather than in main.c: the block drivers probe and mount, the kernel
 * threads start, and control passes to the user-space shell.
 *
 * The threads are started conditionally on the hardware actually being there,
 * so a machine with no NIC or no codec does not carry a thread that has
 * nothing to do. The shell (apps/zsh, staged on the RAM disk) owns line
 * editing and history and reaches the console's commands through the `run`
 * syscall; if its image is missing or it exits, the in-kernel debug console
 * takes over and never returns.
 */
void main_proc() {
    //enable_int();
    ramdisk_init(); // mounts the boot RAM disk from os.iso as "rd"
    floppy_init(); // requires irqs to be enabled
    ata_init();    // probes the legacy IDE channels and mounts hd{a,b,...}
    ahci_init();   // probes SATA ports (the disk path on an X250-era laptop)
    nvme_init();   // probes an NVMe controller (the M.2 SSD on a T470s)

    mu();

    start_kernel_proc("draw_thread", &refresh_screen);
    start_kernel_proc("usb", &usb_thread);
    if (e1000_present())
        start_kernel_proc("net", &net_thread);
    if (hda_present())
        start_kernel_proc("hda-mod", &sound_hda_thread);
    if (virtio_gpu_active())
        start_kernel_proc("virtio-gpu", &virtio_gpu_thread);
    start_kernel_proc("ssh-worker", &ssh_worker_func);
    //start_kernel_proc("demo_thread", &demo_thread);
    //start_kernel_proc("uart_read", &uart_read_proc);

    /*
     * Hand control to the user-space shell (apps/zsh, staged on the "rd" RAM
     * disk that GRUB loaded from os.iso). It owns line editing, history and
     * completion and reaches every command in commands.c through the `run`
     * syscall. Fall back to the in-kernel debug console if the shell image is
     * missing or fails to load, and again once the shell exits (Ctrl-D / `exit`).
     */
    int sh = start_proc("/rd/zsh", "");
    if(sh != PROC_STOPPED) {
        while(proc_state(sh) != PROC_STOPPED) {
            console_spawn_service();   /* run programs the shell asks us to */
            asm volatile("pause");
        }
        remove_proc(sh);
    }

    // Rescue console (does not return).
    kmain_console();
}

/** @brief Nudge the other CPUs to reschedule (a new/boosted thread appeared). */
static void kick_others(void) {
    if (ncpu > 1)
        lapic_ipi_allbutself(0xFC /* IPI_RESCHED */);
}

/**
 * @brief Link a process into the global run queue.
 * @param proc The process; its thread list must already be built.
 *
 * Inserted just after the ring head, left unclaimed by any CPU, and the other
 * CPUs are kicked so a higher-priority arrival is picked up before their next
 * timer tick rather than after it.
 */
void sched_add_proc(process_t *proc) {
    uint32_t f = spin_lock(&sched_lock);
    proc->cpu = -1;
    proc->last_ran = 0;
    n_proc++;
    proc->prec = list;
    proc->next = list->next;
    proc->next->prec = proc;
    list->next = proc;
    spin_unlock(&sched_lock, f);
    kick_others();
}

/**
 * @brief Unlink a process from the run queue.
 * @param id The process's main-thread id.
 *
 * Only unlinks: the process, its threads and its address space are freed by
 * the caller. The ring head is moved if it pointed at the process being
 * removed, which is what keeps the ring walkable afterwards.
 */
void sched_remove_proc(int id) {
    uint32_t f = spin_lock(&sched_lock);
    process_t *app = list;
    int hit = 0;
    for (int i = 0; i < n_proc; i++, app = app->next) {
        if (app->thread_list->pid == id) { hit = 1; break; }
    }
    if (hit) {
        app->prec->next = app->next;
        app->next->prec = app->prec;
        n_proc--;
        /* Keep the ring head valid if it pointed at the unlinked process. */
        if (list == app)
            list = app->prec;
    }
    spin_unlock(&sched_lock, f);
}

/* ------------------------------------------------------------------ *
 *  Idle threads                                                       *
 * ------------------------------------------------------------------ */

/** @brief Per-CPU idle loop: interrupts on, halt until the next tick. */
static void idle_loop(void) {
    for (;;)
        asm volatile("sti; hlt");
}

/** @brief Build an idle thread (own kernel stack, iret frame -> idle_loop). */
static thread_t *make_idle_thread(int cpu_index) {
    thread_t *t = (thread_t *) kmalloc(sizeof(thread_t));
    memset(t, 0, sizeof(*t));
    thread_alloc_fpu_state(t);
    t->pid = -1000 - cpu_index;
    t->state = PROC_ACTIVE;
    t->priority = SCHED_PRIO_MIN;
    t->policy = SCHED_OTHER;
    t->time = 10;
    t->main = 1;
    t->parent = (void *) &idle_proc;
    t->next = t->prec = t;

    /* Idle only ever nests one interrupt (tick -> schedule); 8 KiB is plenty. */
    uint32_t ISIZE = 2 * PAGE_SIZE;
    uint8_t *stk = (uint8_t *) kmalloc(ISIZE);
    uint32_t top = (uint32_t) stk + ISIZE;
    t->stack_kernel_limit = top;
    t->stack_limit = top;

    uint32_t *sp = (uint32_t *) top;
    *--sp = 0x10;                 // ss
    *--sp = top;                  // esp (unused: idle never enters ring 3)
    *--sp = 0x202;                // eflags (IF set)
    *--sp = 0x08;                 // cs
    *--sp = (uint32_t) &idle_loop;// eip
    *--sp = 0;                    // eax
    *--sp = 0;                    // ebx
    *--sp = 0;                    // ecx
    *--sp = 0;                    // edx
    *--sp = 0;                    // esi
    *--sp = 0;                    // edi
    *--sp = top;                  // ebp
    *--sp = 0x10;                 // ds
    *--sp = 0x10;                 // es
    *--sp = 0x10;                 // fs
    *--sp = 0x10;                 // gs
    t->esp_kernel = (uint32_t) sp;
    return t;
}

/**
 * @brief Adopt thread @p t on this CPU and `iret` into it; never returns.
 *
 * Shared by @ref sched_init (the console's first thread) and @ref ap_main (an
 * AP's idle thread). The thread's kernel stack must already hold the 16-word
 * frame @ref schedule leaves behind (built by hand for the first entry).
 */
void __attribute__((noreturn)) sched_run_thread(thread_t *t) {
    cpu_t *c = this_cpu();
    process_t *p = (process_t *) t->parent;

    c->current = t;
    c->current_proc = p;
    if (p != &idle_proc)
        p->cpu = (int) c->index;

    change_page_directory(p->pdir);
    set_esp0(t->stack_kernel_limit);

    asm volatile("mov %0, %%esp" : : "r"(t->esp_kernel));
    asm volatile(
        "pop %gs\n\t pop %fs\n\t pop %es\n\t pop %ds\n\t"
        "pop %ebp\n\t pop %edi\n\t pop %esi\n\t pop %eax\n\t"
        "pop %ebx\n\t pop %ecx\n\t pop %edx\n\t iret\n\t");
    __builtin_unreachable();
}

/**
 * @brief Build the first process, start the other CPUs and enter @ref main_proc.
 *
 * Constructs process 1 by hand — there is no parent to fork from — including a
 * kernel stack with an `iret` frame already laid out on it, then builds one
 * idle thread per CPU, claims the console for the BSP, sets @c sched_on,
 * releases the parked APs and jumps into the frame.
 *
 * Does not return: the last thing it does is `iret` into @ref main_proc.
 * Interrupts are disabled from the point the idle threads are built until the
 * frame's EFLAGS re-enables them, so no CPU can take a timer tick while the
 * run queue is half-built.
 */
void sched_init() {
    memcpy((void *) RETURN_ADDR, (void *) (uintptr_t) end_process_return, PAGE_SIZE);

    process_t *proc = (process_t *) kmalloc(sizeof(process_t));
    strcpy(proc->name, "console");
    thread_t *main_thread = (thread_t *) kmalloc(sizeof(thread_t));
    thread_alloc_fpu_state(main_thread);
    proc->thread_list = main_thread;
    proc->threads = 1;
    proc->cpu = -1;
    proc->last_ran = 0;
    main_thread->time = WEIGHT_BASE;
    main_thread->weight = 1;
    main_thread->priority = SCHED_PRIO_DEFAULT;
    main_thread->policy = SCHED_OTHER;
    main_thread->yield = 0;
    main_thread->next = main_thread;
    main_thread->prec = main_thread;
    main_thread->pid = 1;
    main_thread->main = 1;
    main_thread->state = PROC_ACTIVE;
    main_thread->parent = (void *) proc;
    proc->pdir = get_kern_directory();
    main_thread->eip = (uint32_t) &main_proc;

    /* The console's ring-0 stacks must not sit on KERNEL_SPACE_END (0x800000):
     * every user image is linked at exactly that address (every link.lds in
     * apps/ uses `. = 8M`), so load_elf_relocate() maps and copies the ELF
     * right over the console's live stack pages. Place them below the elf
     * staging window (0x700000) and the image base, above the page-table
     * storage window, in frames pmm_init2() has already reserved
     * (0 .. KERNEL_SPACE_END).
     *
     * Sited at KPROC_STACK_END (mm.h) so start_kernel_proc()'s slots, which
     * grow up towards it, stop before reaching these pages. */
#define CONSOLE_STACK_BASE KPROC_STACK_END
    vmm_map(proc->pdir, (vmm_addr_t) CONSOLE_STACK_BASE, PAGE_PRESENT | PAGE_RW);

    main_thread->esp = (uint32_t) CONSOLE_STACK_BASE;
    main_thread->stack_limit = ((uint32_t) main_thread->esp + PAGE_SIZE);

    main_thread->esp_kernel = main_thread->stack_limit;
    main_thread->stack_kernel_limit = main_thread->esp_kernel + PAGE_SIZE;

    vmm_map(proc->pdir, main_thread->esp_kernel, PAGE_PRESENT | PAGE_RW);
    uint32_t *stackp = (uint32_t *) main_thread->stack_kernel_limit;
    *--stackp = 0x10;                     // ss
    *--stackp = main_thread->esp;         // esp
    *--stackp = 0x202;                    // eflags
    *--stackp = 0x8;                      // cs
    *--stackp = main_thread->eip;         // eip
    *--stackp = 0;                        // eax
    *--stackp = 0;                        // ebx
    *--stackp = 0;                        // ecx
    *--stackp = 0;                        // edx
    *--stackp = 0;                        // esi
    *--stackp = 0;                        // edi
    *--stackp = main_thread->stack_limit; // ebp
    *--stackp = 0x10;                     // ds
    *--stackp = 0x10;                     // es
    *--stackp = 0x10;                     // fs
    *--stackp = 0x10;                     // gs
    main_thread->esp_kernel = (uint32_t) stackp;

    proc->next = proc;
    proc->prec = proc;
    proc->state = PROC_ACTIVE;
    list = proc;
    n_proc = 1;

    idle_proc.state = PROC_ACTIVE;
    idle_proc.pdir = get_kern_directory();
    idle_proc.cpu = -1;
    strcpy(idle_proc.name, "idle");

    disable_int();

    /* One idle thread per online CPU. */
    for (int i = 0; i < ncpu; i++)
        cpus[i].idle = make_idle_thread(i);

    /* The BSP claims the console before the APs are let loose. */
    proc->cpu = 0;
    cpus[0].current = main_thread;
    cpus[0].current_proc = proc;
    cpus[0].current_dir = proc->pdir;
    cpus[0].preempt_disable = 0;

    sched_on = 1;
    smp_release();

    change_page_directory(proc->pdir);
    set_esp0(main_thread->stack_kernel_limit);
    lapic_timer_start();

    sched_run_thread(main_thread);
}


/** @return Non-zero if @p proc's current head thread can run right now. */
static inline int proc_runnable(process_t *proc) {
    return proc->state == PROC_ACTIVE &&
           proc->thread_list->state == PROC_ACTIVE;
}

/** @brief Pack a resume-ESP + a CR3-to-load (0 = keep current) for the stub. */
#define SCHED_RESUME(esp, cr3) (((uint64_t)(uint32_t)(cr3) << 32) | (uint32_t)(esp))

/** How far below its recorded top a thread's kernel ESP may legitimately sit.
 *  Generous: the largest kernel stack here is 8 KiB, so anything past this is
 *  not a deep call chain, it is the wrong stack entirely. */
#define SCHED_KSTACK_SPAN 0x10000u

/**
 * @brief Pick the next thread for this CPU and hand back where to resume it.
 * @param esp The outgoing thread's kernel ESP, as saved by the caller stub.
 * @return A packed (CR3 << 32) | ESP: the stack to switch to, and the address
 *         space to load, or 0 in the high half to keep the current one.
 *
 * Called only from the LAPIC timer and reschedule-IPI stubs in smp_asm.S, with
 * the interrupt frame already pushed on the outgoing thread's kernel stack.
 * Highest priority wins, ties broken by least-recently-run; a process already
 * claimed by another CPU is never picked, and a CPU with nothing to run falls
 * back to its idle thread.
 *
 * Two things are deliberately left for the stub to do after it has switched
 * ESP, and both matter:
 *
 *   - CR3 is returned rather than loaded. This code is still running on the
 *     outgoing thread's kernel stack, which need not be mapped in the incoming
 *     address space; loading CR3 here can pull the stack out from under the
 *     rest of the function.
 *   - The outgoing process stays claimed by this CPU. Releasing it here would
 *     let another CPU resume one of its threads and run down the same kernel
 *     stack whose frames are still being unwound. @ref sched_switch_done
 *     performs the release once ESP has moved.
 *
 * The incoming thread's resume ESP is checked against its own kernel stack
 * before the switch. A thread carrying someone else's stack would otherwise
 * return through whatever happened to be there and surface much later as a
 * jump to a garbage address, so this panics instead, while the thread
 * responsible can still be named.
 */
uint64_t schedule(uint32_t esp) {
    if (list == 0)
        return esp;

    cpu_t *c = this_cpu();
    page_dir_t *entry_dir = c->current_dir;
    uint32_t f = spin_lock(&sched_lock);

    thread_t  *out_t = c->current;
    process_t *out_p = c->current_proc;
    int idle_now = (out_t == c->idle) || (out_p == &idle_proc);

    int yielding = out_t && out_t->yield;
    if (out_t) out_t->yield = 0;

    int quantum_expired = out_t && out_t->policy != SCHED_FIFO &&
                          out_t->time > 0 &&
                          c->sched_ticks >= (uint32_t) out_t->time;

    /* Highest priority among run-queue processes this CPU may pick (runnable
     * and not already running on another CPU), plus the one we are on. */
    int top = SCHED_PRIO_MIN - 1;
    {
        process_t *p = list;
        for (int i = 0; i < n_proc; i++, p = p->next) {
            if (!proc_runnable(p))
                continue;
            if (p->cpu >= 0 && p != out_p)
                continue;
            if (p->thread_list->priority > top)
                top = p->thread_list->priority;
        }
    }

    /* Fast path: keep running the current (real) process. */
    if (!idle_now && proc_runnable(out_p) && !yielding &&
        out_p->thread_list->priority >= top && !quantum_expired) {
        spin_unlock(&sched_lock, f);
        return esp;
    }

    /* Defensive: c->current must be one of c->current_proc's own threads. If
     * they have desynced (a foreign thread pointer) fall back to the process's
     * own current thread, so the context save below never lands in a kernel
     * thread's control block. */
    if (!idle_now && out_t && (process_t *) out_t->parent != out_p)
        out_t = out_p->thread_list;

    /* Save the outgoing context. */
    c->sched_ticks = 0;
    if (out_t)
        out_t->esp_kernel = esp;
    if (!idle_now) {
        /* out_p->cpu deliberately stays claimed here: this CPU is still running
         * on out_t's kernel stack and only leaves it when the stub reloads ESP.
         * Releasing it now would let another CPU resume one of out_p's threads
         * and run down that same stack, over the frames we are still unwinding.
         * sched_switch_done() performs the release once we are off it. */
        thread_t *start = out_t ? out_t : out_p->thread_list;
        thread_t *t = start;
        do {
            t = t->next;
        } while (t != start && t->state != PROC_ACTIVE);
        if ((process_t *) t->parent == out_p)   /* never rotate out of the ring */
            out_p->thread_list = t;
    }

    /* Pick the next process: highest priority, then least-recently-run. */
    process_t *nxt_p = 0;
    {
        process_t *p = list;
        for (int i = 0; i < n_proc; i++, p = p->next) {
            /* out_p is still claimed by this CPU (released after the switch),
             * so admit it here the way the priority scan above does. */
            if (!proc_runnable(p) || (p->cpu >= 0 && p != out_p))
                continue;
            if (p->thread_list->priority < top)
                continue;
            if (!nxt_p || p->last_ran < nxt_p->last_ran)
                nxt_p = p;
        }
    }

    thread_t *nxt_t;
    if (nxt_p) {
        nxt_p->cpu = (int) c->index;
        nxt_p->last_ran = pit_ms();
        nxt_t = nxt_p->thread_list;
        c->current_proc = nxt_p;
        c->current = nxt_t;
    } else {
        nxt_t = c->idle;
        c->current_proc = &idle_proc;
        c->current = c->idle;
    }

    /* Hand the outgoing process to sched_switch_done(). Re-picking it means we
     * never left its stack, so there is nothing to release. */
    c->prev_proc = (!idle_now && nxt_p != out_p) ? out_p : 0;

    thread_t *save_from = out_t ? out_t : c->idle;
    if (nxt_t != save_from) {
        asm volatile("fxsave (%0)" :: "r"(save_from->fpu_state) : "memory");
        asm volatile("fxrstor (%0)" :: "r"(nxt_t->fpu_state) : "memory");
    }

    /* The ESP we are about to resume on must lie inside the incoming thread's
     * own kernel stack. Anything else means its control block is carrying a
     * stack that is not its own, and switching to it drops the thread onto
     * memory it never owned — it then returns through whatever happens to be
     * there, surfacing much later as a jump to a garbage address. Stop here,
     * while the thread responsible is still named. */
    if (nxt_t->esp_kernel > nxt_t->stack_kernel_limit ||
        nxt_t->esp_kernel + SCHED_KSTACK_SPAN < nxt_t->stack_kernel_limit) {
        uint32_t bad = nxt_t->esp_kernel, top = nxt_t->stack_kernel_limit;
        int pid = (int) nxt_t->pid;
        const char *nm = nxt_p ? nxt_p->name : "idle";
        spin_unlock(&sched_lock, f);
        panic("schedule: %s pid %d resume esp %x outside kernel stack (top %x)\n",
              nm, pid, bad, top);
    }

    set_esp0(nxt_t->stack_kernel_limit);

    /* Record the target address space but do NOT load CR3 here: we are still
     * running on the outgoing thread's kernel stack, which may not be mapped in
     * the new directory. The asm stub loads CR3 right after it switches ESP. */
    page_dir_t *nd = nxt_p ? nxt_p->pdir : get_kern_directory();
    c->current_dir = nd;

    spin_unlock(&sched_lock, f);
    return SCHED_RESUME(nxt_t->esp_kernel, nd != entry_dir ? nd : 0);
}

/**
 * @brief Release the process this CPU just switched away from.
 *
 * Called from the context-switch stubs (smp_asm.S) immediately after ESP has
 * moved to the incoming thread's kernel stack. Until it runs, the outgoing
 * process stays claimed by this CPU, so no other CPU can resume one of its
 * threads while we are still unwinding on its stack. Only this CPU can clear
 * the claim, so no other CPU can have taken the process in the meantime.
 */
void sched_switch_done(void) {
    cpu_t *c = this_cpu();
    process_t *prev = c->prev_proc;
    if (!prev)
        return;
    c->prev_proc = 0;
    uint32_t f = spin_lock(&sched_lock);
    prev->cpu = -1;
    spin_unlock(&sched_lock, f);
}

/**
 * @brief Give up the rest of this thread's quantum.
 *
 * Does not switch directly: it flags the thread and halts until the next timer
 * tick runs @ref schedule, so the switch always happens through the one path
 * that knows how to save a context. A no-op when the scheduler is not yet live
 * or this CPU's preemption gate is closed, which is what makes it safe to call
 * from inside a driver's wait loop.
 */
void sched_yield(void) {
    if (!sched_on || this_cpu()->preempt_disable)
        return;
    thread_t *cur = this_cpu()->current;
    if (cur)
        cur->yield = 1;
    /* Wait for the next tick to run schedule(); we resume here once repicked. */
    asm volatile("sti; hlt");
}

/**
 * @brief Find a thread by id anywhere in the run queue.
 * @param pid Thread id.
 * @return The thread, or NULL.
 *
 * The caller must already hold @ref sched_lock; this walks both the process
 * ring and each process's thread ring without taking it.
 */
static thread_t *thread_by_id_locked(int pid) {
    process_t *p = list;
    for (int i = 0; i < n_proc; i++, p = p->next) {
        thread_t *t = p->thread_list;
        for (int j = 0; j < p->threads; j++, t = t->next) {
            if (t->pid == pid)
                return t;
        }
    }
    return 0;
}

/**
 * @brief Set a thread's scheduling priority.
 * @param pid      Thread id.
 * @param priority Clamped to @ref SCHED_PRIO_MIN .. @ref SCHED_PRIO_MAX rather
 *                 than rejected.
 * @return 0 if the thread was found, -1 otherwise.
 *
 * The other CPUs are kicked on success, so a thread raised above what they are
 * running is picked up at once instead of at their next tick.
 */
int sched_set_priority(int pid, int priority) {
    if (priority < SCHED_PRIO_MIN)
        priority = SCHED_PRIO_MIN;
    if (priority > SCHED_PRIO_MAX)
        priority = SCHED_PRIO_MAX;

    uint32_t f = spin_lock(&sched_lock);
    thread_t *t = thread_by_id_locked(pid);
    if (t != 0)
        t->priority = priority;
    spin_unlock(&sched_lock, f);
    if (t != 0)
        kick_others();
    return t != 0 ? 0 : -1;
}

/**
 * @brief Set a thread's scheduling policy.
 * @param pid    Thread id.
 * @param policy @ref SCHED_OTHER, @ref SCHED_RR or @ref SCHED_FIFO.
 * @return 0 if the thread was found, -1 for an unknown policy or no such thread.
 *
 * FIFO is the one with teeth: @ref schedule never treats a FIFO thread's
 * quantum as expired, so it runs until it yields, blocks, or something of
 * higher priority appears.
 */
int sched_set_policy(int pid, int policy) {
    if (policy != SCHED_OTHER && policy != SCHED_RR && policy != SCHED_FIFO)
        return -1;

    uint32_t f = spin_lock(&sched_lock);
    thread_t *t = thread_by_id_locked(pid);
    if (t != 0)
        t->policy = policy;
    spin_unlock(&sched_lock, f);
    return t != 0 ? 0 : -1;
}

/**
 * @brief Set a thread's quantum as a multiple of the base tick count.
 * @param pid    Thread id.
 * @param weight At least 1; larger means a longer turn before preemption.
 * @return 0 if the thread was found, -1 otherwise.
 *
 * Takes effect from the thread's next quantum; the one in progress is not
 * re-timed.
 */
int sched_set_weight(int pid, int weight) {
    if (weight < 1)
        return -1;

    uint32_t f = spin_lock(&sched_lock);
    thread_t *t = thread_by_id_locked(pid);
    if (t != 0) {
        t->weight = weight;
        t->time = WEIGHT_BASE * weight;
    }
    spin_unlock(&sched_lock, f);
    return t != 0 ? 0 : -1;
}

/** @brief How many processes are in the run queue.
 *  @return The count, read without the lock — a snapshot, not a reservation. */
int get_nproc() {
    return n_proc;
}

/** @brief A short display name for a scheduling policy.
 *  @return "FIFO", "RR", or "OTHER" for anything else. */
static const char *policy_name(int policy) {
    switch (policy) {
    case SCHED_FIFO: return "FIFO";
    case SCHED_RR:   return "RR";
    default:         return "OTHER";
    }
}

/**
 * @brief Print every process in the run queue: the console's `ps`.
 *
 * One line per process showing its main thread's id, address space, state,
 * owning CPU, priority and policy, then its register and image layout. Holds
 * @ref sched_lock throughout, so the snapshot is consistent — at the cost of
 * stalling every other CPU's scheduler for as long as the printing takes.
 */
void print_procs() {
    uint32_t f = spin_lock(&sched_lock);
    process_t *app = list;
    printf("n_proc = %d\n", n_proc);
    for(int i = 0; i < n_proc; i++) {
        thread_t *th = app->thread_list;
        printf("%s id: %d page directory: 0x%x state: %d cpu: %d prio: %d/%s%s\n",
               app->name, th->pid, (uint32_t)app->pdir, app->state, app->cpu,
               th->priority, policy_name(th->policy),
               SCHED_IS_RT(th->priority) ? " [rt]" : "");
        printf("    eip: 0x%x esp: 0x%x stack limit: 0x%x\nimage base: 0x%x image size: %x\n\n",
               th->eip, th->esp, th->stack_limit,
               th->image_base, th->image_size);
        app = app->next;
    }
    spin_unlock(&sched_lock, f);
}
