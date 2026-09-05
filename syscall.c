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
#include <kconsole.h>
#include <keyboard.h>
#include <vfs.h>
#include <heap.h>
#include <rtc.h>
#include <pit.h>

/** One past the highest valid call number. */
#define MAX_SYSCALL 17

/** Set to 1 to log every syscall on the console (default 0: off). */
#define SYSCALL_TRACE 0

/** Generic syscall implementation signature. */
typedef uint32_t (*syscall_call_func)(uint32_t, ...);

/**
 * @brief `write` syscall (#12): emit @p len bytes of @p buf to the console.
 *
 * Unlike the `printf` syscall this takes an explicit length (no NUL scan, no
 * format interpretation), which is what a real stdio `fwrite` needs.
 */
static uint32_t sys_write(const char *buf, uint32_t len) {
    for(uint32_t i = 0; i < len; i++)
        putchar_(buf[i]);
    return len;
}

/**
 * @brief `fread` syscall (#13): read the next 512-byte block of @p f into
 *        @p buf (which must be at least 512 bytes) and report EOF via @c f->eof.
 *
 * @return The block size (512); the caller trims the final block using
 *         @c f->len.
 */
static uint32_t sys_fread(file *f, char *buf) {
    if(!f || !buf)
        return 0;
    vfs_file_read(f, buf);
    return 512;
}

/** @brief `time` syscall (#14): seconds since the Unix epoch (from the RTC). */
static uint32_t sys_time(void) {
    return rtc_now_unix();
}

/** @brief `clock` syscall (#15): milliseconds of uptime (from the PIT). */
static uint32_t sys_clock(void) {
    return pit_ms();
}

/**
 * @brief `spit` syscall (#16): create/truncate the file at @p path and write
 *        @p len bytes of @p buf to it.
 *
 * A one-shot whole-file write — the userspace side (@c write_file) hands the
 * complete output buffer over in a single call, which is exactly what a
 * program that generates a file (a compiler, say) needs and keeps the
 * kernel side simple. @return bytes written, or -1.
 */
static uint32_t sys_spit(const char *path, const char *buf, uint32_t len) {
    if(len > (8u * 1024u * 1024u))
        return (uint32_t) -1;
    return (uint32_t) vfs_spit((char *) path, (char *) buf, len);
}

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
    &ufree_sys,                 // free     10
    &urealloc_sys,              // realloc  11
    &sys_write,                 // write    12
    &sys_fread,                 // fread    13
    &sys_time,                  // time     14
    &sys_clock,                 // clock    15
    &sys_spit                   // spit     16
};

/**
 * @brief Install the syscall gate on vector 0x72 with DPL 3 so ring 3 can
 *        invoke it.
 *
 * A 32-bit *trap* gate (type 0xF), not an interrupt gate: it leaves EFLAGS.IF
 * set, so a system call is preemptible and — importantly on SMP — a CPU that
 * blocks on a subsystem spinlock inside a syscall keeps servicing the
 * TLB-shootdown IPI of whichever CPU holds that lock. Critical sections that
 * must not be preempted disable it locally with @ref sched_state.
 */
void syscall_init() {
    install_ir(0x72, 0x80 | 0x0F | 0x60, 0x8, &syscall_handle);
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
#if SYSCALL_TRACE
    printf("syscall_disp() eax %u ebx %u ecx %u\n", re->eax, re->ebx, re->ecx);
#endif
    if(re->eax >= MAX_SYSCALL) {
        re->eax = -1;
        return;
    }
    syscall_call_func func = syscalls[re->eax];
    re->eax = func(re->ebx, re->ecx, re->edx, re->esi, re->edi);
}
