/**
 * @file apps/microui/hostinc/types.h
 * @brief <types.h> for the host build of mkfont.c.
 *
 * ssfn.h includes <types.h>, which in this tree is the kernel's freestanding
 * one. mkfont runs on the build machine against real glibc headers, where
 * that file's own typedefs for size_t/uint32_t collide with the system's, so
 * the host build gets this shim on its include path instead.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
