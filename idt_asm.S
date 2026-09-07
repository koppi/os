/*
 *  idt_asm.S
 */

    .globl idt_set
    .type idt_set, @function
idt_set:
    movl 4(%esp), %eax
    lidt (%eax)

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ljmp $0x08, $1f
1:
    ret
    .size idt_set, .-idt_set
