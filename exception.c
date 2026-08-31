/**
 * @file exception.c
 * @brief C side of the CPU-exception handlers.
 *
 * Each handler prints a diagnostic. If the fault happened in kernel mode
 * (saved ES == 0x10) it is unrecoverable and the machine panics; if it
 * happened in ring 3 the current process is torn down via
 * @ref return_exception instead.
 */
#include <io.h>
#include <panic.h>
#include <printf.h>
#include <proc.h>
#include <mm.h>
#include <paging.h>

/** Entry point jumped to in userspace to unwind a faulted process. */
void (*return_error)() = (void *) RETURN_ADDR;

/**
 * @brief Hand control to the userspace return stub with status 1.
 *
 * Used by the exception handlers when a ring-3 fault should kill the process
 * rather than panic the kernel.
 */
void return_exception() {
    int error = 1;
    asm volatile("mov %0, %%eax" : : "r" (error));
    (*return_error)();
}

/** @brief Handler for an unexpected vector — always fatal. */
void default_ir_handler() {
    disable_int();
    printf("Unhandled exception\n");
    panic("");
}

/** @brief #DE — divide error. Fatal. */
void ex_divide_by_zero() {
    printf("Division by zero\n");
    panic("");
}

/** @brief #DB — debug / single step. Fatal. */
void ex_single_step() {
    printf("Single step\n");
    panic("");
}

/** @brief Non-maskable interrupt trap. Fatal. */
void ex_nmi() {
    printf("NMI trap\n");
    panic("");
}

/** @brief #BP — breakpoint. Fatal. */
void ex_breakpoint() {
    printf("Breakpoint\n");
    panic("");
}

/** @brief #OF — overflow. Fatal. */
void ex_overflow() {
    printf("Overflow\n");
    panic("");
}

/** @brief #BR — bound range exceeded. Fatal. */
void ex_bounds_check() {
    printf("Bounds check\n");
    panic("");
}

/**
 * @brief #UD — invalid opcode. Dumps the saved register frame.
 * @param re Saved registers; a kernel-mode fault panics, a user-mode fault
 *           unwinds the process.
 */
void ex_invalid_opcode(struct regs *re) {
    printf("Invalid opcode\n");
    printf("eip: %x cs: %x\neax: %u ebx: %u ecx: %u edx: %u\nesp: %x ebp: %x esi: %u edi: %u\nds: %x es: %x fs: %x gs: %x\n", re->eip, re->cs, re->eax, re->ebx, re->ecx, re->edx, re->esp, re->ebp, re->esi, re->edi, re->ds, re->es, re->fs, re->gs);
    if(re->es == 0x10) {
        // If an Invalid Opcode occurs in kernel mode, we don't really want to continue
        panic("");
    } else {
        // If we were in user mode, just kill that thread or process
        return_exception();
    }
}

/** @brief #NM — device (FPU) not available. Fatal. */
void ex_device_not_available() {
    printf("Device not available\n");
    panic("");
}

/** @brief #DF — double fault. Fatal. */
void ex_double_fault() {
    printf("Double fault\n");
    panic("");
}

/** @brief #TS — invalid TSS. Fatal. */
void ex_invalid_tss() {
    printf("Invalid TSS\n");
    panic("");
}

/** @brief #NP — segment not present. Fatal. */
void ex_segment_not_present() {
    printf("Segment not present\n");
    panic("");
}

/** @brief #SS — stack-segment fault. Fatal. */
void ex_stack_fault() {
    printf("Stack fault\n");
    panic("");
}

/**
 * @brief #GP — general protection fault. Dumps the register frame plus
 *        CR2/CR3; kernel-mode is fatal, user-mode unwinds the process.
 * @param re Saved register/error-code frame.
 */
void ex_gpf(struct regs_error *re) {
    printf("\nGeneral protection fault\nError code: %u\n", re->error);
    printf("eip: %x cs: %x\neax: %u ebx: %u ecx: %u edx: %u\nesp: %x ebp: %x esi: %u edi: %u\nds: %x es: %x fs: %x gs: %x\n", re->eip, re->cs, re->eax, re->ebx, re->ecx, re->edx, re->esp, re->ebp, re->esi, re->edi, re->ds, re->es, re->fs, re->gs);
    printf("cr2: %x cr3: %x\n", get_cr2(), get_pdbr());

    // If a GPF occurs in kernel mode, we don't really want to continue
    if(re->es == 0x10) {
        panic("");
    } else {
        // If we were in user mode, just kill that thread or process
        return_exception();
    }
}

/**
 * @brief #PF — page fault. Decodes CR2 and the error code, then panics
 *        (kernel-mode fault) or unwinds the process (user-mode fault).
 * @param re Saved register/error-code frame.
 */
void ex_page_fault(struct regs_error *re) {
    int virt_addr = get_cr2();
    mm_addr_t phys_addr = (mm_addr_t) get_phys_addr(get_page_directory(), virt_addr);

    int present = !(re->error & 1);   // Page not present
    int rw = re->error & 0x2;         // Write operation
    int us = re->error & 0x4;         // User mode?
    int reserved = re->error & 0x8;   // Overwritten CPU-reserved bits of page entry
    int id = re->error & 0x10;        // Caused by an instruction fetch?

    printf("\nPage fault occurs while %s address 0x%x\nPage attributes: %s%s%s%s",
           rw?"writing":"reading",
           (unsigned)virt_addr,
           present?"\0":"not-present ",
           us?"user-mode ":"\0",
           reserved?"cpu-reserved ":"\0",
           id?"instruction-fetch":"\0"
    );

    //printf("\nPage fault at addr: 0x%x\n", (unsigned)virt_addr);
    printf("Phys addr: 0x%x\n", phys_addr);
    // If a Page Fault occurs in kernel mode, we don't really want to continue
    if(re->es == 0x10) {
        panic("");
    } else {
        // If we were in user mode, just kill that thread or process
        return_exception();
    }
}

/** @brief #MF — x87 FPU error. Fatal. */
void ex_fpu_error() {
    printf("FPU error\n");
    panic("");
}

/** @brief #AC — alignment check. Fatal. */
void ex_alignment_check() {
    printf("Alignment check\n");
    panic("");
}

/** @brief #MC — machine check. Fatal. */
void ex_machine_check() {
    printf("Machine check\n");
    panic("");
}

/** @brief #XM — SIMD floating-point exception. Fatal. */
void ex_simd_fpu() {
    printf("SIMD FPU error\n");
    panic("");
}
