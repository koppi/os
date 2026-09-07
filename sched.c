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

process_t *get_cur_proc() {
    return this_cpu()->current_proc;
}

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

void uart_read_proc() {
    char ch[2];

    while(1){
        ch[0] = 0; ch[1] = 0;
        kconsole->read(kconsole, &ch[0], 1);
        printf("%d", ch[0]);
    }
}

void demo_thread() {
    beep_note(0, 0);
    sleep(100);
    beep_off();
    while(1) halt();
}

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

void sched_init() {
    memcpy((void *) RETURN_ADDR, &end_process_return, PAGE_SIZE);

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

    vmm_map(proc->pdir, (vmm_addr_t) KERNEL_SPACE_END, PAGE_PRESENT | PAGE_RW);

    main_thread->esp = (uint32_t) KERNEL_SPACE_END;
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
        out_p->cpu = -1;
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
            if (!proc_runnable(p) || p->cpu >= 0)
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

    thread_t *save_from = out_t ? out_t : c->idle;
    if (nxt_t != save_from) {
        asm volatile("fxsave (%0)" :: "r"(save_from->fpu_state) : "memory");
        asm volatile("fxrstor (%0)" :: "r"(nxt_t->fpu_state) : "memory");
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

void sched_yield(void) {
    if (!sched_on || this_cpu()->preempt_disable)
        return;
    thread_t *cur = this_cpu()->current;
    if (cur)
        cur->yield = 1;
    /* Wait for the next tick to run schedule(); we resume here once repicked. */
    asm volatile("sti; hlt");
}

/** @return The thread with id @p pid anywhere in the ring, or NULL. Caller-locked. */
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

int get_nproc() {
    return n_proc;
}

/** @return A short name for scheduling policy @p policy. */
static const char *policy_name(int policy) {
    switch (policy) {
    case SCHED_FIFO: return "FIFO";
    case SCHED_RR:   return "RR";
    default:         return "OTHER";
    }
}

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
