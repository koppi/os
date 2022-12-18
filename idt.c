#include <idt.h>
#include <exception.h>
#include <lib/string.h> // for memset
#include <log.h>
#include <io.h>

struct idt_ptr idtr;
struct idt_info idt[NUM_INTERRUPTS];

void idt_init(uint16_t code) {
    int i;

    idtr.limit = sizeof(struct idt_info) * NUM_INTERRUPTS - 1;
    idtr.base = (uint32_t) &idt;
    
    memset(&idt, 0, idtr.limit);

    for(i = 0; i < NUM_INTERRUPTS; i++)
      install_ir(i, 0x80 | 0x0E, code, &default_ir_handler);
    
    install_ir(0, 0x80 | 0x0E, code, &ex_divide_by_zero);
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
    
    idt_set(&idtr);
}

#define PIC1 0x20
#define PIC1_DATA (PIC1 + 1)

#define PIC2 0xA0
#define PIC2_DATA (PIC2 + 1)

static void irq_clear_mask(size_t i) {
    uint16_t port = i < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t value = inportb(port) & ~(1 << i);
    outportb(port, value);
}

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
