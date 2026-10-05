/**
 * @file apps/microui/shim/io.h
 * @brief <io.h> for the ring-3 build of the vendored ../../microui.c.
 *
 * microui.c includes the kernel's port-I/O header for one symbol: `halt()`,
 * which its `expect()` macro calls after printing a failed assertion. In the
 * kernel that is the `hlt` instruction, which a ring-3 program may not
 * execute -- it would turn a diagnosable assertion failure into a general
 * protection fault. mui.c supplies a userspace one instead: give the screen
 * back, then exit. Nothing else in microui.c touches this header, so nothing
 * else is here.
 */
#pragma once

/** @brief Release the screen and end the process. Does not return. */
void halt(void);
