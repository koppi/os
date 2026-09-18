/**
 * @file ver.h
 * @brief Compile-time OS version number.
 */
#pragma once

#include <types.h>

/**
 * @brief OS version as a (major, minor, revision) tuple.
 *
 * The single instance @c os_ver is defined in main.c and printed at boot.
 */
extern struct version_tuplet {
	uint32_t maj; /**< Major version. */
	uint32_t min; /**< Minor version. */
	uint32_t rev; /**< Revision. */
} os_ver;
