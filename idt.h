/**
 * @file idt.h
 * @brief Interrupt Descriptor Table setup and gate installation.
 */
#pragma once

#include <types.h>

/** Number of IDT entries (full 0-255 vector space). */
#define NUM_INTERRUPTS (256)

/**
 * @brief One IA-32 interrupt gate.
 * @see https://wiki.osdev.org/Interrupt_Descriptor_Table#Structure_on_IA-32
 */
struct idt_info {
    uint16_t base_low;  /**< Handler address bits 0-15. */
    uint16_t sel;       /**< Code segment selector. */
    uint8_t ist;        /**< Unused on IA-32, kept 0. */
    uint8_t flags;      /**< Gate type, DPL and present bit. */
    uint16_t base_high; /**< Handler address bits 16-31. */
} __attribute__((__packed__));

/** Operand for the @c lidt instruction. */
struct idt_ptr {
    uint16_t limit; /**< Table size in bytes minus one. */
    uint32_t base;  /**< Linear address of the table. */
} __attribute__((__packed__));

/**
 * @brief Fill the IDT with the CPU-exception and default handlers and load it.
 * @param code Kernel code selector to use for every gate.
 */
void idt_init(uint16_t code);

/** @brief Fallback handler for any vector without a specific one. */
void default_ir_handler();

/**
 * @brief Install one interrupt gate and unmask the matching PIC line.
 * @param i     Vector number.
 * @param flags Gate flags byte (type/DPL/present).
 * @param sel   Code selector.
 * @param irq   Handler entry point.
 */
void install_ir(uint32_t i, uint16_t flags, uint16_t sel, void *irq);

/** @brief Load @p ptr with @c lidt (asm). */
extern void idt_set(struct idt_ptr *ptr);

/** @brief Raise software interrupt @p code (asm helper). */
extern void gen_int(uint32_t code);
