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
    
    mu();

    start_kernel_proc("draw_thread", &refresh_screen);
    //start_kernel_proc("demo_thread", &demo_thread);
    //start_kernel_proc("uart_read", &uart_read_proc);

    //console_exec("help");
    //console_exec("ls");
    //console_exec("cd fda");
    //console_exec("ls");
    //console_exec("start tst");
    //console_exec("start hello");
    //console_exec("ps");
    
    while(1) halt();
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
        app->prec->next = app->next;
        app->next->prec = app->prec;
        n_proc--;
        list = app->next;
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


uint32_t schedule(uint32_t esp) {
    // Check if the time slice ended
    if (get_tick_count() <= list->thread_list->time)
        return esp;

    reset_tick_count();

    // Save the stack pointer
    list->thread_list->esp_kernel = esp;

    // Change thread for the next run
    list->thread_list = list->thread_list->next;

    do {
        // Change process, make sure it's not a finished one
        list = list->next;
    } while(list->state == PROC_STOPPED);
    set_esp0(list->thread_list->stack_kernel_limit);
    change_page_directory(list->pdir);

    return list->thread_list->esp_kernel;
}

int get_nproc() {
    return n_proc;
}

void print_procs() {
    process_t *app = list;
    printf("n_proc = %d\n", n_proc);
    for(int i = 0; i < n_proc; i++) {
        printf("%s id: %d page directory: 0x%x state: %d\n",
               app->name, app->thread_list->pid, (uint32_t)app->pdir, app->state);
        printf("    eip: 0x%x esp: 0x%x stack limit: 0x%x\nimage base: 0x%x image size: %x\n\n",
               app->thread_list->eip, app->thread_list->esp, app->thread_list->stack_limit,
               app->thread_list->image_base, app->thread_list->image_size);
        app = app->next;
    }
}
