/**
 * @file spinlock.h
 * @brief Test-and-set ticketed spinlocks that disable local interrupts while
 *        held, plus the per-subsystem lock instances.
 *
 * These replace the "only one CPU exists" assumption. Every lock must be taken
 * with interrupts disabled locally (so the owning CPU is never switched away
 * mid-critical-section) and released with the previous interrupt state intact.
 * The asm helpers poke the IF flag directly -- they deliberately do NOT use
 * enable_int()/disable_int(), which log on every call.
 */
#pragma once

#include <types.h>

/** A single-word test-and-set spinlock. */
typedef struct spinlock {
    volatile uint32_t lock;   /**< 1 = held, 0 = free. */
#if defined(DEBUG)
    volatile uint32_t owner;  /**< CPU index that holds the lock (debug). */
    volatile uint32_t depth;  /**< Re-entrancy depth on the owner CPU. */
#endif
} spinlock_t;

/** @brief Statically initialise a spinlock to the unlocked state. */
#if defined(DEBUG)
#define SPINLOCK_INIT { 0, 0xFFFFFFFF, 0 }
#else
#define SPINLOCK_INIT { 0 }
#endif

/** @brief Acquire @p lock, saving the interrupt flag; returns the saved IF. */
uint32_t spin_lock(spinlock_t *lock);
/** @brief Release @p lock and restore the interrupt flag @p flags. */
void spin_unlock(spinlock_t *lock, uint32_t flags);
/** @brief Try once to acquire @p lock (no wait). @return old IF, or 0 if busy. */
uint32_t spin_trylock(spinlock_t *lock);
/** @brief Release @p lock without touching the interrupt flag. */
void spin_unlock_raw(spinlock_t *lock);

/** @brief Initialise a runtime-constructed lock. */
void spinlock_init(spinlock_t *lock);

/*
 * One lock per subsystem. Declared here so every user shares the same
 * instance; defined in spinlock.c.
 */
extern spinlock_t pmm_lock;     /**< Physical frame allocator (mm.c). */
extern spinlock_t pgtbl_lock;   /**< Page-table storage allocator (paging.c). */
extern spinlock_t kheap_lock;   /**< Kernel heap (kheap.c). */
extern spinlock_t vmm_lock;     /**< Virtual memory maps (vmm.c). */
extern spinlock_t sched_lock;   /**< Scheduler process/thread ring (sched.c). */
extern spinlock_t proc_lock;    /**< Process lifecycle: start/kernel/remove (proc.c). */
extern spinlock_t fs_lock;      /**< Filesystem / block drivers (vfs.c, fat.c). */
extern spinlock_t con_lock;     /**< Console output path (printf/uart/video). */
extern spinlock_t tlb_lock;     /**< TLB-shootdown operation state (vmm.c/apic.c). */
extern spinlock_t uheap_lock;   /**< User-process heap free lists (heap.c, vfs.c). */

/*
 * Lock order (acquire outer first):
 *   proc_lock  >  vmm_lock  >  { pmm_lock, pgtbl_lock, tlb_lock }
 *   proc_lock  >  kheap_lock
 *   proc_lock  >  sched_lock
 * sched_lock, kheap_lock, con_lock and fs_lock are otherwise leaves.
 * tlb_lock is only ever taken under vmm_lock or on its own (from flush_tlb).
 */
