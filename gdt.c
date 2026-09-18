/**
 * @file gdt.c
 * @brief Builds and loads a flat Global Descriptor Table.
 */
#include <gdt.h>
#include <tss.h>
#include <percpu.h>

/**
 * Number of GDT slots: 0 null, 1-4 flat code/data rings, then one TSS
 * descriptor per CPU (5..5+MAX_CPU-1), so every processor can ltr to its own
 * TSS and the scheduler can load this single table on every core.
 */
#define GDT_TSS_BASE 5
#define GDT_LEN (GDT_TSS_BASE + MAX_CPU)

struct gdt_info gdt_tab[GDT_LEN];
struct gdt_ptr ptr;

/**
 * @brief Populate the GDT with the flat segment model and load it.
 *
 * Entries: 0 = null, 1 = ring-0 code (0x9A), 2 = ring-0 data (0x92),
 * 3 = ring-3 code (0xFA), 4 = ring-3 data (0xF2). The per-CPU TSS descriptors
 * (slots 5+) are added later by tss_init_cpu().
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

/** @brief Load the shared kernel GDT on an application processor (for APs). */
void gdt_load_ap(void) {
    gdt_set(&ptr);
}

/**
 * @brief Fill @p cpu's TSS descriptor at GDT slot @ref GDT_TSS_BASE + index.
 * @param index Index into @ref cpus[] (0 = BSP).
 * @param base  Linear address of that CPU's TSS.
 * @return The GDT slot holding the descriptor.
 */
int gdt_tss_entry(int index, uint32_t base) {
    int slot = GDT_TSS_BASE + index;
    gdt_set_entry(slot, base, base + sizeof(tss_t), 0xE9);
    return slot;
}

/**
 * @brief Encode a base/limit/access triple into GDT slot @p index
 *        (see @ref gdt.h). The granularity nibble is fixed at 0xC.
 */
void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access) {
    gdt_tab[index].base_low = base & 0xFFFF;
    gdt_tab[index].base_middle = (base >> 16) & 0xFF;
    gdt_tab[index].base_high = (base >> 24) & 0xFF;
    gdt_tab[index].limit_low = limit & 0xFFFF;
    gdt_tab[index].granularity = ((limit >> 16) & 0x0F) | (0xCF & 0xF0);
    gdt_tab[index].flags = access;
}
