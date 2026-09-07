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
#include <commands.h>

/** One past the highest valid call number. */
#define MAX_SYSCALL 22

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

/**
 * @brief `run` syscall (#18): execute one command line through the same
 *        dispatcher the in-kernel debug console uses (@ref console_exec).
 *
 * This is how the userspace shell (apps/zsh) reuses every command in
 * commands.c — `ls`, `pci`, `ping`, `start <prog>`, `poweroff`, ... — while
 * itself running in ring 3.
 */
static uint32_t sys_run(const char *line) {
    if(line)
        console_exec((char *) line);
    return 0;
}

/**
 * @brief `getcwd` syscall (#19): copy the console working directory (the path
 *        `cd` maintains) into @p buf. @return the string length.
 */
static uint32_t sys_getcwd(char *buf, uint32_t n) {
    if(!buf || !n)
        return 0;
    const char *d = console_cwd();
    uint32_t i = 0;
    for(; d[i] && i + 1 < n; i++)
        buf[i] = d[i];
    buf[i] = 0;
    return i;
}

/**
 * @brief `listdir` syscall (#20): write the newline-separated leaf names of
 *        directory @p path into @p buf. @return the entry count.
 */
static uint32_t sys_listdir(const char *path, char *buf, uint32_t n) {
    return (uint32_t) vfs_listdir((char *) path, buf, n);
}

/**
 * @brief `spawn` syscall (#21): load and run the program at @p path with
 *        argument string @p args, blocking until it exits. @return its status.
 *
 * Unlike `run("start ...")`, the ELF load is marshalled onto the init thread
 * (@ref console_spawn_request), which runs on the kernel page directory — the
 * loader stages the image at a fixed kernel address that a ring-3 process's
 * directory does not map.
 */
static uint32_t sys_spawn(const char *path, const char *args) {
    return (uint32_t) console_spawn_request(path ? path : "", args ? args : "");
}

/** Call number → implementation. NULL entries are unimplemented. */
static uintptr_t syscalls[] = {
    (uintptr_t) printf,              // printf   0
    (uintptr_t) gets,                // scanf    1
    (uintptr_t) NULL,                // clear    2
    (uintptr_t) start_thread,        // fork     3
    (uintptr_t) stop_thread,         // exit     4
    (uintptr_t) end_process,         // return n 5
    (uintptr_t) vfs_file_open_user,  // fopen    6
    (uintptr_t) vfs_file_close_user, // fclose   7
    (uintptr_t) NULL,                // PWD      8
    (uintptr_t) umalloc_sys,         // malloc   9
    (uintptr_t) ufree_sys,           // free     10
    (uintptr_t) urealloc_sys,        // realloc  11
    (uintptr_t) sys_write,           // write    12
    (uintptr_t) sys_fread,           // fread    13
    (uintptr_t) sys_time,            // time     14
    (uintptr_t) sys_clock,           // clock    15
    (uintptr_t) sys_spit,            // spit     16
    (uintptr_t) keyboard_getkey,     // getkey   17  (blocking, unechoed keystroke)
    (uintptr_t) sys_run,             // run      18  (console_exec on behalf of ring 3)
    (uintptr_t) sys_getcwd,          // getcwd   19
    (uintptr_t) sys_listdir,         // listdir  20
    (uintptr_t) sys_spawn            // spawn    21  (load+run a program for ring 3)
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
    syscall_call_func func = (syscall_call_func) syscalls[re->eax];
    re->eax = func(re->ebx, re->ecx, re->edx, re->esi, re->edi);
}
