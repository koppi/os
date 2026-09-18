/**
 * @file modfile.h
 * @brief Access to the Amiga MOD music module linked into the kernel image.
 *
 * The bytes of @c mods/01.mod are embedded by modfile_asm.asm.
 */
#pragma once

#include <types.h>

extern uint8_t modfile_0[];   /**< Embedded MOD file data. */
extern int     modfile_0_size; /**< Size of @ref modfile_0 in bytes. */
