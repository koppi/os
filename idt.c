/**
 * @file idt.c
 * @brief Interrupt Descriptor Table construction and per-vector gate install.
 */
#include <idt.h>
#include <exception.h>
#include <apic.h>
#include <smp_asm.h>
#include <lib/string.h> // for memset
#include <log.h>
#include <io.h>

struct idt_ptr idtr;                    /**< @c lidt operand. */
struct idt_info idt[NUM_INTERRUPTS];    /**< The table itself. */

/**
 * @brief Point every vector at @ref default_ir_handler, then override the
 *        CPU-exception vectors (0-19) with their dedicated handlers, and load
 *        the table. @p code is the kernel code selector for every gate.
 */
void idt_init(uint16_t code) {
    int i;

    idtr.limit = sizeof(struct idt_info) * NUM_INTERRUPTS - 1;
    idtr.base = (uint32_t) &idt;

    memset(&idt, 0, idtr.limit);

    for(i = 0; i < NUM_INTERRUPTS; i++)
      install_ir(i, 0x80 | 0x0E, code, &default_ir_handler);

    install_ir(0, 0x80 | 0x0E, code, &de_handle);
    install_ir(1, 0x80 | 0x0E, code, &ex_single_step);
    install_ir(2, 0x80 | 0x0E, code, &ex_nmi);
    install_ir(3, 0x80 | 0x0E, code, &ex_breakpoint);
    install_ir(4, 0x80 | 0x0E, code, &ex_overflow);
    install_ir(5, 0x80 | 0x0E, code, &ex_bounds_check);
    install_ir(6, 0x80 | 0x0E, code, &invop_handle);
    install_ir(7, 0x80 | 0x0E, code, &ex_device_not_available);
    install_ir(8, 0x80 | 0x0E, code, &ex_double_fault);

    install_ir(10, 0x80 | 0x0E, code, &ex_invalid_tss);
    install_ir(11, 0x80 | 0x0E, code, &ex_segment_not_present);
    install_ir(12, 0x80 | 0x0E, code, &ex_stack_fault);
    install_ir(13, 0x80 | 0x0E, code, &gpf_handle);
    install_ir(14, 0x80 | 0x0E, code, &pf_handle);

    install_ir(16, 0x80 | 0x0E, code, &ex_fpu_error);
    install_ir(17, 0x80 | 0x0E, code, &ex_alignment_check);
    install_ir(18, 0x80 | 0x0E, code, &ex_machine_check);
    install_ir(19, 0x80 | 0x0E, code, &ex_simd_fpu);

    /* Local APIC vectors (0xEF timer, 0xFC resched-IPI, 0xFD TLB-IPI,
     * 0xFE LAPIC error, 0xFF spurious). */
    install_ir(0xEF, 0x80 | 0x0E, code, &lapic_timer_int);
    install_ir(0xFC, 0x80 | 0x0E, code, &ipi_resched_int);
    install_ir(0xFD, 0x80 | 0x0E, code, &ipi_tlb_int);
    install_ir(0xFE, 0x80 | 0x0E, code, &lapic_error_int);
    install_ir(0xFF, 0x80 | 0x0E, code, &lapic_spurious_int);

    idt_set(&idtr);
}

/**
 * @brief Load this CPU's IDT register.
 *
 * The table is global (built once by @ref idt_init); an application processor
 * must simply point its own IDTR at it. Unlike @ref idt_init this does not
 * re-program the 8259 PIC, which is BSP-only.
 */
void idt_load(void) {
    idt_set(&idtr);
}

#define PIC1 0x20
#define PIC1_DATA (PIC1 + 1)

#define PIC2 0xA0
#define PIC2_DATA (PIC2 + 1)

/**
 * @brief Unmask the 8259 line for IDT vector @p vec so its IRQ is delivered.
 *
 * PIC IRQs are remapped to base 0x20, so IRQ = vec - 0x20. The old code used
 * the vector itself as the line number and picked the PIC by `vec < 8`, so it
 * poked random bits of the *slave* for every vector (keyboard/PIT only worked
 * because firmware had already unmasked the master). That left IRQ 12 masked
 * on real hardware -> no PS/2 mouse. For a slave IRQ (8..15) the master's
 * cascade line (IRQ 2) is unmasked too.
 */
static void irq_clear_mask(size_t vec) {
    if (vec < 0x20 || vec >= 0x30)
        return;                                   /* not a PIC-mapped vector */
    unsigned irq = (unsigned) vec - 0x20;
    if (irq < 8) {
        outportb(PIC1_DATA, inportb(PIC1_DATA) & (uint8_t) ~(1u << irq));
    } else {
        outportb(PIC2_DATA, inportb(PIC2_DATA) & (uint8_t) ~(1u << (irq - 8)));
        outportb(PIC1_DATA, inportb(PIC1_DATA) & (uint8_t) ~(1u << 2));
    }
}

/**
 * @brief Write handler @p irq into IDT slot @p i and unmask the PIC line.
 *        See @ref idt.h for the argument meanings.
 */
void install_ir(uint32_t i, uint16_t flags, uint16_t sel, void *irq) {
    uint32_t ir_addr = (uint32_t) irq;

    idt[i].base_low = (uint16_t) ir_addr & 0xFFFF;
    idt[i].base_high = (uint16_t) (ir_addr >> 16) & 0xFFFF;
    idt[i].ist = 0;
    idt[i].flags = (uint8_t) flags;
    idt[i].sel = sel;

    irq_clear_mask(i);

    //klogf(LOG_INFO, "install_ir addr 0x%x irq %d\n", ir_addr, i);
}
