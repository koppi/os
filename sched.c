/**
 * @file sched.c
 * @brief Preemptive fixed-priority round-robin scheduler with real-time
 *        policies, and the first process.
 *
 * @ref sched_init builds process 1 by hand and `iret`s into @ref main_proc,
 * which brings up the block devices, starts the framebuffer redraw thread and
 * runs the console. The timer IRQ calls @ref schedule on every tick while
 * @ref sched_on is set.
 *
 * @ref schedule keeps the process ring but no longer just rotates it: it runs
 * the highest-priority ready thread, round-robins threads that share that
 * priority (each for its @ref thread_t::time quantum), and lets a newly-ready
 * higher-priority thread preempt a running lower-priority one on the next tick.
 * See sched.h for the policy / priority model.
 */
#include <sched.h>

#include <io.h>
#include <kheap.h>
#include <paging.h>
#include <pit.h>
#include <proc.h>
#include <tss.h>
#include <lib/string.h>
#include <lib/system_calls.h>

#include <commands.h>
#include <graphics.h>
#include <kconsole.h>
#include <pcspk.h>
#include <printf.h>
#include <video.h>

#include <floppy.h>
#include <ata.h>
#include <usb.h>
#include <e1000.h>
#include <net.h>

process_t *list;
static int n_proc = 1;

process_t *get_cur_proc() {
    return list;
}

process_t *get_proc_by_id(int id) {
    process_t *app = list;
    for(int i = 0; i < n_proc; i++) {
        thread_t *thr_app = app->thread_list;
	for(int j = 0; j < app->threads; j++) {
            if(thr_app->pid == id) {
                return app;
            }
            thr_app = thr_app->next;
        }
        app = app->next;
    }
    return 0;
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
    floppy_init(); // requires irqs to be enabled
    ata_init();    // probes the IDE channels and mounts hd{a,b,...}

    mu();

    start_kernel_proc("draw_thread", &refresh_screen);
    start_kernel_proc("usb", &usb_thread);
    if (e1000_present())
        start_kernel_proc("net", &net_thread);
    //start_kernel_proc("demo_thread", &demo_thread);
    //start_kernel_proc("uart_read", &uart_read_proc);

    // Hand control to the interactive console (does not return).
    kmain_console();
}

void sched_add_proc(process_t *proc) {
    sched_state(0);
    n_proc++;
    proc->prec = list;
    proc->next = list->next;
    proc->next->prec = proc;
    list->next = proc;
    sched_state(1);
}

void sched_remove_proc(int id) {
    process_t *app = get_proc_by_id(id);
    if(app != 0) {
        sched_state(0);
        app->prec->next = app->next;
        app->next->prec = app->prec;
        n_proc--;
        // Only move the run cursor if it pointed at the process we just
        // unlinked; retarget it to a still-live neighbour, never to an
        // unrelated process (which would corrupt that process's saved state
        // on the next tick).
        if(list == app)
            list = app->prec;
        sched_state(1);
    }
}

void sched_init() {
    memcpy((void *) RETURN_ADDR, &end_process_return, PAGE_SIZE);

    process_t *proc = (process_t *) kmalloc(sizeof(process_t));
    strcpy(proc->name, "console");
    thread_t *main_thread = (thread_t *) kmalloc(sizeof(thread_t));
    proc->thread_list = main_thread;
    proc->threads = 1;
    main_thread->time = 10;
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
    disable_int();
    sched_state(1);
    change_page_directory(proc->pdir);
    set_esp0(main_thread->stack_kernel_limit);
    asm volatile("mov %%eax, %%esp" : : "a" (main_thread->esp_kernel));
    asm volatile("pop %gs;          \
                  pop %fs;          \
                  pop %es;          \
                  pop %ds;          \
                  pop %ebp;         \
                  pop %edi;         \
                  pop %esi;         \
                  pop %eax;         \
                  pop %ebx;         \
                  pop %ecx;         \
                  pop %edx;         \
                  iret");
}


/** @return Non-zero if @p proc's current head thread can run right now. */
static inline int proc_runnable(process_t *proc) {
    return proc->state == PROC_ACTIVE &&
           proc->thread_list->state == PROC_ACTIVE;
}

