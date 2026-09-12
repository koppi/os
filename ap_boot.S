/**
 * @file ap_boot.S
 * @brief SMP application-processor boot trampoline.
 *
 *  Assembled to a flat ELF (link script sets VMA/LMA = 0x8000) and wrapped as a
 *  flat binary via objcopy; C accesses it via
 *  _binary_ap_boot_bin_start/_end/_size. Every AP begins running here in real
 *  mode and must:
 *    1) transition to 32-bit protected mode (with its own tiny flat GDT),
 *    2) atomically claim its 1-based CPU index,
 *    3) load the kernel page directory and enable paging,
 *    4) jump to ap_main() on its own dedicated stack.
 *
 *  Per-CPU data lives in the fixed low block (also wired up by smp.c):
 *       [0x8FFC]  dword  ap_claim     atomic 1-based index source
 *       APINFO_BASE + i*APINFO_SIZE:  { cr3, stack_top, ap_main }   (0x9000)
 *
 *  Because the kernel is identity-mapped and the code segment is a flat base-0
 *  segment, linking at ORG = 0x8000 makes every label an absolute *linear*
 *  address: the 16-bit part reaches data with DS = 0 (base 0), and once in flat
 *  protected mode the same labels are already the correct linear addresses.
 */

    .code16
    .section .text

    .globl ap_entry
    .type ap_entry, @function
ap_entry:
    cli
    cld

    /* Access absolute low-memory addresses (labels) with a base-0 data segment. */
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    /* Load the tiny flat GDT embedded right after this code. */
    lgdt gdtr

    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0

    /* Far jump into flat 32-bit code. Emitted manually so the 16-bit offset is
     * the full absolute linear address of ap_pm (0x8000 + offset): entering
     * protected mode zero-extends it to EIP, and the base-0 code segment maps
     * it straight to the trampoline's physical location. */
    .byte 0xEA
    .word ap_pm                      /* target EIP (== ap_pm, org 0x8000) */
    .word 0x0008                     /* ring-0 code selector */

    .code32
ap_pm:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    /* Claim our 1-based CPU index atomically.
     *   lock xadd [AP_CLAIM_ADDR], eax   -> eax = old value (my index) */
    movl $1, %eax
    lock xaddl %eax, 0x8FFC

    /* ecx = &apinfo[my_index] = APINFO_BASE + index*APINFO_SIZE */
    imull $0x40, %eax, %ecx
    addl $0x9000, %ecx

    /* Enable paging with the kernel page directory so high kernel addresses
     * (the stack and ap_main) become valid. */
    movl 0(%ecx), %eax              /* cr3 (physical) */
    movl %eax, %cr3
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    /* Switch to our per-CPU kernel stack and enter C. ap_main() never returns. */
    movl 4(%ecx), %esp              /* stack_top */
    movl 8(%ecx), %eax              /* ap_main (linear) */
    jmp *%eax
    .size ap_entry, .-ap_entry

    /* --- data: a 3-entry flat GDT (null, code 0x08, data 0x10). --- */
    .balign 8, 0
gdt:
    .quad 0
    /* Code, DPL0, present, granularity 4K (base 0, limit 4 GiB). */
    .word 0xFFFF, 0x0000            /* limit_low, base_low */
    .byte 0x00, 0x9A, 0xCF, 0x00    /* base_middle, access, gran, base_high */
    /* Data, DPL0, present. */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdtr:
    .word gdt_end - gdt - 1          /* limit */
    .long gdt                        /* base (linear == physical here) */
