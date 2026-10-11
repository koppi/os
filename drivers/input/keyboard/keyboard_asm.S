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
 * @file keyboard_asm.S
 * @brief Interrupt stub for the PS/2 keyboard (IRQ 1).
 *
 * Saves the register frame, switches to the kernel data selectors and calls the
 * C handler, which reads the scancode and pushes it into the keystroke ring.
 */

    .extern keyboard_read_key

    .globl keyboard_int
    .type keyboard_int, @function
keyboard_int:
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

    call keyboard_read_key

    movb $0x20, %al
    outb %al, $0x20

    pop %ds
    pop %es
    pop %fs
    pop %gs
    popa
    iret
    .size keyboard_int, .-keyboard_int
