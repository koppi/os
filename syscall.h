/**
 * @file syscall.h
 * @brief `int 0x72` system-call gate.
 */
#pragma once

/** Forward declaration; full definition lives in proc.h. */
struct regs;

/** @brief Install the ring-3-callable syscall gate on vector 0x72. */
void syscall_init(void);

/**
 * @brief Dispatch a system call: index in EAX, args in EBX/ECX/EDX/ESI/EDI,
 *        result written back to EAX. Called from the asm stub with the saved
 *        register frame.
 */
void syscall_disp(struct regs *re);
