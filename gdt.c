/**
 * @file gdt.c
 * @brief Builds and loads a flat Global Descriptor Table.
 */
#include <gdt.h>

/** Number of GDT slots (0 null, 1-4 code/data rings, 5 TSS, 6-7 spare). */
#define GDT_LEN 8

struct gdt_info gdt_tab[GDT_LEN];
struct gdt_ptr ptr;

/**
 * @brief Populate the GDT with the flat segment model and load it.
 *
 * Entries: 0 = null, 1 = ring-0 code (0x9A), 2 = ring-0 data (0x92),
 * 3 = ring-3 code (0xFA), 4 = ring-3 data (0xF2). The TSS descriptor (slot 5)
 * is added later by install_tss().
 */
void gdt_init() {
    gdt_set_entry(0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A);
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92);
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA);
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2);

    ptr.base = (uint32_t) &gdt_tab;
    ptr.limit = (sizeof(struct gdt_info) * GDT_LEN) - 1;

    gdt_set(&ptr);
}

/**
 * @brief Encode a base/limit/access triple into GDT slot @p index.
 *
 * The granularity nibble is fixed at 0xC (4 KiB granularity, 32-bit).
 *
 * @param index  GDT slot.
 * @param base   Segment base linear address.
 * @param limit  Segment limit (20 bits significant).
 * @param access Access byte (type/DPL/present).
 */
void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access) {
    gdt_tab[index].base_low = base & 0xFFFF;
    gdt_tab[index].base_middle = (base >> 16) & 0xFF;
    gdt_tab[index].base_high = (base >> 24) & 0xFF;
    gdt_tab[index].limit_low = limit & 0xFFFF;
    gdt_tab[index].granularity = ((limit >> 16) & 0x0F) | (0xCF & 0xF0);
    gdt_tab[index].flags = access;
}
