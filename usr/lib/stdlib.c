

#include <lib/system_calls.h>
#include <lib/stdlib.h>

void *malloc(size_t len) {
    asm volatile("mov %0, %%ebx" : : "b" (len));
    return syscall_call(9);
}

void free(void *ptr) {
    asm volatile("lea (%0), %%ebx" : : "b" (ptr));
    syscall_call(10);
}

void *realloc(void *ptr, size_t nsize) {
    asm volatile("lea (%0), %%ebx" : : "b" (ptr));
    asm volatile("mov %0, %%ecx" : : "c" (nsize));
    return syscall_call(11);
}
