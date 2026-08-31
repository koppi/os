/**
 * @file uart.h
 * @brief 16550 UART (COM1) driver — the kernel's serial console and log sink.
 */
#pragma once

#include <chardev.h>
#include <types.h>

/** @brief Configure COM1 (115200 8N1) and enable its FIFO. */
void uart_init(void);
/** @brief Enable the receive-data-available interrupt. */
void uart_rx_ir(void);
/** @return Non-zero when the transmit holding register is empty. */
uint8_t uart_tx_empty(void);
/** @brief Block until the transmitter is ready, then send @p c. */
void uart_putc(char c);
/** @return One received byte, or -1 if none is available. */
int uart_getc(void);
/** @brief UART interrupt handler. */
void uart_handler(void);

/** The chardev wrapper; @c kconsole points here during boot. */
extern chardev_t uartdev;
