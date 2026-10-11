/**
 * @file ata_asm.S
 * @brief Interrupt stub for the PIO ATA/IDE driver.
 *
 * Bumps @c ata_irq_done so the polling code in ata.c can tell the interrupt
 * arrived, then EOIs both PICs — the IDE channels sit on the slave.
 */

    .extern ata_irq_done

    .globl ata_int
    .type ata_int, @function
ata_int:
    pusha
    incb ata_irq_done
    movb $0x20, %al
    outb %al, $0xA0
    outb %al, $0x20
    popa
    iret
    .size ata_int, .-ata_int