/**
 * @brief Highest priority among all ready threads (their process head thread).
 * @return A priority in [@c SCHED_PRIO_MIN, @c SCHED_PRIO_MAX], or
 *         @c SCHED_PRIO_MIN-1 when nothing is ready.
 */
static int top_priority(void) {
    int top = SCHED_PRIO_MIN - 1;
    process_t *p = list;
    for (int i = 0; i < n_proc; i++, p = p->next) {
        if (proc_runnable(p) && p->thread_list->priority > top)
            top = p->thread_list->priority;
    }
    return top;
}

/**
 * @brief Pick the next process to run: highest priority wins, and threads that
 *        share the top priority are taken in round-robin order.
 *
 * The scan starts one past the current process so equal-priority peers rotate
 * fairly; the current process is considered last, so a lone top-priority thread
 * simply keeps running.
 *
 * @param top Target priority (from @ref top_priority).
 * @return The chosen process (never NULL; falls back to the current one).
 */
static process_t *pick_next(int top) {
    process_t *p = list;
    for (int i = 0; i < n_proc; i++) {
        p = p->next;
        if (proc_runnable(p) && p->thread_list->priority >= top)
            return p;
    }
    return list;
}

uint32_t schedule(uint32_t esp) {
    if (list == 0)
        return esp;

    thread_t *cur = list->thread_list;

    /* A voluntary yield counts for one scheduling decision only. */
    int yielding = cur->yield;
    cur->yield = 0;

    /* SCHED_FIFO ignores the timer; the others are rotated once their quantum
     * is spent. get_tick_count() counts ticks since the last switch. */
    int quantum_expired = cur->policy != SCHED_FIFO &&
                          cur->time > 0 &&
                          get_tick_count() >= cur->time;

    int top = top_priority();

    /* Keep the current thread when it is still the most eligible one:
     * runnable, not yielding, no higher-priority thread is waiting (real-time
     * preemption) and it still has quantum left. This is the fast path. */
    if (proc_runnable(list) && !yielding &&
        cur->priority >= top && !quantum_expired)
        return esp;

    reset_tick_count();
    cur->esp_kernel = esp;

    /* Round-robin this process's own thread ring, skipping any dead threads.
     * Falls back to the current thread when it is the only one left. */
    thread_t *t = cur;
    do {
        t = t->next;
    } while (t != cur && t->state != PROC_ACTIVE);
    list->thread_list = t;

    /* Move to the next process by priority / round-robin. */
    list = pick_next(top);

    set_esp0(list->thread_list->stack_kernel_limit);
    change_page_directory(list->pdir);

    return list->thread_list->esp_kernel;
}

void sched_yield(void) {
    if (list == 0 || !get_sched_state())
        return;
    list->thread_list->yield = 1;
    /* Wait for the next tick to run schedule(); we resume here once picked. */
    halt();
}

/** @return The thread with id @p pid anywhere in the ring, or NULL. */
static thread_t *thread_by_id(int pid) {
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

    int prev = get_sched_state();
    sched_state(0);
    thread_t *t = thread_by_id(pid);
    if (t != 0)
        t->priority = priority;
    sched_state(prev);
    return t != 0 ? 0 : -1;
}

int sched_set_policy(int pid, int policy) {
    if (policy != SCHED_OTHER && policy != SCHED_RR && policy != SCHED_FIFO)
        return -1;

    int prev = get_sched_state();
    sched_state(0);
    thread_t *t = thread_by_id(pid);
    if (t != 0)
        t->policy = policy;
    sched_state(prev);
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
    process_t *app = list;
    printf("n_proc = %d\n", n_proc);
    for(int i = 0; i < n_proc; i++) {
        thread_t *th = app->thread_list;
        printf("%s id: %d page directory: 0x%x state: %d prio: %d/%s%s\n",
               app->name, th->pid, (uint32_t)app->pdir, app->state,
               th->priority, policy_name(th->policy),
               SCHED_IS_RT(th->priority) ? " [rt]" : "");
        printf("    eip: 0x%x esp: 0x%x stack limit: 0x%x\nimage base: 0x%x image size: %x\n\n",
               th->eip, th->esp, th->stack_limit,
               th->image_base, th->image_size);
        app = app->next;
    }
}
