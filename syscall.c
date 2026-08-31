/**
 * @file syscall.c
 * @brief System-call table and dispatcher for the `int 0x72` gate.
 *
 * ABI: EAX = call number, EBX/ECX/EDX/ESI/EDI = up to five arguments, return
 * value written back into the caller's saved EAX. The userspace side lives in
 * lib/system_calls.c.
 */
#include <types.h>
#include <syscall.h>
#include <idt.h>
#include <exception.h>
//#include <proc/proc.h>
//#include <proc/thread.h>
#include <printf.h>
#include <keyboard.h>
#include <vfs.h>
#include <heap.h>

/** One past the highest valid call number. */
#define MAX_SYSCALL 11

/** Generic syscall implementation signature. */
typedef uint32_t (*syscall_call_func)(uint32_t, ...);

/** Call number → implementation. NULL entries are unimplemented. */
static void *syscalls[] = {
    &printf,                    // printf   0
    &gets,                      // scanf    1
    NULL,                       // clear    2
    &start_thread,              // fork     3
    &stop_thread,               // exit     4
    &end_process,               // return n 5
    &vfs_file_open_user,        // fopen    6
    &vfs_file_close_user,       // fclose   7
    NULL,                       // PWD      8
    &umalloc_sys,               // malloc   9
    &ufree_sys                  // free     10
};

/**
 * @brief Install the syscall gate on vector 0x72 with DPL 3 so ring 3 can
 *        invoke it.
 */
void syscall_init() {
    install_ir(0x72, 0x80 | 0x0E | 0x60, 0x8, &syscall_handle);
}

/**
 * @brief Dispatch one system call.
 *
 * An out-of-range call number sets EAX to -1. Otherwise the handler is called
 * with (EBX, ECX, EDX, ESI, EDI) and its return value is stored in EAX.
 *
 * @param re Saved register frame from the asm stub.
 */
void syscall_disp(struct regs *re) {
    printf("syscall_disp() eax %u ebx %u ecx %u\n", re->eax, re->ebx, re->ecx);
    if(re->eax >= MAX_SYSCALL) {
        re->eax = -1;
        return;
    }
    syscall_call_func func = syscalls[re->eax];
    re->eax = func(re->ebx, re->ecx, re->edx, re->esi, re->edi);
}
