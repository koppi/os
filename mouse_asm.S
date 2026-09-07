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

    .extern mouse_handler

    .globl mouse_int
    .type mouse_int, @function
mouse_int:
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

    call mouse_handler

    movb $0x20, %al
    outb %al, $0xA0
    outb %al, $0x20

    pop %ds
    pop %es
    pop %fs
    pop %gs
    popa
    iret
    .size mouse_int, .-mouse_int
