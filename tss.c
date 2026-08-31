/**
 * @file tss.c
 * @brief Single static Task State Segment used only to supply ESP0/SS0.
 */
#include <io.h>
#include <tss.h>
#include <lib/string.h>
#include <gdt.h>

/** The one TSS. Only esp0 changes at runtime (per scheduled process). */
static tss_t tss;

/** @brief Load the task register with selector 0x2B (GDT slot 5, RPL 3). */
void flush_tss() {
    asm volatile("mov $0x2B, %ax; \
                  ltr %ax;");
}

/**
 * @brief Install the TSS descriptor in the GDT, initialise the TSS and load it.
 *
 * SS0 is the ring-0 data selector; the ring-3 segment fields are set so an
 * @c iret back to userspace has valid selectors.
 */
void install_tss() {
    uint32_t base = (uint32_t) &tss;
    gdt_set_entry(5, base, base + sizeof(tss_t), 0xE9);
    memset((void *) base, 0, sizeof(tss_t));

    tss.esp0 = 0;
    tss.ss0 = 0x10;
    tss.cs = 0x0B;
    tss.ss = 0x13;
    tss.es = 0x13;
    tss.ds = 0x13;
    tss.fs = 0x13;
    tss.gs = 0x13;

    flush_tss();
}

/**
 * @brief Record the kernel stack (@p esp) the CPU switches to on the next
 *        ring-3 → ring-0 transition.
 */
void set_esp0(uint32_t esp) {
    tss.esp0 = esp;
}
