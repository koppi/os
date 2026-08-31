/**
 * @file apic.c
 * @brief Local APIC driver: MMIO access, enable, EOI, IPIs and the LAPIC timer.
 *
 * The LAPIC register block (default 0xFEE00000) is identity-mapped on demand
 * into the kernel directory. KVM's in-kernel irqchip emulates the LAPIC and
 * handles INIT-SIPI-SIPI, so only the MMIO registers above need touching.
 *
 * Timer: the LAPIC timer is calibrated once against the free-running PIT
 * (which stays on the BSP for pit_ms); after that every CPU drives its own
 * preemption tick off its LAPIC timer instead of the PIT.
 */
#include <apic.h>
#include <acpi.h>
#include <paging.h>
#include <mm.h>
#include <io.h>
#include <pit.h>
#include <percpu.h>
#include <lib/string.h>
#include <log.h>

/* --- LAPIC MMIO register offsets (Intel SDM Vol 3A §10.4). --- */
#define LAPIC_ID        0x20
#define LAPIC_SVR       0xF0
#define LAPIC_TPR       0x80
#define LAPIC_EOI       0xB0
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_ERROR 0x370
#define LAPIC_TIMER_INITCNT 0x380
#define LAPIC_TIMER_CURCNT  0x390
#define LAPIC_TIMER_DIV     0x3E0
#define LAPIC_ICR_LOW   0x300
#define LAPIC_ICR_HIGH  0x310
#define LAPIC_ESR       0x280

#define LAPIC_SVR_ENABLE 0x100

/* Timer delivery modes: period = 1 for periodic; vector low 8 bits. */
#define TIMER_PERIODIC   0x20000
#define TIMER_ONESHOOT   0x00000

/* ICR shorthand for "all but self". */
#define ICR_SHORTHAND_ALL      (3 << 18)
#define ICR_DELIVERY_FIXED     (0 << 8)
#define ICR_DELIVERY_INIT      (5 << 8)
#define ICR_DELIVERY_STARTUP   (6 << 8)
#define ICR_LEVEL_ASSERT       (1 << 14)
#define ICR_LEVEL_DEASSERT     (0 << 14)
#define ICR_TRIGGER_EDGE       (0 << 15)
#define ICR_DEST_LOGICAL       (1 << 11)

#define ICR_IDLE_MASK  (1 << 12)

/* Vectors (see idt.c stage-1 install). */
#define VEC_SPURIOUS 0xFF
#define VEC_LAPIC_ERR 0xFE
#define VEC_TLB_IPI   0xFD
#define VEC_RESCHED_IPI 0xFC
#define VEC_LAPIC_TIMER 0xEF

/** Local APIC base (global so the asm EOI stub can use it). */
uint32_t lapic_base = 0xFEE00000;

/** CPU frequency divisor setting for the LAPIC timer. */
#define TIMER_DIVISOR 16

/** Calibration result (ticks per millisecond), shared with lapic_timer_start. */
static uint32_t ticks_per_ms = 0;

uint32_t lapic_read(uint32_t off) {
    return *(volatile uint32_t *) (lapic_base + off);
}

void lapic_write(uint32_t off, uint32_t val) {
    *(volatile uint32_t *) (lapic_base + off) = val;
}

uint32_t lapic_id(void) {
    /* Read the id from the identity-mapped MMIO. */
    uint32_t id = lapic_read(LAPIC_ID);
    return (id >> 24) & 0xFF;
}

/* --- CPU discovery: fill in cpus[] from the ACPI MADT before APs start. --- */
extern int ncpu;

void acpi_init(void);

/* Forward decl from smp.c (implemented there) to register each CPU struct. */
void smp_register_bsp(uint32_t apicid);

void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

void lapic_ipi_to(uint32_t dest, uint8_t vector) {
    lapic_write(LAPIC_ICR_HIGH, dest << 24);
    lapic_write(LAPIC_ICR_LOW, ICR_DELIVERY_FIXED | vector);
}

void lapic_ipi_allbutself(uint8_t vector) {
    lapic_write(LAPIC_ICR_HIGH, 0);
    lapic_write(LAPIC_ICR_LOW, ICR_SHORTHAND_ALL | ICR_TRIGGER_EDGE |
                               ICR_DELIVERY_FIXED | vector);
}

