#pragma once

#include <types.h>

// Input a single byte from the specified port.
static inline uint8_t inportb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %%dx, %%al" : "=a" (val) : "d" (port));
    return val;
}

// Input a single uint32 from the specified port.
static inline uint32_t inportl(uint32_t port) {
    uint32_t val;
    __asm__ volatile("inl %%dx,%%eax":"=a" (val):"d"(port));
    return val;
}

// Input a single word from the specified port.
static inline uint16_t inportw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %%dx, %%ax" : "=a" (val) : "d" (port));
    return val;
}

// Output a single byte to the specified port.
static inline void outportb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %%al, %%dx" : : "d" (port), "a" (val));
}

// Output a single uint32 to the specified port.
static inline void outportl(uint32_t port, uint32_t val) {
    __asm__ volatile("outl %%eax,%%dx"::"d" (port), "a" (val));
}

void halt();
void exit_qemu(const int exit_status);
void enable_int();
void disable_int();
void sleep(int s);
