
#include <lib/unistd.h>
#include <lib/system_calls.h>



/* Creates a child thread. Unimplemented in the kernel: always returns -1. */
pid_t fork() {
    return (pid_t) syscall_call(3);
}

/* Terminates a thread */
void exit(int code) {
    asm volatile("mov %0, %%ebx" : : "b" (code));
    syscall_call(4);
}

/* Waits until a thread ends */
pid_t wait(int *x) {
    // TODO implement
    x = x;
    return 0;
}

/* Waits until a certain thread ends
pid_t wait(pid_t proc, int *x, int code) {
    return 0;
}*/

/* Gets the thread's pid */
pid_t getpid() {
    return 0;
}

/* Gets the parent's pid */
pid_t getppid() {
    return 0;
}

