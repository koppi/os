/*
 *  smp_asm.S - Local APIC interrupt stubs.
 *
 *  lapic_timer_int    LAPIC timer tick (drives per-CPU preemption instead of the
 *                     PIT). Bumps the local CPU's sched_ticks via a C helper,
 *                     optionally runs schedule(), then EOIs the LAPIC.
 *  ipi_tlb_int        TLB-shootdown IPI: flush the requested TLB entries and EOI.
 *  ipi_resched_int    Reschedule IPI: kick the target CPU's scheduler and EOI.
 *  lapic_spurious_int Spurious interrupt (no EOI needed).
 *  lapic_error_int    LAPIC error interrupt: clear and EOI.
 */

    .extern lapic_base
    .extern lapic_timer_tick
    .extern ipi_tlb_handler
    .extern ipi_resched_tick
    .extern sched_switch_done
    .extern lapic_eoi

    .macro LAPIC_EOI
    push %eax
    movl lapic_base, %eax
    movl $0, 0xB0(%eax)
    pop %eax
    .endm

    .globl lapic_timer_int
    .type lapic_timer_int, @function
lapic_timer_int:
    push %eax
    push %ebx
    push %ecx
    push %edx
    push %esi
    push %edi
    push %ebp
    push %ds
    push %es
    push %fs
    push %gs

    movl %esp, %ebx        /* save stack pointer */

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    push %ebx
    call lapic_timer_tick    /* edx:eax = (cr3-or-0):(resume esp) */
    addl $4, %esp
    movl %eax, %esp          /* switch to the chosen thread's kernel stack */
    testl %edx, %edx         /* edx != 0 -> load the new address space now that */
    jz 1f                    /*   ESP is the new stack (mapped there, not here) */
    movl %edx, %cr3
1:
    /* Off the outgoing thread's stack: only now may another CPU resume it.
     * Scratches eax/ecx/edx, which are restored from the frame below. */
    call sched_switch_done

    LAPIC_EOI

    pop %gs
    pop %fs
    pop %es
    pop %ds
    pop %ebp
    pop %edi
    pop %esi
    pop %edx
    pop %ecx
    pop %ebx
    pop %eax
    iret
    .size lapic_timer_int, .-lapic_timer_int

    .globl ipi_tlb_int
    .type ipi_tlb_int, @function
ipi_tlb_int:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    call ipi_tlb_handler

    LAPIC_EOI

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret
    .size ipi_tlb_int, .-ipi_tlb_int

    .globl ipi_resched_int
    .type ipi_resched_int, @function
ipi_resched_int:
    push %eax
    push %ebx
    push %ecx
    push %edx
    push %esi
    push %edi
    push %ebp
    push %ds
    push %es
    push %fs
    push %gs

    movl %esp, %ebx

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    push %ebx
    call ipi_resched_tick    /* edx:eax = (cr3-or-0):(resume esp) */
    addl $4, %esp
    movl %eax, %esp
    testl %edx, %edx
    jz 1f
    movl %edx, %cr3
1:
    call sched_switch_done   /* see lapic_timer_int */

    LAPIC_EOI

    pop %gs
    pop %fs
    pop %es
    pop %ds
    pop %ebp
    pop %edi
    pop %esi
    pop %edx
    pop %ecx
    pop %ebx
    pop %eax
    iret
    .size ipi_resched_int, .-ipi_resched_int

    .globl lapic_spurious_int
    .type lapic_spurious_int, @function
lapic_spurious_int:
    iret
    .size lapic_spurious_int, .-lapic_spurious_int

    .globl lapic_error_int
    .type lapic_error_int, @function
lapic_error_int:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* The error handler just needs to EOI so the LAPIC resumes normal delivery. */
    LAPIC_EOI

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret
    .size lapic_error_int, .-lapic_error_int
