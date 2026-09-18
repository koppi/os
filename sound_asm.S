/**
 * @file sound_asm.S
 * @brief Interrupt stub for the Sound Blaster 16.
 *
 * Saves the register frame, switches to the kernel data selectors and calls
 * @c sb16_irq_handler, which acknowledges the DSP and refills the half of the
 * DMA buffer the card has finished playing.
 */

    .extern sb16_irq_handler

    .globl sound_int
    .type sound_int, @function
sound_int:
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

    call sb16_irq_handler

    movb $0x20, %al
    outb %al, $0xA0
    outb %al, $0x20

    pop %ds
    pop %es
    pop %fs
    pop %gs
    popa
    iret
    .size sound_int, .-sound_int
