/**
 * @file tss.h
 * @brief Task State Segment — only ESP0/SS0 are used, so ring 3 → ring 0
 *        transitions land on the right kernel stack.
 */
#pragma once

#include <types.h>

/* Forward declaration to avoid pulling the whole per-CPU header in here. */
struct cpu;

/** The 32-bit hardware TSS layout (Intel SDM). Only esp0/ss0 are meaningful here. */
typedef struct tss {
    uint32_t prev_tss;
    uint32_t esp0;   /**< Ring-0 stack pointer loaded on a privilege change. */
    uint32_t ss0;    /**< Ring-0 stack segment. */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtr;
    uint16_t iomap;  /**< I/O permission bitmap offset. */
} __attribute__((__packed__)) tss_t;

/** @brief Load the task register with the TSS selector (@c ltr). */
void flush_tss();

/** @brief Load @p cpu's task register with its own TSS selector. */
void flush_tss_for(struct cpu *c);

/** @brief Initialise @p cpu's TSS and install its GDT descriptor. */
void tss_init_cpu(struct cpu *c);

/** @brief Add the TSS descriptor to the GDT, zero the TSS and load it. */
void install_tss();

/**
 * @brief Set the ring-0 stack pointer used on the next interrupt from ring 3.
 * @param esp Kernel stack top for the process about to run.
 */
void set_esp0(uint32_t esp);
