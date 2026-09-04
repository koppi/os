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
 * Starts a new process
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
        sched_state(1);
        return PROC_STOPPED;
    }
    
    if(!build_stack(proc->thread_list, proc->pdir, 0)) {
        printf("Failed allocating memory, error 1\n");
        sched_state(1);
        return PROC_STOPPED;
    }
    
    if(!build_heap(proc->thread_list, proc->pdir, 0)) {
        printf("Failed allocating memory, error 2\n");
        sched_state(1);
        return PROC_STOPPED;
    }
    
    uint32_t argc, argv;
    
    if(!heap_fill(proc->thread_list, name, arguments, &argc, &argv)) {
        printf("Failed allocating memory, error 3\n");
        sched_state(1);
        return PROC_STOPPED;
    }
    
    if(!stack_fill(proc->thread_list, argc, argv)) {
        printf("Failed allocating memory, error 4\n");
        sched_state(1);
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
 *Builds the stack for a thread
 */
/**
 * Map @p pages consecutive pages from @p base into both the kernel directory
 * (so start_proc, running on the kernel directory, can seed them) and the
 * target process directory, zeroing each. Frames are then unmapped from the
 * kernel directory once seeding is done (stack_fill / heap_fill).
 */
static int map_user_range(page_dir_t *pdir, vmm_addr_t base, int pages) {
    for(int i = 0; i < pages; i++) {
        vmm_addr_t va = base + (uint32_t) i * PAGE_SIZE;
        if(!vmm_map(get_kern_directory(), va, PAGE_PRESENT | PAGE_RW) ||
           !vmm_map_phys(pdir, va,
                         (uint32_t) get_phys_addr(get_kern_directory(), va),
                         PAGE_PRESENT | PAGE_RW | PAGE_USER))
            return 0;
        memset((void *) va, 0, PAGE_SIZE);
    }
    return 1;
}

int build_stack(thread_t *thread, page_dir_t *pdir, int nthreads) {
    /* Per-thread footprint: user stack + kernel stack + heap, plus slack. Only
     * the main thread (nthreads == 0) is laid out exactly; forked threads are
     * offset by this much so they miss the image and each other. */
    uint32_t span = (uint32_t) nthreads * PAGE_SIZE *
                    (PROC_USER_STACK_PAGES + PROC_KERNEL_STACK_PAGES + PROC_HEAP_PAGES + 8);

    uint32_t ustack_base = thread->image_base + thread->image_size + span;
    if(!map_user_range(pdir, ustack_base, PROC_USER_STACK_PAGES))
        return 0;
    thread->esp = ustack_base;   /* real SP set by stack_fill() */
    thread->stack_limit = ustack_base + PROC_USER_STACK_PAGES * PAGE_SIZE;

    thread->esp_kernel = thread->stack_limit;
    thread->stack_kernel_limit = thread->esp_kernel + PROC_KERNEL_STACK_PAGES * PAGE_SIZE;
    if(!map_user_range(pdir, thread->esp_kernel, PROC_KERNEL_STACK_PAGES))
        return 0;

    return 1;
}

/**
 * Builds the heap for a userspace thread
 */
int build_heap(thread_t *thread, page_dir_t *pdir, int nthreads) {
    uint32_t span = (uint32_t) nthreads * PAGE_SIZE *
                    (PROC_USER_STACK_PAGES + PROC_KERNEL_STACK_PAGES + PROC_HEAP_PAGES + 8);
    vmm_addr_t heap = thread->stack_kernel_limit + span;

    if(!map_user_range(pdir, heap, PROC_HEAP_PAGES))
        return 0;

    thread->heap = heap;
    thread->heap_limit = heap + PROC_HEAP_PAGES * PAGE_SIZE;

    heap_init((vmm_addr_t *) heap, PROC_HEAP_PAGES * PAGE_SIZE);

    return 1;
}

/**
 * Fills the heap with arguments
 */
#define PROC_MAX_ARGV 15

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
 * Fills the stack with register values
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
 * Terminates a process and frees all the memory 
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
 * Removes a terminated process
 */
void remove_proc(int pid) {
    process_t *cur = get_proc_by_id(pid);
    if(cur == 0)
        return;

    uint32_t plf = spin_lock(&proc_lock);

    /* Unlink it from the run queue first, then wait until whichever CPU was
     * running it has switched away (its next LAPIC tick sees state != ACTIVE),
     * so we never free page tables that are still live in some CPU's CR3. */
    if(cur->thread_list && cur->thread_list->main)
        sched_remove_proc(cur->thread_list->pid);
    for(;;) {
        int busy = 0;
        for(int i = 0; i < ncpu; i++)
            if(cpus[i].current_proc == cur)
                busy = 1;
        if(!busy)
            break;
        asm volatile("pause");
    }

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
 * Creates a kernel process from a function
 */
int start_kernel_proc(char *name, void *addr) {
    /* Each kernel process gets its own stack window above KERNEL_SPACE_END;
     * bump the base so a second (third, ...) kernel thread does not land on
     * the previous one's stack. */
    static uint32_t kproc_stack_base = (uint32_t) KERNEL_SPACE_END + 0x5000;

    uint32_t plf = spin_lock(&proc_lock);

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
    proc->thread_list->eip = (uint32_t) addr;

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
 * Returns given id process state
 */
int proc_state(int id) {
    process_t *cur = get_proc_by_id(id);
    if(cur == 0)
        return PROC_STOPPED;
    return cur->state;
}
