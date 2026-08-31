/**
 * @file panic.c
 * @brief Unrecoverable-error handler.
 */
#include <io.h>
#include <printf.h>

/**
 * @brief Report a fatal kernel error and stop the machine (see @ref panic.h).
 *
 * Disables interrupts, prints "Kernel panic: " followed by the formatted
 * message, then halts the CPU. Never returns.
 */
void panic(const char *format, ...) {
    disable_int();

	printf("\n\nKernel panic: ");

    va_list args;
    va_start(args, format);
    vprintf(format, args);

    printf("\nHalting the system.\n");
    halt();
}
