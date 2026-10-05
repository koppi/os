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
#include <proc.h>
#include <sched.h>
#include <printf.h>
#include <kconsole.h>
#include <keyboard.h>
#include <mouse.h>
#include <csprng.h>
#include <vfs.h>
#include <heap.h>
#include <rtc.h>
#include <pit.h>
#include <commands.h>
#include <video.h>
#include <snd.h>
#include <io.h>

/** One past the highest valid call number. */
#define MAX_SYSCALL 38

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

/**
 * @name Full-screen graphics (#22..#25)
 *
 * The four calls a ring-3 program needs to own the display: grab it, install
 * a palette, push frames, give it back. See @ref video_grab for why a frame
 * crosses the boundary as 8-bpp indexed pixels rather than true colour.
 *
 * A grab also switches the keyboard into raw-scancode mode, because every
 * program that wants the whole screen wants key releases and the arrow keys
 * with it, and pairing the two here means neither can be left on by itself.
 */
///@{

/** @brief `gfx_open` (#22): take the screen for a @p w x @p h indexed surface. */
static uint32_t sys_gfx_open(uint32_t w, uint32_t h) {
    if(!video_grab(w, h))
        return 0;
    keyboard_raw_mode(1);
    mouse_raw_mode(1);
    return 1;
}

/** @brief `gfx_close` (#23): hand the screen back to the desktop. */
static uint32_t sys_gfx_close(void) {
    keyboard_raw_mode(0);
    mouse_raw_mode(0);
    video_ungrab();
    return 0;
}

/** @brief `gfx_palette` (#24): install 256 entries of 0x00RRGGBB. */
static uint32_t sys_gfx_palette(const uint32_t *pal) {
    if(!pal || !video_grabbed())
        return (uint32_t) -1;
    video_set_palette(pal);
    return 0;
}

/** @brief `gfx_blit` (#25): present one indexed frame of the grabbed size. */
static uint32_t sys_gfx_blit(const uint8_t *pix) {
    if(!pix || !video_grabbed())
        return (uint32_t) -1;
    video_blit8(pix);
    return 0;
}
///@}

/**
 * @brief `getscan` (#26): pop one raw key event, or 0 if none are queued.
 *
 * Non-blocking, unlike `getkey` (#17): a game polls it once per frame and
 * must not stall when the player is not typing.
 */
static uint32_t sys_getscan(void) {
    return (uint32_t) keyboard_raw_get();
}

/**
 * @brief `getmouse` (#36): pop one pointer event, or 0 if none are queued.
 *
 * Non-blocking, like `getscan`. Events are recorded only while the program
 * holds the gfx grab. Packed as described in mouse.h: bit 31 valid, bits 24..26
 * buttons, bits 12..23 signed dy (+down), bits 0..11 signed dx.
 */
static uint32_t sys_getmouse(void) {
    return mouse_raw_get();
}

/**
 * @brief `getrandom` (#37): fill a user buffer with random bytes from the kernel CSPRNG.
 *
 * At most 256 bytes per call (what getentropy() allows), and the buffer must lie wholly
 * above the kernel's identity-mapped low memory: the pointer comes straight from ring 3 and
 * the kernel would otherwise write wherever it points. @return 0 on success, -1 on a bad
 * argument. The generator is the one the SSH server uses -- timing-jitter seeded, best effort,
 * not audited cryptographic randomness (see csprng.h).
 */
static uint32_t sys_getrandom(uint32_t buf, uint32_t len) {
    if(len == 0 || len > 256 || buf < KERNEL_SPACE_END || buf + len < buf)
        return (uint32_t) -1;
    csprng_bytes((uint8_t *) buf, (int) len);
    return 0;
}

/**
 * @brief `msleep` (#27): block the caller for @p ms milliseconds.
 *
 * Capped at a second: a frame-paced program never asks for more, and a bad
 * argument should not wedge a process for minutes with no way to interrupt it.
 */
static uint32_t sys_msleep(uint32_t ms) {
    if(ms > 1000)
        ms = 1000;
    sleep((int) ms);
    return 0;
}

