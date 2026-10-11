/**
 * @file idt_asm.S
 * @brief Loads the IDT and reloads the segment registers.
 *
 * @c idt_set takes a pointer to the IDT descriptor, issues @c lidt, then points
 * every data segment and SS at the kernel data selector and far-jumps to reload
 * CS — a jump is the only way to change CS, and it must happen before the new
 * descriptors are used.
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
