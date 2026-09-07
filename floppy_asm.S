/*
 *  floppy_asm.S
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
