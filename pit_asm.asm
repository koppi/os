;
;  Copyright 2016 Davide Pianca
;
;  Licensed under the Apache License, Version 2.0 (the "License");
;  you may not use this file except in compliance with the License.
;  You may obtain a copy of the License at
;
;      http://www.apache.org/licenses/LICENSE-2.0
;
;  Unless required by applicable law or agreed to in writing, software
;  distributed under the License is distributed on an "AS IS" BASIS,
;  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
;  See the License for the specific language governing permissions and
;  limitations under the License.
;

; The PIT keeps running for the free-running ms clock (pit_uptime) and the
; legacy tick counter. Preemption is no longer driven from here: it moved to the
; per-CPU LAPIC timer (see smp_asm.asm / lapic_timer_tick).

extern pit_ticks
extern pit_uptime

global pit_int
pit_int:

    ; push registers
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

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    lock inc long [pit_ticks]    ; legacy tick counter (get_tick_count used it)
    lock inc long [pit_uptime]   ; free-running ms counter (never reset)

    mov al, 0x20                ; PIC acknowledge (PIT is on master IRQ0)
    out 0x20, al

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

global fork_eip
fork_eip:
    pop eax
    jmp eax
