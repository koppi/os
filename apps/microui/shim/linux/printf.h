/**
 * @file apps/microui/shim/linux/printf.h
 * @brief <linux/printf.h> for the ring-3 microui apps.
 *
 * The declarations are include/linux/printf.h's, included here by path so that
 * <linux/printf.h> stays the shim's (which comes first on the search path) and
 * not the kernel's own -- the kernel's <linux/stdlib.h>, <linux/assert.h> and
 * driver headers are shadowed or simply unused by a ring-3 program.
 *
 * The same file is what the vendored ../../../../lib/microui.c picks up for its own
 * `#include <linux/printf.h>`, so both sides of this build agree on what `sprintf` means. The implementation
 * linked in is apps/doom/shim/printf.c -- the same upstream, already built
 * for ring 3; see ../mui.mk.
 */
#pragma once

#include "../../../../include/linux/printf.h"