void lapic_send_init(uint32_t dest) {
    lapic_write(LAPIC_ICR_HIGH, dest << 24);
    lapic_write(LAPIC_ICR_LOW, ICR_DELIVERY_INIT | ICR_LEVEL_ASSERT);
    /* INIT must be followed by a 10ms wait (done by the caller). */
}

void lapic_send_sipi(uint32_t dest) {
    lapic_write(LAPIC_ICR_HIGH, dest << 24);
    /* SIPI vector = trampoline >> 12 (0x8000 -> 0x08). */
    lapic_write(LAPIC_ICR_LOW, ICR_DELIVERY_STARTUP |
                               ((TRAMPOLINE_ADDR >> 12) & 0xFF));
}

void lapic_timer_mask(int mask) {
    uint32_t v = lapic_read(LAPIC_LVT_TIMER);
    if (mask)
        v |= 0x10000;               /* Mask bit. */
    else
        v &= ~0x10000;
    lapic_write(LAPIC_LVT_TIMER, v);
}

void lapic_timer_start(void) {
    if (!ticks_per_ms)
        ticks_per_ms = 1;
    lapic_write(LAPIC_TIMER_DIV, 0x3);     /* Divide by 16. */
    lapic_write(LAPIC_LVT_TIMER, TIMER_PERIODIC | VEC_LAPIC_TIMER);
    lapic_write(LAPIC_TIMER_INITCNT, ticks_per_ms);
    lapic_timer_mask(0);
}

void lapic_timer_calibrate(void) {
    /* The PIT ms clock (pit_ms) only advances via its IRQ. During early boot
     * interrupts are off, so briefly enable them for the measurement window:
     * sched_on is 0 here and pit_int no longer calls schedule(), so no thread
     * can be switched away mid-calibration. */
    extern uint8_t sched_on;
    asm volatile("sti");

    /* Divide by 16 for a finer granularity. */
    lapic_write(LAPIC_TIMER_DIV, 0x3);
    lapic_write(LAPIC_LVT_TIMER, TIMER_ONESHOOT | VEC_LAPIC_TIMER);

    /* Count ticks in a known PIT window (~100 ms). */
    uint32_t pit_start = pit_ms();
    /* Give the timer a large initial count. */
    lapic_write(LAPIC_TIMER_INITCNT, 0xFFFFFFFF);
    while (pit_ms() - pit_start < 100) ;   /* Busy-wait ~100 ms. */

    uint32_t count = lapic_read(LAPIC_TIMER_CURCNT);
    uint32_t elapsed = 0xFFFFFFFF - count;
    /* elapsed CPC in ~100 ms, at divide-by-16 -> ticks per ms. */
    ticks_per_ms = elapsed / 100;

    asm volatile("cli");

    klogf(LOG_INFO, "apic: LAPIC timer ~%u ticks/ms\n", (unsigned) ticks_per_ms);

    /* Mask the calibration timer until lapic_timer_start() re-arms it. */
    lapic_timer_mask(1);

    /* sched_on is a global; make sure we leave IF restored for the caller. */
    (void) sched_on;
}

void lapic_enable(void) {
    /* Enable the local APIC via SVR, set the spurious vector. */
    uint32_t svr = lapic_read(LAPIC_SVR);
    svr = (svr & ~0xFF) | VEC_SPURIOUS | LAPIC_SVR_ENABLE;
    lapic_write(LAPIC_SVR, svr);

    /* Clear any pending errors. */
    lapic_write(LAPIC_ESR, 0);
    lapic_read(LAPIC_ESR);
    lapic_write(LAPIC_ESR, 0);
}

void apic_init(void) {
    /* Map the LAPIC MMIO block 1:1 into the kernel directory. */
    for (uint32_t off = 0; off < 0x1000; off += PAGE_SIZE)
        vmm_map_phys(get_kern_directory(), lapic_base + off, lapic_base + off,
                     PAGE_PRESENT | PAGE_RW);

    /* Enable this (BSP) CPU's LAPIC. */
    lapic_enable();

    /* Calibrate the LAPIC timer against the free-running PIT. */
    lapic_timer_calibrate();

    klogf(LOG_INFO, "apic: BSP LAPIC id 0x%x\n", (unsigned) lapic_id());
}
