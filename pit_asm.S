/*
 *  Copyright 2016 Davide Pianca
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @file pit_asm.S
 * @brief Timer interrupt stub (IRQ 0) and the fork return trampoline.
 *
 * @c pit_int saves the register frame and calls into pit.c, which advances the
 * tick count and may run the scheduler. @c fork_eip pops the return address and
 * jumps to it, so a freshly built thread stack returns into ordinary C code the
 * first time it is scheduled.
 */

/* The PIT keeps running for the free-running ms clock (pit_uptime) and the
 * legacy tick counter. Preemption is no longer driven from here: it moved to the
 * per-CPU LAPIC timer (see smp_asm.S / lapic_timer_tick). */

    .extern pit_ticks
    .extern pit_uptime

    .globl pit_int
    .type pit_int, @function
pit_int:

    /* push registers */
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

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    lock incl pit_ticks    /* legacy tick counter (get_tick_count used it) */
    /* pit_uptime is advanced by the BSP LAPIC timer now (see lapic_timer_tick):
     * IRQ 0 may never fire on a machine a UEFI firmware left with a masked 8259. */

    movb $0x20, %al       /* PIC acknowledge (PIT is on master IRQ0) */
    outb %al, $0x20

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
    .size pit_int, .-pit_int

    .globl fork_eip
    .type fork_eip, @function
fork_eip:
    pop %eax
    jmp *%eax
    .size fork_eip, .-fork_eip
