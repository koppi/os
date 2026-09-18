/**
 * @file chardev.h
 * @brief Character-device interface used by the kernel console.
 */
#pragma once

#include <types.h>

/**
 * @brief A byte-stream device (e.g. the serial UART).
 *
 * @c kconsole points at one of these; @ref putchar_ writes through it.
 */
typedef struct chardev_struct {
    /** Read up to @c nbyte bytes into @c buf; returns the count read. */
    int (*read)(struct chardev_struct *dev, char *buf, size_t nbyte);
    /** Write @c nbyte bytes from @c buf; returns the count written. */
    int (*write)(struct chardev_struct *dev, const char *buf, size_t nbyte);
    /** Driver-private context pointer. */
    void *data;
} chardev_t;
