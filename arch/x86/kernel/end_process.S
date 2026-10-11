/**
 * @file end_process.S
 * @brief Syscall trampoline that terminates the calling process.
 *
 * Reached through the syscall table from @c syscall_disp(); forwards the
 * process's return value to @c end_proc() and never comes back.
 */

    .extern end_proc

/* void end_process(int ret, ...)
 * Called through the syscall table by syscall_disp(); forwards the process
 * return value (its first argument) to end_proc(). Does not return. */
    .globl end_process
    .type end_process, @function
end_process:
    pushl 4(%esp)
    call end_proc
    addl $4, %esp
    ret    /* We won't ever reach this point */
    .size end_process, .-end_process
