/**
 * @file panic.c
 * @brief Unrecoverable-error handler.
 */
#include <io.h>
#include <printf.h>
#include <video.h>

/** @brief Stop every other CPU (implemented in smp.c). Safe before SMP is up. */
void smp_halt_others(void);

/**
 * @brief Report a fatal kernel error and stop the machine (see @ref panic.h).
 *
 * Disables interrupts, freezes the other CPUs (so the desktop compositor cannot
 * scribble over the message), prints "Kernel panic: " followed by the formatted
 * message, then halts. Output goes to every console sink, including the
 * framebuffer text console — the only one visible on a machine with no serial
 * port. Never returns.
 */
void panic(const char *format, ...) {
    disable_int();
    smp_halt_others();

    printf("\n\n*** KERNEL PANIC ***\n");

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\nSystem halted.\n");
    for (;;)
        asm volatile("cli; hlt");
}
