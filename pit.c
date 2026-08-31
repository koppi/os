/**
 * @file pit.c
 * @brief 8254 PIT programming and the free-running tick counter.
 *
 * Counter 0 is programmed for a 1 kHz square wave in main.c; its IRQ (vector
 * 32, handled by pit_int in pit_asm.asm) bumps @ref pit_ticks and, when
 * @ref sched_on is set, calls schedule().
 */
#include <pit.h>
#include <io.h>
#include <idt.h>
#include <printf.h>

/** Non-zero while preemptive scheduling from the timer IRQ is allowed. */
uint8_t sched_on = 0;

/**
 * Tick counter, bumped by the timer IRQ. 4-byte aligned so 32-bit reads and
 * writes are atomic (Intel SDM Vol 3A §8.1.1).
 */
uint32_t pit_ticks __attribute__ ((aligned (4)));

extern void pit_int();

/** @brief Set the scheduler-enable flag. @param on Non-zero to enable. */
void sched_state(int on) {
    sched_on = on;
}

/** @brief @return The current scheduler-enable flag. */
int get_sched_state() {
    return sched_on;
}

/** @brief Write @p cmd to the PIT command port. */
void pit_send_command(uint8_t cmd) {
    outportb(PIT_REG_COMMAND, cmd);
}

/**
 * @brief Write a reload byte to counter 0, 1 or 2.
 * @param data    Byte to write.
 * @param counter @c PIT_COUNTER_0, @c PIT_COUNTER_1 or @c PIT_COUNTER_2.
 */
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
 * @brief Program counter 0 for a periodic interrupt.
 *
 * The reload value is 1193180 / @p frequency (the PIT input clock).
 *
 * @param frequency Interrupts per second; 0 is ignored.
 * @param counter   Counter select bits for the command word.
 * @param mode      One of the @c PIT_MODE_* values.
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

/** @brief @return Ticks elapsed since the last @ref reset_tick_count. */
int get_tick_count() {
    return pit_ticks;
}

/** @brief Zero the tick counter. */
void reset_tick_count() {
    pit_ticks = 0;
}
