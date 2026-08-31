/**
 * @file kconsole.c
 * @brief Kernel console output sink — fans one character out to the serial
 *        console, the on-screen scroll-back log and the VGA text buffer.
 */
#include <kconsole.h>
#include <graphics.h>
#include <vga.h>

/** The character device kernel output goes to (serial UART during boot). */
chardev_t *kconsole;

/**
 * @brief Write character @p c to all kernel output surfaces.
 *
 * This is the single-character primitive the printf implementation calls.
 */
void putchar_(char c) {
    kconsole->write(kconsole, &c, 1);
    char buf[2];
    buf[0] = c;
    buf[1] = 0;
    write_log(buf);
    vga_putchar(c);
}
