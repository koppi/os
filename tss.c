/**
 * @file tss.c
 * @brief Per-CPU Task State Segments, each supplying ESP0/SS0 so a ring-3 →
 *        ring-0 transition lands on the right kernel stack.
 *
 * Each @c cpu_t carries its own TSS (see @ref percpu.h). A TSS descriptor for
 * a given CPU lives at GDT slot 5 + @c index, and @c set_esp0 writes the
 * current CPU's ESP0.
 */
#include <io.h>
#include <tss.h>
#include <lib/string.h>
#include <gdt.h>
#include <percpu.h>

/**
 * @brief Initialise one CPU's TSS and install its GDT descriptor.
 *
 * SS0 is the ring-0 data selector; the ring-3 segment fields are set so an
 * @c iret back to userspace has valid selectors. The BSP is set up by
 * @ref install_tss and APs call this from @ref ap_main.
 */
void tss_init_cpu(cpu_t *c) {
    tss_t *t = &c->tss;
    memset(t, 0, sizeof(tss_t));

    t->esp0 = 0;
    t->ss0 = 0x10;
    t->cs = 0x0B;
    t->ss = 0x13;
    t->es = 0x13;
    t->ds = 0x13;
    t->fs = 0x13;
    t->gs = 0x13;

    gdt_tss_entry(c->index, (uint32_t) t);
}

/** @brief Load the task register for @p cpu (with its own selector). */
void flush_tss_for(cpu_t *c) {
    uint16_t sel = (uint16_t) (((5 + c->index) * 8) + 3);
    asm volatile("mov %0, %%ax; ltr %%ax" :: "r"(sel) : "eax");
}

/** @brief Load the BSP's task register (selector 0x2B, GDT slot 5, RPL 3). */
void flush_tss() {
    asm volatile("mov %0, %%ax; ltr %%ax" :: "r"((uint16_t) 0x2B) : "eax");
}

/**
 * @brief Initialise the BSP's TSS descriptor and load its task register.
 *
 * Call once during boot, after the GDT exists. On `-smp 1` this is the only
 * TSS the kernel ever needs.
 */
void install_tss() {
    tss_init_cpu(&cpus[0]);
    flush_tss();
}

/**
 * @brief Record the kernel stack (@p esp) the *current* CPU switches to on the
 *        next ring-3 → ring-0 transition.
 */
void set_esp0(uint32_t esp) {
    this_cpu()->tss.esp0 = esp;
}
