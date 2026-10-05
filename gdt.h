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

/** @brief Load the shared kernel GDT and reload segments (for APs). */
void gdt_load_ap(void);

/**
 * @brief Install a CPU's TSS descriptor into the GDT.
 * @param index Index into @ref cpus[] (0 = BSP).
 * @param base  Linear address of that CPU's TSS.
 * @return The GDT slot index holding the descriptor.
 */
int gdt_tss_entry(int index, uint32_t base);

/** Number of per-thread TLS segment descriptors (one GDT slot per user thread that
 *  called `set_thread_area`). */
#define GDT_TLS_SLOTS 512

/** @return The ring-3 selector for TLS slot @p slot (a GDT index from @ref gdt_tls_alloc). */
#define GDT_TLS_SELECTOR(slot) ((uint16_t) (((slot) << 3) | 3))

/**
 * @brief Claim a free TLS segment descriptor and point it at @p base.
 *
 * The descriptor is a flat ring-3 read/write data segment (limit 4 GiB) whose base is the
 * thread's TLS pointer, so `%gs:0` is the thread control block and `%gs:-N` its thread-local
 * variables, exactly as ELF's local-exec model for i386 expects.
 * @return The GDT index (use @ref GDT_TLS_SELECTOR), or 0 if all slots are taken.
 */
int gdt_tls_alloc(uint32_t base);
/** @brief Re-point an allocated TLS slot at a new @p base. */
void gdt_tls_set_base(int slot, uint32_t base);
/** @brief Release a TLS slot (and invalidate its descriptor). Ignores 0. */
void gdt_tls_free(int slot);

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
