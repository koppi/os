#pragma once

#include <types.h>

// Defines the OS version as a major, minor, revision tuplet.
extern struct version_tuplet {
	uint32_t maj, min, rev;
} os_ver;
