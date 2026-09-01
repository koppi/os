/**
 * @file smp.c
 * @brief Per-CPU bookkeeping, the LAPIC timer/IPI C helpers and AP bring-up.
 *
 * The BSP registers itself as cpus[0], maps + calibrates the LAPIC, then powers
 * on the application processors one at a time: it copies the trampoline
 * (ap_boot.asm) to 0x8000, publishes a per-CPU boot-info block and raises
 * INIT-SIPI-SIPI. Each AP claims its index, loads the kernel page tables and
 * enters @ref ap_main, where it initialises its APIC/FPU/TSS and then parks on
 * @c smp_go. @ref sched_init builds every CPU's idle thread and calls
 * @ref smp_release, after which the APs enter the scheduler and pull runnable
 * threads off the shared run queue in parallel with the BSP.
 */
#include <percpu.h>
#include <acpi.h>
#include <apic.h>
#include <idt.h>
#include <sched.h>
#include <spinlock.h>
#include <io.h>
#include <fpu.h>
#include <mm.h>
#include <kheap.h>
#include <paging.h>
#include <gdt.h>
#include <tss.h>
#include <lib/string.h>
#include <log.h>

cpu_t cpus[MAX_CPU];
int   ncpu = 1;

/** Boot-info the trampoline reads at APINFO_BASE (see percpu.h). */
struct ap_info apinfo[MAX_CPU];

/** Size of each AP's transient trampoline stack (abandoned once it hits idle). */
#define AP_STACK_SIZE 0x2000

/** Master "is the scheduler running yet" switch (set in sched_init). */
extern uint8_t sched_on;

/** Release gate: 0 keeps the APs parked in ap_main; sched_init sets it to 1. */
volatile int smp_go = 0;

/** AP trampoline binary, embedded via the Makefile (ap_boot_bin.o). */
extern char _binary_ap_boot_bin_start[];
extern char _binary_ap_boot_bin_end[];

uint32_t lapic_self_id(void) {
    return lapic_id();
}

cpu_t *cpu_by_apicid(uint32_t apicid) {
    /* Scan every configured slot, not just cpus[0..ncpu): ncpu tracks how many
     * CPUs are *online* and briefly lags an AP that is still in ap_main, and a
     * miss here would silently alias that AP onto cpus[0] — two CPUs then share
     * one TSS and corrupt each other on the next ring-3 -> ring-0 trap. */
    for (int i = 0; i < MAX_CPU; i++)
        if (cpus[i].present && cpus[i].apicid == apicid)
            return &cpus[i];
    return &cpus[0];
}

/* ------------------------------------------------------------------ *
 *  LAPIC interrupt C helpers (called from smp_asm.asm)                *
 * ------------------------------------------------------------------ */

/**
 * @brief LAPIC timer tick: bump this CPU's tick counter and, unless preemption
 *        is gated, run the scheduler.
 * @param esp Kernel ESP of the interrupted thread.
 * @return Packed resume-ESP + CR3 (see @ref schedule).
 */
uint64_t lapic_timer_tick(uint32_t esp) {
    cpu_t *c = this_cpu();
    c->sched_ticks++;
    if (!sched_on || c->preempt_disable)
        return esp;
    return schedule(esp);
}

/**
 * @brief Reschedule IPI: force this CPU through the scheduler now (used to make
 *        a freshly-woken high-priority thread preempt without waiting a tick).
 * @return Packed resume-ESP + CR3 (see @ref schedule).
 */
uint64_t ipi_resched_tick(uint32_t esp) {
    cpu_t *c = this_cpu();
    if (!sched_on || c->preempt_disable)
        return esp;
    return schedule(esp);
}

void smp_register_bsp(uint32_t apicid) {
    cpu_t *b = &cpus[0];
    /* Do NOT memset here: install_tss() has already populated b->tss (and
     * loaded TR to point at it). Zeroing it would leave the BSP's TSS with a
     * null SS0, and the first ring-3 -> ring-0 trap would #TS -> triple fault.
     * cpus[] starts zeroed in .bss, so only the live fields need setting. */
    b->index = 0;
    b->apicid = apicid;
    b->present = 1;
    b->online = 1;
    b->current_dir = get_kern_directory();
}

/* ------------------------------------------------------------------ *
 *  AP bring-up (BSP side)                                             *
 * ------------------------------------------------------------------ */

/** @brief A short IO-port delay. */
static void io_delay(void) { inportb(0x80); }

/** @brief Busy-wait @p ms using the free-running PIT clock (needs IF set). */
static void delay_ms(int ms) {
    uint32_t start = pit_ms();
    while ((int) (pit_ms() - start) < ms)
        io_delay();
}

/** @brief Copy the trampoline to 0x8000 and publish per-CPU boot info. */
static void prepare_trampoline(void) {
    size_t len = (size_t) (_binary_ap_boot_bin_end - _binary_ap_boot_bin_start);
    if (len > (AP_CLAIM_ADDR - TRAMPOLINE_ADDR))
        return;
    memcpy((void *) TRAMPOLINE_ADDR, _binary_ap_boot_bin_start, len);

    *(volatile uint32_t *) AP_CLAIM_ADDR = 1;   /* first AP claims index 1 */

    uint32_t cr3 = (uint32_t) get_kern_directory();
    for (int i = 1; i < ncpu; i++) {
        void *st = kmalloc(AP_STACK_SIZE);
        cpus[i].stack_top  = st ? (uint32_t) st + AP_STACK_SIZE : 0;
        cpus[i].stack_size = st ? AP_STACK_SIZE : 0;
        apinfo[i].cr3       = cr3;
        apinfo[i].ap_main   = (uint32_t) &ap_main;
        apinfo[i].stack_top = cpus[i].stack_top;
        memcpy((void *) (APINFO_BASE + (size_t) i * APINFO_SIZE),
               &apinfo[i], APINFO_SIZE);
    }
}

