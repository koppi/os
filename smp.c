/**
 * @file smp.c
 * @brief Per-CPU bookkeeping, the BSP's timer/IPI C helpers and AP bring-up.
 *
 * Stage 1 registers the BSP as cpus[0] and drives preemption off its LAPIC
 * timer. Stage 2 powers on the application processors: the BSP copies the AP
 * trampoline (ap_boot.asm) to 0x8000, publishes a per-CPU boot-info block,
 * and raises INIT-SIPI-SIPI. Each AP claims its 1-based index, loads the
 * kernel page tables, and enters @ref ap_main.
 */
#include <percpu.h>
#include <acpi.h>
#include <apic.h>
#include <idt.h>
#include <sched.h>
#include <io.h>
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

/** Size of each AP's kernel stack. */
#define AP_STACK_SIZE 0x4000

/** Master "is the scheduler running yet" switch (set in sched_init). */
extern uint8_t sched_on;

/** AP trampoline binary, embedded via Makefile (see %make ap_boot_bin.o). */
extern char _binary_ap_boot_bin_start[];
extern char _binary_ap_boot_bin_end[];

uint32_t lapic_self_id(void) {
    return lapic_id();
}

cpu_t *cpu_by_apicid(uint32_t apicid) {
    for (int i = 0; i < ncpu && i < MAX_CPU; i++)
        if (cpus[i].apicid == apicid)
            return &cpus[i];
    return &cpus[0];
}

/**
 * @brief LAPIC timer tick: bump the local CPU's tick counter and, unless
 *        preemption is disabled, run the scheduler.
 * @param esp Kernel ESP of the interrupted thread.
 * @return Kernel ESP to resume on (from schedule, or @p esp unchanged).
 */
uint32_t lapic_timer_tick(uint32_t esp) {
    this_cpu()->sched_ticks++;
    if (!sched_on || this_cpu()->preempt_disable)
        return esp;
    return schedule(esp);
}

/**
 * @brief Reschedule IPI target: if this CPU is running its idle thread, run
 *        the scheduler so a freshly-woken high-priority thread can take over.
 */
void ipi_resched_handler(void) {
    if (sched_on && !this_cpu()->preempt_disable)
        asm volatile("int $0xFC" ::: "memory");
}

/**
 * @brief TLB-shootdown IPI target (Stage 4): flush the shared request and EOI.
 */
void ipi_tlb_handler(void) {
    /* Implemented with the tlb_op machinery in Stage 4 (vmm.c/apic.c). */
}

void smp_register_bsp(uint32_t apicid) {
    cpu_t *b = &cpus[0];
    memset(b, 0, sizeof(cpu_t));
    b->index = 0;
    b->apicid = apicid;
    b->online = 1;
    b->current = 0;
    b->current_proc = 0;
    b->idle = 0;
    b->current_dir = get_kern_directory();
    b->preempt_disable = 0;
    b->sched_ticks = 0;
}

/** @brief A short IO-port delay (used to pace the INIT-SIPI handshake). */
static void io_delay(void) {
    inportb(0x80);
}

/** @brief Busy-wait @p ms using the free-running PIT clock (IF-enabled). */
static void delay_ms(int ms) {
    extern uint32_t pit_ms(void);
    asm volatile("sti");
    uint32_t start = pit_ms();
    while ((int)(pit_ms() - start) < ms)
        io_delay();
    asm volatile("cli");
}

/**
 * @brief Copy the trampoline to 0x8000 and publish per-CPU boot info.
 *
 * Called once after cpus[] are populated. APs read their block (index 1..) at
 * APINFO_BASE to learn the kernel CR3, their own stack and ap_main's address.
 */
static void prepare_trampoline(void) {
    size_t len = (size_t) (_binary_ap_boot_bin_end - _binary_ap_boot_bin_start);
    if (len > (APINFO_BASE - TRAMPOLINE_ADDR))
        return; /* won't fit below APINFO_BASE; bail and stay single-CPU */
    memcpy((void *) TRAMPOLINE_ADDR, _binary_ap_boot_bin_start, len);

    /* The claim counter starts at 1: the first AP to run grabs index 1. */
    *(volatile uint32_t *) AP_CLAIM_ADDR = 1;

    uint32_t cr3 = (uint32_t) get_kern_directory();
    for (int i = 1; i < ncpu && i < MAX_CPU; i++) {
        apinfo[i].cr3 = cr3;
        apinfo[i].ap_main = (uint32_t) &ap_main;
        if (cpus[i].stack_top == 0) {
            void *st = kmalloc(AP_STACK_SIZE);
            cpus[i].stack_top = st ? (uint32_t) st + AP_STACK_SIZE : 0;
            cpus[i].stack_size = st ? AP_STACK_SIZE : 0;
        }
        apinfo[i].stack_top = cpus[i].stack_top;
    }

    /* Publish the blocks down to low memory where the trampoline reads them.
     * The C-side apinfo[] lives in kernel BSS (0x34xxxx), so without this copy
     * the APs would read garbage from APINFO_BASE and triple-fault. */
    for (int i = 0; i < ncpu; i++)
        memcpy((void *) (APINFO_BASE + (size_t)i * APINFO_SIZE), &apinfo[i], APINFO_SIZE);

    /* TEMP DEBUG */
    for (int i = 0; i < ncpu; i++) {
        unsigned long *p = (unsigned long *)(APINFO_BASE + (size_t)i * APINFO_SIZE);
        unsigned long *s = (unsigned long *)&apinfo[i];
        klogf(LOG_INFO, "smp: apinfo[%d] src cr3=%p apmain=%p stack=%p | low cr3=%p apmain=%p stack=%p\n",
              i, (void *)s[0], (void *)s[2], (void *)s[1],
              (void *)p[0], (void *)p[2], (void *)p[1]);
    }
}

