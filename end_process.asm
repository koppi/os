extern end_proc

; void end_process(int ret, ...)
; Called through the syscall table by syscall_disp(); forwards the process
; return value (its first argument) to end_proc(). Does not return.
global end_process
end_process:
    push dword [esp + 4]
    call end_proc
    add esp, 4
    ret    ; We won't ever reach this point

