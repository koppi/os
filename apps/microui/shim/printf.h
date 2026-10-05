/**
 * @file apps/microui/shim/printf.h
 * @brief <printf.h> for the ring-3 microui apps.
 *
 * The declarations are the repo root's, included here by path rather than by
 * putting the whole kernel include directory on an app's search path -- where
 * it would also offer the kernel's <stdlib.h>, <assert.h> and every driver
 * header, none of which a ring-3 program should be able to reach by accident.
 *
 * The same file is what the vendored ../../microui.c picks up for its own
 * `#include "printf.h"` (a quoted include resolves next to microui.c), so
 * both sides of this build agree on what `sprintf` means. The implementation
 * linked in is apps/doom/shim/printf.c -- the same upstream, already built
 * for ring 3; see ../mui.mk.
 */
#pragma once

#include "../../../printf.h"
