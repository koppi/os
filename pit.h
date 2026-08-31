/**
 * @file pit.h
 * @brief 8253/8254 Programmable Interval Timer control and the tick counter
 *        the scheduler runs off.
 */
#pragma once

#include <types.h>

/** @name Control-word field masks */
///@{
#define PIT_BINCOUNT_MASK       0x1
#define PIT_MODE_MASK           0xE
#define PIT_RL_MASK             0x30
#define PIT_COUNTER_MASK        0xC0
///@}

/** @name Control-word field values */
///@{
#define	PIT_BINCOUNT_BINARY     0
#define PIT_BINCOUNT_BCD        1
#define	PIT_MODE_TERMINALCOUNT  0
#define	PIT_MODE_ONESHOT        0x2
#define	PIT_MODE_RATEGEN        0x4
#define	PIT_MODE_SQUAREWAVEGEN  0x6
#define	PIT_MODE_SOFTWARETRIG   0x8
#define	PIT_MODE_HARDWARETRIG   0xA
#define PIT_RL_LATCH            0
#define	PIT_RL_LSBONLY          0x10
#define	PIT_RL_MSBONLY          0x20
#define	PIT_RL_DATA             0x30
#define PIT_COUNTER_0           0
#define	PIT_COUNTER_1           0x40
#define	PIT_COUNTER_2           0x80
///@}

/** @name I/O ports */
///@{
#define PIT_REG_COUNTER0        0x40
#define PIT_REG_COUNTER1        0x41
#define PIT_REG_COUNTER2        0x42
#define PIT_REG_COMMAND         0x43
///@}

/**
 * @brief Enable/disable preemptive task switching from the timer IRQ.
 * @param on Non-zero to allow schedule() to run on a tick.
 */
void sched_state(int on);

/** @brief Query the current scheduler enable flag. @return non-zero if on. */
int get_sched_state();

/** @brief Write a control byte to the PIT command register. */
void pit_send_command(uint8_t cmd);

/**
 * @brief Write a reload value byte to a counter.
 * @param data    Byte to write.
 * @param counter One of @c PIT_COUNTER_0..2.
 */
void pit_send_data(uint16_t data, uint8_t counter);

/** @brief Read a counter's current value byte. */
uint8_t pit_read_data(uint8_t counter);

/** @brief Install the timer IRQ handler on vector 32. */
void pit_init();

/**
 * @brief Program counter 0 to fire at @p frequency Hz.
 * @param frequency Interrupts per second.
 * @param counter   Counter selector (@c PIT_COUNTER_0 in practice).
 * @param mode      One of the @c PIT_MODE_* values.
 */
void pit_start_counter(uint32_t frequency, uint8_t counter, uint8_t mode);

/** @brief Ticks elapsed since the last reset. */
int get_tick_count();

/** @brief Reset the tick counter to 0. */
void reset_tick_count();

/**
 * @brief Free-running millisecond counter since boot (never reset by the
 *        scheduler, unlike @ref get_tick_count). Wraps after ~49 days.
 */
uint32_t pit_ms(void);
