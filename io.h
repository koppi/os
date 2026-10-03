/**
 * @file io.h
 * @brief x86 port I/O primitives and low-level CPU/interrupt controls.
 *
 * The `inport*` / `outport*` helpers are thin wrappers around the `in`/`out`
 * instructions; the rest control the interrupt flag, halt the CPU, and provide
 * a coarse tick-based busy sleep.
 */
#pragma once

#include <types.h>

/**
 * @brief Read one byte from an I/O port.
 * @param port Port address.
 * @return The byte read.
 */
static inline uint8_t inportb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %%dx, %%al" : "=a" (val) : "d" (port));
    return val;
}

/**
 * @brief Read one 32-bit dword from an I/O port.
 * @param port Port address.
 * @return The dword read.
 */
static inline uint32_t inportl(uint32_t port) {
    uint32_t val;
    __asm__ volatile("inl %%dx,%%eax":"=a" (val):"d"(port));
    return val;
}

/**
 * @brief Read one 16-bit word from an I/O port.
 * @param port Port address.
 * @return The word read.
 */
static inline uint16_t inportw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %%dx, %%ax" : "=a" (val) : "d" (port));
    return val;
}

/**
 * @brief Write one byte to an I/O port.
 * @param port Port address.
 * @param val  Byte to write.
 */
static inline void outportb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %%al, %%dx" : : "d" (port), "a" (val));
}

/**
 * @brief Write one 16-bit word to an I/O port.
 * @param port Port address.
 * @param val  Word to write.
 */
static inline void outportw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * @brief Write one 32-bit dword to an I/O port.
 * @param port Port address.
 * @param val  Dword to write.
 */
static inline void outportl(uint32_t port, uint32_t val) {
    __asm__ volatile("outl %%eax,%%dx"::"d" (port), "a" (val));
}

/**
 * @brief Barrier between filling a DMA structure and handing it to a device.
 *
 * x86 retires stores in program order, so no CPU fence is needed here -- but
 * the compiler is under no such obligation. Only *volatile* accesses are
 * ordered with respect to each other, so the plain stores that build a
 * descriptor, a TRB or a ring can legally be scheduled *after* the volatile
 * MMIO write (or doorbell) that tells the device to go read it. At -O3 that
 * is not theoretical: it is what GCC did to the xHCI event-ring segment
 * table, publishing a half-built entry (see xhci.c).
 *
 * Call it between the last store that builds a structure and the write that
 * hands it over -- and, inside a structure the device polls, between the body
 * and the dword whose flag says "this entry is yours now".
 */
static inline void dma_wmb(void) { __asm__ volatile("" ::: "memory"); }

/** @brief Execute a single @c hlt (wait for the next interrupt). */
void halt(void);

/**
 * @brief Shut the machine down through QEMU's isa-debug-exit device, then
 *        fall back to a keyboard-controller triple fault; never returns.
 * @param status_code Non-zero value is written to port 0xF4 as the QEMU exit code.
 */
void exit_qemu(const int status_code);

/** @brief Set the interrupt flag (`sti`) and log it. */
void enable_int(void);

/** @brief Clear the interrupt flag (`cli`) and log it. */
void disable_int(void);

/**
 * @brief Busy-wait for roughly @p s PIT ticks, halting between checks.
 * @param s Number of ticks to wait (PIT runs at 1 kHz).
 */
void sleep(int s);
