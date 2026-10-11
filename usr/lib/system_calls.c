#include <lib/string.h>
#include <lib/system_calls.h>

void system(char *arg) {
    if(strcmp(arg, "clear") == 0) {
        syscall_call(2);
    }
}

/*
 * Syscall 8 ("PWD") was never wired up in the kernel's dispatch table --
 * it's a NULL entry, left behind once the real implementation moved to
 * syscall 19 (getcwd(buf, n), used directly by apps/zsh) without this
 * wrapper being updated to match. Calling it landed on a null function
 * pointer -- invisible until fopen()'s own use of pwd() (to turn a
 * relative path into an absolute one) was finally exercised by a caller
 * that actually opens a file the kernel hasn't already touched, which is
 * what surfaced this: Qt's font loader opening the font file to render
 * real glyphs. Route through the real, working getcwd syscall instead.
 */
char *pwd() {
    static char buf[256];
    syscall3(19, (unsigned) buf, sizeof buf, 0);
    return buf;
}

/*
 * Trampoline at RETURN_ADDR: where a thread lands when its entry function
 * returns normally (main() included). Syscall 4 (exit/stop_thread) branches
 * correctly on whether this is the process's main thread (whole-process
 * teardown) or a thread_create()d sibling falling off the end of its
 * function (just that thread unlinked) -- syscall 5 (return n/end_process)
 * does not, it always tears down the whole process, which is exactly wrong
 * for a sibling thread.
 */
void end_process_return() {
    asm volatile("mov %eax, %ebx");
    syscall_call(4);
}

void *syscall_call(int n) {
    void *ret;
    asm volatile("mov %0, %%eax; \
	              int $0x72" : : "a" (n));
    asm volatile("mov %%eax, %0" : "=r" (ret));
    return ret;
}

/*
 * Robust 3-argument syscall: a single asm statement with proper in/out/clobber
 * constraints, so the compiler cannot lose the argument registers between
 * setting them up and the `int` (unlike the split `asm volatile` idiom above).
 */
unsigned syscall3(int n, unsigned a, unsigned b, unsigned c) {
    unsigned ret;
    asm volatile("int $0x72"
                 : "=a"(ret)
                 : "0"(n), "b"(a), "c"(b), "d"(c)
                 : "memory");
    return ret;
}

/* Create/truncate @p path and write @p len bytes of @p buf (syscall 16). */
int write_file(const char *path, const void *buf, unsigned len) {
    return (int) syscall3(16, (unsigned) path, (unsigned) buf, len);
}

/* Start a new thread inside the calling process, sharing its address space,
 * at entry(arg) (syscall 32). Returns the new thread's id, or -1. */
int thread_create(void *entry, void *arg) {
    return (int) syscall3(32, (unsigned) entry, (unsigned) arg, 0);
}

/* Block until thread tid exits; returns immediately if it already has
 * (syscall 33). */
int thread_join(int tid) {
    return (int) syscall3(33, (unsigned) tid, 0, 0);
}

/* Give up the rest of the calling thread's quantum (syscall 34). */
void thread_yield(void) {
    syscall3(34, 0, 0, 0);
}

/* The calling thread's own id (syscall 35). */
int thread_self(void) {
    return (int) syscall3(35, 0, 0, 0);
}
