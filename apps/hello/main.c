/**
 * @file apps/hello/main.c
 * @brief Smallest useful demo: one `printf` syscall, then exit 0.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Print a greeting through the printf syscall and return. */
int main() {
    printf("Hello from userspace!\n");
    return 0;
}
