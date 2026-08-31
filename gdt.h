/**
 * @file gdt.h
 * @brief Global Descriptor Table setup.
 *
 * A flat 4 GiB model: null, ring-0 code/data, ring-3 code/data, and a TSS
 * descriptor installed later by tss.c.
 */
#pragma once

#include <types.h>

/** One 8-byte GDT entry (see the Intel SDM segment-descriptor layout). */
struct gdt_info {
    uint16_t limit_low;   /**< Limit bits 0-15. */
    uint16_t base_low;    /**< Base bits 0-15. */
    uint8_t base_middle;  /**< Base bits 16-23. */
    uint8_t flags;        /**< Access byte (type, DPL, present). */
    uint8_t granularity;  /**< Limit bits 16-19 plus granularity flags. */
    uint8_t base_high;    /**< Base bits 24-31. */
} __attribute__((__packed__));

/** Operand for the @c lgdt instruction. */
struct gdt_ptr {
    uint16_t limit; /**< Table size in bytes minus one. */
    uint32_t base;  /**< Linear address of the table. */
} __attribute__((__packed__));

/** @brief Build the flat GDT and load it. */
void gdt_init();

/**
 * @brief Fill one GDT entry.
 * @param index  Entry index.
 * @param base   Segment base address.
 * @param limit  Segment limit.
 * @param access Access byte.
 */
void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access);

/** @brief Load @p ptr with @c lgdt and reload the segment registers (asm). */
extern void gdt_set(struct gdt_ptr *ptr);
