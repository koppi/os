;
;  smp_asm.asm - Local APIC interrupt stubs.
;
;  lapic_timer_int    LAPIC timer tick (drives per-CPU preemption instead of the
;                     PIT). Bumps the local CPU's sched_ticks via a C helper,
;                     optionally runs schedule(), then EOIs the LAPIC.
;  ipi_tlb_int        TLB-shootdown IPI: flush the requested TLB entries and EOI.
;  ipi_resched_int    Reschedule IPI: kick the target CPU's scheduler and EOI.
;  lapic_spurious_int Spurious interrupt (no EOI needed).
;  lapic_error_int    LAPIC error interrupt: clear and EOI.

extern lapic_base
extern lapic_timer_tick
extern ipi_tlb_handler
extern ipi_resched_tick
extern lapic_eoi

%macro LAPIC_EOI 0
    push eax
    mov eax, [lapic_base]
    mov dword [eax + 0xB0], 0
    pop eax
%endmacro

global lapic_timer_int
lapic_timer_int:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    push ds
    push es
    push fs
    push gs

    mov ebx, esp            ; save stack pointer

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push ebx
    call lapic_timer_tick    ; uint32_t lapic_timer_tick(uint32_t esp)
    add esp, 4
    mov esp, eax             ; switch to the chosen thread's kernel stack

    LAPIC_EOI

    pop gs
    pop fs
    pop es
    pop ds
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    iretd

global ipi_tlb_int
ipi_tlb_int:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call ipi_tlb_handler

    LAPIC_EOI

    pop gs
    pop fs
    pop es
    pop ds
    popa
    iretd

global ipi_resched_int
ipi_resched_int:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    push ds
    push es
    push fs
    push gs

    mov ebx, esp

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push ebx
    call ipi_resched_tick
    add esp, 4
    mov esp, eax

    LAPIC_EOI

    pop gs
    pop fs
    pop es
    pop ds
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    iretd

global lapic_spurious_int
lapic_spurious_int:
    iret

global lapic_error_int
lapic_error_int:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; The error handler just needs to EOI so the LAPIC resumes normal delivery.
    LAPIC_EOI

    pop gs
    pop fs
    pop es
    pop ds
    popa
    iretd
