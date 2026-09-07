/*
 *  end_process.S
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
