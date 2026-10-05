/**
 * @file apps/microui/shim/stdlib.h
 * @brief <stdlib.h> for the ring-3 build of the vendored ../../microui.c.
 *
 * microui.c wants one thing from <stdlib.h> that the ring-3 libc in lib/ does
 * not provide: `qsort`, which mu_end() uses to order the root containers by
 * z-index. The kernel build picks it up from the repo root's stdlib.h (and
 * qsort.c / qsort_r.c, which are kernel objects). This header puts the
 * declaration in front of microui.c without forking include/lib/stdlib.h,
 * whose other users have no need of it; mui.c has the implementation.
 */
#pragma once

#include <lib/stdlib.h>   /* malloc / free / realloc */

/** @brief Sort @p n elements of @p size bytes at @p base with @p cmp. */
void qsort(void *base, size_t n, size_t size,
           int (*cmp)(const void *, const void *));
