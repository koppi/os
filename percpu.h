/**
 * @file percpu.h
 * @brief Per-CPU state: the @c cpus[] array and @ref this_cpu().
 *
 * Every CPU carries its own "current" process/thread, idle thread, page
 * directory, TSS, preemption gate and tick counter. @ref this_cpu() finds the
 * owning struct by reading the local LAPIC id and scanning a small table, which
 * is cheap enough for 4 cores.
 */
#pragma once

#include <types.h>

#ifndef __ASSEMBLER__

#include <tss.h>
#include <proc.h>
#include <paging.h>

/** Maximum number of CPUs this kernel supports. */
#define MAX_CPU 8

/** Maximum concurrent CPUs reported by ACPI. */
#define MAX_ACPI_CPU 8

/** Physical address where the AP trampoline is copied (below the kernel image). */
#define TRAMPOLINE_ADDR 0x8000

/** Atomic "claim a CPU index" counter, one dword, just below the AP info array. */
#define AP_CLAIM_ADDR 0x8FFC

/** Base of the per-CPU AP boot-info blocks (in the low identity-mapped 4 MiB). */
#define APINFO_BASE 0x9000
#define APINFO_SIZE 0x40

/** Values the BSP publishes for each AP before it powers on. */
struct ap_info {
    uint32_t cr3;         /**< +0  Physical address of the kernel page directory. */
    uint32_t stack_top;   /**< +4  Top of this AP's kernel stack (linear). */
    uint32_t ap_main;     /**< +8  Linear address of ap_main(). */
};

/** The low-memory AP boot-info array (Base = @ref APINFO_BASE). */
extern struct ap_info apinfo[MAX_CPU];

/** One CPU's run state. */
typedef struct cpu {
    uint32_t index;            /**< Index into @ref cpus[]. */
    uint32_t apicid;           /**< Local APIC id. */
    int present;               /**< Slot is configured (apicid is valid). */
    volatile int online;       /**< Non-zero once this CPU is running ap_main(). */
    uint32_t sched_ticks;      /**< LAPIC-timer ticks since last reset. */

    thread_t *current;         /**< Thread currently running on this CPU. */
    process_t *current_proc;   /**< Process owning @c current. */
    thread_t *idle;            /**< This CPU's idle thread (never leaves it). */
    page_dir_t *current_dir;   /**< Page directory loaded on this CPU. */
    process_t *prev_proc;      /**< Process switched away from but not yet
                                    released: it stays claimed by this CPU until
                                    @ref sched_switch_done runs. */

    int preempt_disable;       /**< Per-CPU preemption gate (was sched_state). */
    tss_t tss;                 /**< This CPU's TSS (ESP0/SS0 for ring-3 entry). */

    uint32_t stack_top;        /**< Top of this CPU's kernel stack (APs). */
    uint32_t stack_size;       /**< Size of the AP stack. */
} cpu_t;

/** Global per-CPU array. */
extern cpu_t cpus[MAX_CPU];
/** Number of CPUs currently online (set after AP bring-up). */
extern int ncpu;

/** @brief Map a LAPIC id to its @ref cpu_t (falls back to cpus[0]). */
cpu_t *cpu_by_apicid(uint32_t apicid);

/** @brief Simple ABI used by @ref this_cpu. */
uint32_t lapic_self_id(void);

/** GDT slot of cpus[0]'s TSS descriptor (see gdt.c). */
#define PERCPU_GDT_TSS_BASE 5

/**
 * @brief The @ref cpu_t for the CPU we are running on.
 *
 * Uses the task register: each CPU runs @c ltr with its own TSS selector
 * (@c PERCPU_GDT_TSS_BASE + index), so @c str reads back an id that is 100%
 * reliable and needs no memory access. The LAPIC-id path is only a fallback
 * for the sliver of AP bring-up before @c flush_tss_for() — a cacheable LAPIC
 * mapping once let @c lapic_id() return another CPU's id.
 */
static inline cpu_t *this_cpu(void) {
    uint16_t tr;
    __asm__ volatile("str %0" : "=r"(tr));
    int idx = (int) ((tr & 0xFFF8u) >> 3) - PERCPU_GDT_TSS_BASE;
    if (idx >= 0 && idx < MAX_CPU && cpus[idx].present)
        return &cpus[idx];
    return cpu_by_apicid(lapic_self_id());
}

/* --- SMP bring-up and reporting (smp.c). --- */

/** @brief Register the BSP as cpus[0] and set up its idle/current state. */
void smp_register_bsp(uint32_t apicid);

/** @brief Copy the trampoline to 0x8000, INIT-SIPI-SIPI every non-BSP CPU. */
void smp_init(void);

/** @brief AP entry (from ap_boot.asm); never returns. */
void __attribute__((noreturn)) ap_main(void);

/** @brief Release all parked APs (set smp_go, start each core's timer). */
void smp_release(void);

/** @brief Print per-CPU state (the `cpus` command). */
void smp_report(void);

/** @brief Send a cli;hlt IPI to every other CPU (used before poweroff). */
void smp_halt_others(void);

#endif /* __ASSEMBLER__ */