/**
 * @brief Raise INIT-SIPI-SIPI for the application processor with @p apicid.
 */
static void start_ap(uint32_t apicid) {
    lapic_send_init(apicid);
    delay_ms(10);                  /* ≥10 ms between INIT and SIPI */

    lapic_send_sipi(apicid);       /* vector 0x08 -> trampoline at 0x8000 */
    delay_ms(1);
    lapic_send_sipi(apicid);
    delay_ms(1);
}

/**
 * @brief Bring up the application processors.
 *
 * Stage 1 / `-smp 1`: ncpu stays 1 and nothing further happens.
 */
void smp_init(void) {
    int n = acpi_cpu_count();
    if (n < 1)
        n = 1;
    if (n == 1) {
        klogf(LOG_INFO, "smp: 1/1 CPU online\n");
        return;
    }
    if (n > MAX_CPU)
        n = MAX_CPU;

    ncpu = n;

    for (int i = 1; i < n; i++) {
        memset(&cpus[i], 0, sizeof(cpu_t));
        cpus[i].index = i;
        cpus[i].apicid = acpi_cpu_apicid(i);
        cpus[i].online = 0;
        cpus[i].current_dir = get_kern_directory();
        cpus[i].preempt_disable = 1;
    }

    prepare_trampoline();

    klogf(LOG_INFO, "smp: INIT-SIPI-SIPI %d AP(s)\n", n - 1);
    for (int i = 1; i < n; i++)
        start_ap(cpus[i].apicid);

    /* Give the APs a moment to announce themselves. */
    delay_ms(50);

    int on = 0;
    for (int i = 0; i < n; i++)
        if (cpus[i].online)
            on++;
    klogf(LOG_INFO, "smp: %d/%d CPU(es) online\n", on, n);
}

/**
 * @brief Application-processor entry (from ap_boot.asm); never returns.
 *
 * Runs on the AP's own kernel stack with the kernel page tables active. It
 * registers the CPU, loads its IDT, installs its own TSS, enables its LAPIC,
 * and parks so the BSP can continue booting.
 */
void __attribute__((noreturn)) ap_main(void) {
    cpu_t *c = this_cpu();
    c->online = 1;

    idt_load();
    gdt_load_ap();
    tss_init_cpu(c);
    flush_tss_for(c);
    lapic_enable();
    c->current_dir = get_kern_directory();
    c->preempt_disable = 1;         /* AP scheduling lands in Stage 3 */

    klogf(LOG_INFO, "smp: cpu%d (apic 0x%x) online\n",
          (unsigned) c->index, (unsigned) c->apicid);

    /* Park: keep interrupts enabled (so IPIs can wake us) but never let
     * anything schedule onto this CPU until the SMP scheduler is in place. */
    for (;;) {
        asm volatile("sti; hlt");
    }
}

/**
 * @brief Release all parked APs (set smp_go, start each core's timer).
 *
 * Stage 2: APs self-release in the trampoline, so this is a confirmation
 * point for the BSP.
 */
void smp_release(void) {
    /* No barrier is needed yet; each AP already claims its index and parks. */
    klogf(LOG_INFO, "smp: release\n");
}

/** @brief Print per-CPU state (the `cpus` command). */
void smp_report(void) {
    for (int i = 0; i < ncpu && i < MAX_CPU; i++) {
        cpu_t *c = &cpus[i];
        printf("cpu%d: apic 0x%x online=%d ticks=%u current=%s\n",
               (int) c->index, (unsigned) c->apicid, c->online,
               (unsigned) c->sched_ticks,
               c->current_proc ? c->current_proc->name : "-");
    }
}

/** @brief Send a cli;hlt IPI to every other CPU (used before poweroff). */
void smp_halt_others(void) {
    /* Stage 4 wires an actual halt-IPI; for now just NMI-free no-op. */
    (void) 0;
}
