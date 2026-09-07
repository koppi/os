/*
 *  ata_asm.S
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
