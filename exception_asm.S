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
 * @file exception_asm.S
 * @brief Assembly entry points for the CPU exception and syscall vectors.
 *
 * Each stub saves a register frame in the layout @c struct regs / @c regs_error
 * describes, switches to the kernel data selectors and calls the matching C
 * handler in exception.c. The vectors that push an error code (\#GP, \#PF) and
 * those that do not are handled separately, so the frame the C side receives is
 * the same shape either way.
 */

    .extern ex_gpf

    .globl gpf_handle
    .type gpf_handle, @function
gpf_handle:
    pusha
    push %gs
    push %fs
    push %es
    push %ds
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    push %esp
    call ex_gpf
    ret
    .size gpf_handle, .-gpf_handle

    .extern ex_divide_by_zero

    .globl de_handle
    .type de_handle, @function
de_handle:
    pusha
    push %gs
    push %fs
    push %es
    push %ds
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    push %esp
    call ex_divide_by_zero
    ret
    .size de_handle, .-de_handle

    .extern ex_invalid_opcode

    .globl invop_handle
    .type invop_handle, @function
invop_handle:
    pusha
    push %gs
    push %fs
    push %es
    push %ds
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    push %esp
    call ex_invalid_opcode
    ret
    .size invop_handle, .-invop_handle

    .extern ex_page_fault

    .globl pf_handle
    .type pf_handle, @function
pf_handle:
    pusha
    push %gs
    push %fs
    push %es
    push %ds
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    push %esp
    call ex_page_fault
    addl $4, %esp
    iret
    .size pf_handle, .-pf_handle

    .extern syscall_disp

    .globl syscall_handle
    .type syscall_handle, @function
syscall_handle:
    pusha
    push %gs
    push %fs
    push %es
    push %ds
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    push %esp
    call syscall_disp
    addl $4, %esp
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret
    .size syscall_handle, .-syscall_handle
