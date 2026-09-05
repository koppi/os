#include <lib/string.h>
#include <lib/system_calls.h>

void system(char *arg) {
    if(strcmp(arg, "clear") == 0) {
        syscall_call(2);
    }
}

char *pwd() {
    return (char *) syscall_call(8);
}

void end_process_return() {
    asm volatile("mov %eax, %ebx");
    syscall_call(5);
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
