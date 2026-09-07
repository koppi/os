/*
 *  pci_ac97_asm.S
 */

    .extern ac97_irq_handler

    .globl ac97_int
    .type ac97_int, @function
ac97_int:
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

    call ac97_irq_handler

    movb $0x20, %al
    outb %al, $0xA0
    outb %al, $0x20

    pop %ds
    pop %es
    pop %fs
    pop %gs
    popa
    iret
    .size ac97_int, .-ac97_int
