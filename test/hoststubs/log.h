/* Host stand-in for the kernel's include/log.h: klogf prints to stderr when
 * GP_VERBOSE is set in the environment, and is silent otherwise. */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_INFO    6
#define klogf(prio, ...) \
    do { (void) (prio); if (getenv("GP_VERBOSE")) fprintf(stderr, __VA_ARGS__); } while (0)
