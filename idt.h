#pragma once

#include <types.h>

#define NUM_INTERRUPTS (256)

// See https://wiki.osdev.org/Interrupt_Descriptor_Table#Structure_on_IA-32
struct idt_info {
    uint16_t base_low;  // Lower bits.
    uint16_t sel;       // Code selector.
    uint8_t ist;        // unused, set to 0.
    uint8_t flags;      // gate type, dpl, and p fields.
    uint16_t base_high; // Middle bits.
} __attribute__((__packed__));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((__packed__));

void idt_init(uint16_t code);
void default_ir_handler();
void install_ir(uint32_t i, uint16_t flags, uint16_t sel, void *irq);
extern void idt_set(struct idt_ptr *ptr);
extern void gen_int(uint32_t code);

