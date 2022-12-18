#include <kconsole.h>
#include <graphics.h>
#include <vga.h>

chardev_t *kconsole;

void putchar_(char c) {
    kconsole->write(kconsole, &c, 1);
    char buf[2];
    buf[0] = c;
    buf[1] = 0;
    write_log(buf);
    vga_putchar(c);
}
