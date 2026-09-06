/**
 * @file pit.c
 * @brief 8254 PIT programming and the free-running tick counter.
 *
 * Counter 0 is programmed for a 1 kHz square wave in main.c; its IRQ (vector
 * 32, handled by pit_int in pit_asm.asm) bumps @c pit_ticks and, when
 * @c sched_on is set, calls schedule().
 */
#include <pit.h>
#include <io.h>
#include <idt.h>
#include <printf.h>
#include <percpu.h>

/**
 * Master switch: non-zero once the scheduler is running (set by sched_init).
 * Per-CPU "don't preempt me right now" gating is separate — see @ref sched_state.
 */
uint8_t sched_on = 0;

/**
 * Tick counter, bumped by the timer IRQ. 4-byte aligned so 32-bit reads and
 * writes are atomic (Intel SDM Vol 3A §8.1.1).
 */
uint32_t pit_ticks __attribute__ ((aligned (4)));

/** Free-running millisecond counter (1 kHz tick), never reset. */
uint32_t pit_uptime __attribute__ ((aligned (4)));

extern void pit_int();

/**
 * @brief Enable/disable preemption *on the calling CPU only*.
 *
 * On SMP the old global flag became a per-CPU flag: cross-CPU exclusion is now
 * the subsystem spinlocks' job, and @c sched_state only keeps the local LAPIC
 * tick from switching this CPU away mid-critical-section. It is a plain flag
 * (not a nesting counter) so the unbalanced exit paths in proc.c / thread.c
 * still leave preemption enabled.
 *
 * @param on 0 disables preemption on this CPU, non-zero re-enables it.
 */
void sched_state(int on) {
    this_cpu()->preempt_disable = on ? 0 : 1;
}

/** @return Non-zero if preemption is currently enabled on the calling CPU. */
int get_sched_state() {
    return this_cpu()->preempt_disable == 0;
}

/** @brief Write @p cmd to the PIT command port. */
void pit_send_command(uint8_t cmd) {
    outportb(PIT_REG_COMMAND, cmd);
}

/** @brief Write reload byte @p data to the counter selected by @p counter. */
void pit_send_data(uint16_t data, uint8_t counter) {
    if(counter == PIT_COUNTER_0)
        outportb(PIT_REG_COUNTER0, data);
    else if(counter == PIT_COUNTER_1)
        outportb(PIT_REG_COUNTER1, data);
    else if(counter == PIT_COUNTER_2)
        outportb(PIT_REG_COUNTER2, data);
}

/**
 * @brief Read the current value byte of counter 0, 1 or 2.
 * @param counter Counter selector.
 * @return The byte read, or 0 for an invalid selector.
 */
uint8_t pit_read_data(uint8_t counter) {
    if(counter == PIT_COUNTER_0)
        return inportb(PIT_REG_COUNTER0);
    else if(counter == PIT_COUNTER_1)
        return inportb(PIT_REG_COUNTER1);
    else if(counter == PIT_COUNTER_2)
        return inportb(PIT_REG_COUNTER2);
    else
        return 0;
}

/** @brief Install the timer interrupt handler on vector 32. */
void pit_init() {
    install_ir(32, 0x80 | 0x0E, 0x8, &pit_int);
}

/**
 * @brief Program counter 0 for a periodic interrupt (see @ref pit.h).
 *
 * The reload value is 1193180 / @p frequency (the PIT input clock); a
 * @p frequency of 0 is ignored.
 */
void pit_start_counter(uint32_t frequency, uint8_t counter, uint8_t mode) {
    if(frequency == 0)
        return;

    uint16_t divisor = 1193180 / frequency;

    uint8_t ocw = 0;
    ocw = (ocw & ~PIT_MODE_MASK)    | mode;
    ocw = (ocw & ~PIT_RL_MASK)      | PIT_RL_DATA;
    ocw = (ocw & ~PIT_COUNTER_MASK) | counter;
    pit_send_command(ocw);
    pit_send_data(divisor & 0xFF, PIT_COUNTER_0);
    pit_send_data((divisor >> 8) & 0xFF, PIT_COUNTER_0);

    pit_ticks = 0;
}

/**
 * @brief @return Ticks elapsed on the current CPU since the last
 *        @ref reset_tick_count (driven by the local LAPIC timer).
 */
int get_tick_count() {
    return (int) this_cpu()->sched_ticks;
}

/** @brief Zero the current CPU's tick counter. */
void reset_tick_count() {
    this_cpu()->sched_ticks = 0;
}

/** @brief @return Milliseconds since boot (free-running 1 kHz tick).
 *
 * Advanced by the BSP's LAPIC timer once the scheduler is up (see
 * lapic_timer_tick); before then it stays 0 and callers that need to wait use
 * @ref pit_busywait_ms instead. */
uint32_t pit_ms(void) {
    return pit_uptime;
}

/* PIT input clock: 1193182 Hz. */
#define PIT_INPUT_HZ 1193182u

/**
 * @brief Busy-wait @p ms milliseconds against PIT channel 2 -- no interrupts.
 *
 * Used everywhere a delay is needed before the LAPIC timer is running (AP
 * bring-up, APIC calibration). Channel 2's gate is port 0x61 bit 0 and its
 * output is bit 5, so this works even on a machine whose firmware handed us a
 * masked 8259 with no IRQ 0.
 */
void pit_busywait_ms(uint32_t ms) {
    while (ms) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        uint16_t count = (uint16_t) ((PIT_INPUT_HZ * chunk) / 1000u);

        uint8_t p = inportb(0x61);
        outportb(0x61, (uint8_t) ((p & ~0x02) | 0x01));  /* spkr off, gate on */
        outportb(0x43, 0xB0);                            /* ch2 lo/hi mode 0 */
        outportb(0x42, count & 0xFF);
        outportb(0x42, count >> 8);
        p = inportb(0x61);
        outportb(0x61, (uint8_t) (p & ~0x01));           /* retrigger the gate */
        outportb(0x61, (uint8_t) (p | 0x01));

        uint32_t guard = 0;
        while (!(inportb(0x61) & 0x20))
            if (++guard > 200000000u)                    /* PIT itself is dead */
                return;
        ms -= chunk;
    }
}