/**
 * @name PCM output (#28..#31)
 *
 * The audio counterpart of the graphics grab: a program claims the sound
 * card, writes interleaved stereo frames at the rate `snd_open` reports, and
 * asks how much room is left so it knows how much to render. See snd.h for
 * why the buffer sits here rather than in the driver.
 */
///@{

/** @brief `snd_open` (#28): claim the output. @return its sample rate, or 0. */
static uint32_t sys_snd_open(void) {
    return snd_user_open();
}

/** @brief `snd_close` (#29): release the output. */
static uint32_t sys_snd_close(void) {
    snd_user_close();
    return 0;
}

/**
 * @brief `snd_write` (#30): queue @p nframes stereo frames.
 * @return How many were taken, which is fewer than asked when the ring is
 *         full — the caller paces itself off that (or off `snd_avail`).
 */
static uint32_t sys_snd_write(const int16_t *frames, uint32_t nframes) {
    if(!frames)
        return 0;
    /* A frame is 4 bytes; refuse anything that could not be a real buffer
     * rather than walking megabytes of user memory on a bad argument. */
    if(nframes > (1u << 20))
        return 0;
    return snd_user_write(frames, nframes);
}

/** @brief `snd_avail` (#31): room left in the queue, in frames. */
static uint32_t sys_snd_avail(void) {
    return snd_user_avail();
}
///@}

/**
 * @name Userspace threading (#32..#34)
 *
 * A second (third, ...) thread inside the calling process, sharing its
 * address space -- what `fork` (#3) was meant to be for a child process, but
 * for siblings instead. See create_user_thread() (proc.c) for the frame/stack
 * setup and sched_thread_alive() (sched.c) for how `thread_join` polls.
 */
///@{

/** @brief `thread_create` (#32): start a thread at @p entry(@p arg) inside
 *  the calling process. @return The new thread's id, or -1 on failure. */
static uint32_t sys_thread_create(uint32_t entry, uint32_t arg) {
    process_t *cur = current_user_proc();
    if(!cur)
        return (uint32_t) -1;
    return (uint32_t) create_user_thread(cur, entry, arg);
}

/** @brief `thread_join` (#33): block until thread @p tid exits. Returns
 *  immediately for an already-exited or unknown id. */
static uint32_t sys_thread_join(uint32_t tid) {
    while(sched_thread_alive((int) tid))
        sched_yield();
    return 0;
}

/** @brief `thread_yield` (#34): give up the rest of the calling thread's
 *  quantum. */
static uint32_t sys_thread_yield(void) {
    sched_yield();
    return 0;
}

/** @brief `thread_self` (#35): the calling thread's own id (as opposed to
 *  `getpid`-style process id, which nothing in this kernel implements). */
static uint32_t sys_thread_self(void) {
    process_t *cur = current_user_proc();
    if(!cur)
        return (uint32_t) -1;
    return (uint32_t) cur->thread_list->pid;
}
///@}

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
    (uintptr_t) sys_spawn,           // spawn    21  (load+run a program for ring 3)
    (uintptr_t) sys_gfx_open,        // gfx_open    22  (full-screen grab, video.c)
    (uintptr_t) sys_gfx_close,       // gfx_close   23
    (uintptr_t) sys_gfx_palette,     // gfx_palette 24
    (uintptr_t) sys_gfx_blit,        // gfx_blit    25
    (uintptr_t) sys_getscan,         // getscan     26  (raw scancode, non-blocking)
    (uintptr_t) sys_msleep,          // msleep      27
    (uintptr_t) sys_snd_open,        // snd_open    28  (PCM output, snd.c)
    (uintptr_t) sys_snd_close,       // snd_close   29
    (uintptr_t) sys_snd_write,       // snd_write   30
    (uintptr_t) sys_snd_avail,       // snd_avail   31
    (uintptr_t) sys_thread_create,   // thread_create 32
    (uintptr_t) sys_thread_join,     // thread_join   33
    (uintptr_t) sys_thread_yield,    // thread_yield  34
    (uintptr_t) sys_thread_self,     // thread_self   35
    (uintptr_t) sys_getmouse,        // getmouse      36  (relative motion + buttons, gfx grab only)
    (uintptr_t) sys_getrandom        // getrandom     37  (kernel CSPRNG into a user buffer, <= 256 bytes)
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
