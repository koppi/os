/**
 * @file spinlock.c
 * @brief Test-and-set spinlock implementation with IF save/restore.
 *
 * The hot loop is an xchg on a 1-byte flag with a @c pause in between. The
 * interrupt flag is saved on entry (it is assumed to be enabled) and restored
 * on release, so a locked region keeps its CPU runnable-reentrant but no other
 * CPU-visible data race is possible without interlocks.
 */
#include <spinlock.h>
#include <percpu.h>

/* One lock per subsystem (see spinlock.h). */
spinlock_t pmm_lock   = SPINLOCK_INIT;
spinlock_t pgtbl_lock = SPINLOCK_INIT;
spinlock_t kheap_lock = SPINLOCK_INIT;
spinlock_t vmm_lock   = SPINLOCK_INIT;
spinlock_t sched_lock = SPINLOCK_INIT;
spinlock_t proc_lock  = SPINLOCK_INIT;
spinlock_t fs_lock    = SPINLOCK_INIT;
spinlock_t con_lock   = SPINLOCK_INIT;
spinlock_t tlb_lock   = SPINLOCK_INIT;

/** @brief Read the interrupt flag without touching it. */
static inline uint32_t save_if(void) {
    uint32_t f;
    asm volatile("pushf; pop %0" : "=r"(f));
    return f & 0x200;
}

/** @brief Poke the interrupt flag to @p on (1 = enable, 0 = disable). */
static inline void set_if(int on) {
    if (on)
        asm volatile("sti");
    else
        asm volatile("cli");
}

void spinlock_init(spinlock_t *lock) {
    lock->lock = 0;
#if defined(DEBUG)
    lock->owner = 0xFFFFFFFF;
    lock->depth = 0;
#endif
}

uint32_t spin_lock(spinlock_t *lock) {
    uint32_t flags = save_if();

    /* Acquire with local interrupts *disabled* (so no handler on this CPU can
     * deadlock against a lock its interrupted code holds), but spin with them
     * restored to the caller's state -- otherwise a CPU waiting here could not
     * service the TLB-shootdown IPI of whichever CPU currently holds the lock. */
    for (;;) {
        set_if(0);
        if (__sync_lock_test_and_set(&lock->lock, 1) == 0)
            break;
        if (flags)
            set_if(1);
        while (lock->lock)
            __builtin_ia32_pause();
    }

#if defined(DEBUG)
    lock->owner = this_cpu()->index;
    lock->depth = 1;
#endif
    return flags;
}

uint32_t spin_trylock(spinlock_t *lock) {
    if (__sync_lock_test_and_set(&lock->lock, 1)) {
#if defined(DEBUG)
        uint32_t me = this_cpu()->index;
        if (lock->owner == me && lock->lock) {
            lock->depth++;
            return save_if() & 0x200;
        }
#endif
        return 0;
    }
#if defined(DEBUG)
    lock->owner = this_cpu()->index;
    lock->depth = 1;
#endif
    return save_if() & 0x200;
}

void spin_unlock_raw(spinlock_t *lock) {
#if defined(DEBUG)
    if (lock->depth > 1) {
        lock->depth--;
        return;
    }
    lock->owner = 0xFFFFFFFF;
    lock->depth = 0;
#endif
    __sync_lock_release(&lock->lock);
}

void spin_unlock(spinlock_t *lock, uint32_t flags) {
    spin_unlock_raw(lock);
    set_if(!!(flags & 0x200));
}
