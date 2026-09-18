/**
 * @file uart_asm.S
 * @brief Interrupt stub for the 16550 UART.
 *
 * Saves the register frame, switches to the kernel data selectors and calls
 * @c uart_handler to drain the receive FIFO into the console ring.
 */

    .extern uart_handler

    .globl uart_int
    .type uart_int, @function
uart_int:
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

    call uart_handler

    movb $0x20, %al
    outb %al, $0xA0
    outb %al, $0x20

    pop %ds
    pop %es
    pop %fs
    pop %gs
    popa
    iret
    .size uart_int, .-uart_int
