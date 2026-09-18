/**
 * @file floppy_asm.S
 * @brief Interrupt stub for the floppy controller (IRQ 6).
 *
 * Bumps @c floppy_irq_done, which floppy.c spins on while waiting for a seek
 * or a DMA transfer to finish, then EOIs the master PIC.
 */

    .extern floppy_irq_done

    .globl floppy_int
    .type floppy_int, @function
floppy_int:
    pusha
    incb floppy_irq_done
    movb $0x20, %al
    outb %al, $0x20
    popa
    iret
    .size floppy_int, .-floppy_int
