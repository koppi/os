/**
 * @file exception.c
 * @brief C side of the CPU-exception handlers.
 *
 * Each handler prints a diagnostic. If the fault happened in kernel mode
 * (DS == 0x10) it is unrecoverable and the machine panics; if it
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
 * @brief Return non-zero if the current DS selector belongs to ring 3.
 *
 * For direct C exception handlers the CPU leaves DS untouched, so a
 * user-mode fault still carries the user data selector (0x23) while
 * kernel-mode faults run with DS == 0x10.  This is reliable regardless
 * of whether the exception pushed an error code or switched stacks.
 */
static inline int is_user_mode(void) {
    uint32_t ds;
    asm volatile("mov %%ds, %0" : "=r"(ds));
    return ds != 0x10;
}

/**
 * @brief Return non-zero if the saved code selector @p cs belongs to ring 3.
 *
 * The kernel code selector is 0x08 (RPL 0); ring-3 code runs with 0x1B (RPL 3).
 * Test the RPL bits, not equality against the *data* selector 0x10 — a kernel
 * fault carries cs 0x08, and treating that as user mode sends an unrecoverable
 * fault down @ref return_exception instead of panicking.
 */
static inline int is_user_mode_cs(uint32_t cs) {
    return (cs & 3) != 0;
}

/**
 * @brief Hand control to the userspace return stub with status 1.
 *
 * Used by the exception handlers when a ring-3 fault should kill the process
 * rather than panic the kernel. Never returns — the stub at @ref RETURN_ADDR
 * issues the exit syscall.
 *
 * EAX must hold the status (1) at the indirect call: load it in the same asm
 * block that performs the call and mark EAX clobbered, so the compiler does not
 * stage the call target (a PIC base register) in EAX across the load.
 */
void return_exception() {
    void (*stub)(void) = return_error;
    asm volatile("movl $1, %%eax\n\t"
                 "call *%0\n\t"
                 :
                 : "r"(stub)
                 : "eax", "ecx", "edx", "memory");
    __builtin_unreachable();
}

/** @brief Handler for an unexpected vector — always fatal. */
void default_ir_handler() {
    disable_int();
    printf("Unhandled exception\n");
    panic("");
}

/** @brief \#DE — divide error. Fatal in kernel, kills process in user. */
void ex_divide_by_zero() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Division by zero\n");
    panic("");
}

/** @brief \#DB — debug / single step. Fatal in kernel, kills process in user. */
void ex_single_step() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Single step\n");
    panic("");
}

/** @brief Non-maskable interrupt trap. Always fatal. */
void ex_nmi() {
    printf("NMI trap\n");
    panic("");
}

/** @brief \#BP — breakpoint. Fatal in kernel, kills process in user. */
void ex_breakpoint() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Breakpoint\n");
    panic("");
}

/** @brief \#OF — overflow. Fatal in kernel, kills process in user. */
void ex_overflow() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Overflow\n");
    panic("");
}

/** @brief \#BR — bound range exceeded. Fatal in kernel, kills process in user. */
void ex_bounds_check() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Bounds check\n");
    panic("");
}

/**
 * @brief \#UD — invalid opcode. Dumps the saved register frame.
 * @param re Saved registers; a kernel-mode fault panics, a user-mode fault
 *           unwinds the process.
 */
void ex_invalid_opcode(struct regs *re) {
    printf("Invalid opcode\n");
    printf("eip: %x cs: %x\neax: %u ebx: %u ecx: %u edx: %u\nesp: %x ebp: %x esi: %u edi: %u\nds: %x es: %x fs: %x gs: %x\n", re->eip, re->cs, re->eax, re->ebx, re->ecx, re->edx, re->esp, re->ebp, re->esi, re->edi, re->ds, re->es, re->fs, re->gs);
    if(is_user_mode_cs(re->cs)) {
        return_exception();
    } else {
        panic("");
    }
}

/** @brief \#NM — device (FPU) not available. Fatal in kernel, kills process in user. */
void ex_device_not_available() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Device not available\n");
    panic("");
}

/** @brief \#DF — double fault. Always fatal. */
void ex_double_fault() {
    printf("Double fault\n");
    panic("");
}

/** @brief \#TS — invalid TSS. Fatal in kernel, kills process in user. */
void ex_invalid_tss() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Invalid TSS\n");
    panic("");
}

/** @brief \#NP — segment not present. Fatal in kernel, kills process in user. */
void ex_segment_not_present() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Segment not present\n");
    panic("");
}

/** @brief \#SS — stack-segment fault. Fatal in kernel, kills process in user. */
void ex_stack_fault() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Stack fault\n");
    panic("");
}

/**
 * @brief \#GP — general protection fault. Dumps the register frame plus
 *        CR2/CR3; kernel-mode is fatal, user-mode unwinds the process.
 * @param re Saved register/error-code frame.
 */
void ex_gpf(struct regs_error *re) {
    printf("\nGeneral protection fault\nError code: %u\n", re->error);
    printf("eip: %x cs: %x\neax: %u ebx: %u ecx: %u edx: %u\nesp: %x ebp: %x esi: %u edi: %u\nds: %x es: %x fs: %x gs: %x\n", re->eip, re->cs, re->eax, re->ebx, re->ecx, re->edx, re->esp, re->ebp, re->esi, re->edi, re->ds, re->es, re->fs, re->gs);
    printf("cr2: %x cr3: %x\n", get_cr2(), get_pdbr());

    if(is_user_mode_cs(re->cs)) {
        return_exception();
    } else {
        panic("");
    }
}

/**
 * @brief \#PF — page fault. Decodes CR2 and the error code, then panics
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
    printf("eip: %x cs: %x\neax: %x ebx: %x ecx: %x edx: %x\nesp: %x ebp: %x esi: %x edi: %x\nds: %x es: %x fs: %x gs: %x\n",
           re->eip, re->cs, re->eax, re->ebx, re->ecx, re->edx, re->esp, re->ebp,
           re->esi, re->edi, re->ds, re->es, re->fs, re->gs);
    printf("cr3: %x\n", get_pdbr());
    if(is_user_mode_cs(re->cs)) {
        return_exception();
    } else {
        panic("");
    }
}

/** @brief \#MF — x87 FPU error. Fatal in kernel, kills process in user. */
void ex_fpu_error() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("FPU error\n");
    panic("");
}

/** @brief \#AC — alignment check. Fatal in kernel, kills process in user. */
void ex_alignment_check() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("Alignment check\n");
    panic("");
}

/** @brief \#MC — machine check. Always fatal. */
void ex_machine_check() {
    printf("Machine check\n");
    panic("");
}

/** @brief \#XM — SIMD floating-point exception. Fatal in kernel, kills process in user. */
void ex_simd_fpu() {
    if (is_user_mode()) {
        return_exception();
    }
    printf("SIMD FPU error\n");
    panic("");
}
