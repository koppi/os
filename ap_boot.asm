;
;  ap_boot.asm - SMP application-processor boot trampoline.
;
;  Built as a flat binary with `nasm -f bin` and copied to TRAMPOLINE_ADDR
;  (physical 0x8000) before the BSP raises INIT-SIPI-SIPI. Every AP begins
;  running here in real mode and must:
;    1) transition to 32-bit protected mode (with its own tiny flat GDT),
;    2) atomically claim its 1-based CPU index,
;    3) load the kernel page directory and enable paging,
;    4) jump to ap_main() on its own dedicated stack.
;
;  Per-CPU data lives in the fixed low block (also wired up by smp.c):
;       [0x8FFC]  dword  ap_claim     atomic 1-based index source
;       APINFO_BASE + i*APINFO_SIZE:  { cr3, stack_top, ap_main }   (0x9000)
;
;  Because the kernel is identity-mapped and the code segment is a flat base-0
;  segment, ORG = 0x8000 makes every label an absolute *linear* address: the
;  16-bit part reaches data with DS = 0 (base 0), and once in flat protected
;  mode the same labels are already the correct linear addresses.

BITS 16
ORG 0x8000

    global ap_entry
ap_entry:
    cli
    cld

    ; Access absolute low-memory addresses (labels) with a base-0 data segment.
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Load the tiny flat GDT embedded right after this code.
    lgdt [gdtr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump into flat 32-bit code. Emitted manually so the 16-bit offset is
    ; the full absolute linear address of ap_pm (0x801e): entering protected
    ; mode zero-extends it to EIP, and the base-0 code segment maps it straight
    ; to the trampoline's physical location.
    db 0xEA
    dw ap_pm                      ; target EIP (== ap_pm, org 0x8000)
    dw 0x0008                     ; ring-0 code selector

BITS 32
ap_pm:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Claim our 1-based CPU index atomically.
    ;   lock xadd [AP_CLAIM_ADDR], eax   -> eax = old value (my index)
    mov eax, 1
    lock xadd dword [0x8FFC], eax

    ; ecx = &apinfo[my_index] = APINFO_BASE + index*APINFO_SIZE
    imul ecx, eax, 0x40
    add ecx, 0x9000

    ; Enable paging with the kernel page directory so high kernel addresses
    ; (the stack and ap_main) become valid.
    mov eax, [ecx + 0]          ; cr3 (physical)
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; Switch to our per-CPU kernel stack and enter C. ap_main() never returns.
    mov esp, [ecx + 4]          ; stack_top
    mov eax, [ecx + 8]          ; ap_main (linear)
    jmp eax

    ; --- data: a 3-entry flat GDT (null, code 0x08, data 0x10). ---
    align 8, db 0
gdt:
    dq 0
    ; Code, DPL0, present, granularity 4K (base 0, limit 4 GiB).
    dw 0xFFFF, 0x0000            ; limit_low, base_low
    db 0x00, 0x9A, 0xCF, 0x00    ; base_middle, access, gran, base_high
    ; Data, DPL0, present.
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdtr:
    dw gdt_end - gdt - 1        ; limit
    dd gdt                      ; base (linear == physical here)