/** @brief INIT-SIPI-SIPI the AP with local APIC id @p apicid. */
static void start_ap(uint32_t apicid) {
    lapic_send_init(apicid);
    delay_ms(10);
    lapic_send_sipi(apicid);
    delay_ms(1);
    lapic_send_sipi(apicid);
}

void smp_init(void) {
    int n = acpi_cpu_count();
    if (n < 1) n = 1;
    if (n > MAX_CPU) n = MAX_CPU;

    if (n == 1) {
        klogf(LOG_INFO, "smp: 1/1 CPU online\n");
        return;
    }
    ncpu = n;

    for (int i = 1; i < n; i++) {
        memset(&cpus[i], 0, sizeof(cpu_t));
        cpus[i].index  = i;
        cpus[i].apicid = acpi_cpu_apicid(i);
        cpus[i].present = 1;
        cpus[i].current_dir = get_kern_directory();
        cpus[i].preempt_disable = 1;
    }

    prepare_trampoline();

    /* interrupts on for delay_ms / the AP announcing itself */
    asm volatile("sti");
    for (int i = 1; i < n; i++) {
        klogf(LOG_INFO, "smp: starting AP %d (apic 0x%x)\n", i, cpus[i].apicid);
        start_ap(cpus[i].apicid);

        for (int spin = 0; !cpus[i].online && spin < 200; spin++)
            delay_ms(1);
        if (!cpus[i].online)
            klogf(LOG_WARNING, "smp: AP %d did not come online\n", i);
    }
    asm volatile("cli");

    int on = 0;
    for (int i = 0; i < n; i++)
        on += cpus[i].online ? 1 : 0;
    ncpu = on;
    klogf(LOG_INFO, "smp: %d/%d CPUs online\n", on, n);
}

/**
 * @brief Application-processor entry (tail-called from ap_boot.asm).
 *
 * Runs on the AP's transient trampoline stack with the kernel page tables
 * active. Sets up this CPU's IDT/GDT/TSS/APIC/FPU, parks until @ref smp_release,
 * then hands off into its idle thread (which the scheduler then preempts).
 */
void __attribute__((noreturn)) ap_main(void) {
    cpu_t *c = this_cpu();

    gdt_load_ap();
    idt_load();
    tss_init_cpu(c);
    flush_tss_for(c);
    fpu_init();
    lapic_enable();

    c->current_dir = get_kern_directory();
    c->preempt_disable = 1;
    c->online = 1;

    klogf(LOG_INFO, "smp: cpu%d (apic 0x%x) online\n",
          (unsigned) c->index, (unsigned) c->apicid);

    while (!smp_go)
        __builtin_ia32_pause();

    /* Become the idle thread; the first LAPIC tick will run the scheduler. */
    c->preempt_disable = 0;
    lapic_timer_start();
    sched_run_thread(c->idle);
}

/** @brief Release the parked APs into the scheduler (BSP, from sched_init). */
void smp_release(void) {
    __sync_synchronize();
    smp_go = 1;
    __sync_synchronize();
    if (ncpu > 1)
        klogf(LOG_INFO, "smp: released %d AP(s) into the scheduler\n", ncpu - 1);
}

/** @brief Print per-CPU state (the `cpus` console command). */
void smp_report(void) {
    /* Snapshot under sched_lock so a thread migrating mid-print cannot appear
     * on two CPUs at once in the output. */
    struct { uint8_t on; uint32_t apic, ticks; char name[16]; int pid; int idle; }
        snap[MAX_CPU];
    int me = (int) this_cpu()->index;

    uint32_t f = spin_lock(&sched_lock);
    for (int i = 0; i < MAX_CPU; i++) {
        cpu_t *c = &cpus[i];
        snap[i].on = c->online;
        snap[i].apic = c->apicid;
        snap[i].ticks = c->sched_ticks;
        snap[i].idle = (c->current == c->idle);
        snap[i].pid = c->current ? c->current->pid : 0;
        for (int k = 0; k < 16; k++)
            snap[i].name[k] = c->current_proc ? c->current_proc->name[k] : 0;
        snap[i].name[15] = 0;
    }
    spin_unlock(&sched_lock, f);

    printf("%d CPU(s) online\n", ncpu);
    for (int i = 0; i < MAX_CPU; i++) {
        if (!snap[i].on)
            continue;
        printf("  cpu%d  apic 0x%x  ticks %u  %srunning: %s (pid %d)\n",
               i, (unsigned) snap[i].apic, (unsigned) snap[i].ticks,
               i == me ? "* " : "",
               snap[i].idle ? "idle" : snap[i].name, snap[i].idle ? 0 : snap[i].pid);
    }
}

/** @brief Stop every other CPU (INIT IPI) — best effort, used before poweroff. */
void smp_halt_others(void) {
    int me = (int) this_cpu()->index;
    for (int i = 0; i < MAX_CPU; i++)
        if (cpus[i].online && i != me)
            lapic_send_init(cpus[i].apicid);
}
