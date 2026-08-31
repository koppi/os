/**
 * @file log.h
 * @brief Kernel logging macros. Both @ref printk and @ref klogf prefix each
 *        line with a dmesg-style "[   sec.usec]" timestamp; @ref klogf adds a
 *        "file:line [PRIO]" tag with severities that follow RFC 5424.
 */
#pragma once

#include <printf.h>
#include <io.h>
#include <pit.h>

/* Use as as defined in RFC5424:

           Numerical         Severity
             Code

              0       Emergency: system is unusable
              1       Alert: action must be taken immediately
              2       Critical: critical conditions
              3       Error: error conditions
              4       Warning: warning conditions
              5       Notice: normal but significant condition
              6       Informational: informational messages
              7       Debug: debug-level messages
 */
#define LOG_EMERG	0
#define LOG_ALERT	1
#define LOG_CRIT	2
#define LOG_ERR 	3
#define LOG_WARNING	4
#define LOG_NOTICE	5
#define LOG_INFO	6
#define LOG_DEBUG	7

/**
 * @brief Print a dmesg-style "[   sec.usec] " timestamp for the current moment.
 *
 * The figure is seconds.microseconds since the PIT started (@ref pit_ms, 1 ms
 * resolution; reads 0.000000 before the timer is running). Internal helper for
 * @ref printk and @ref klogf.
 */
#define klog_stamp() \
do { \
	uint32_t _klog_ms = pit_ms(); \
	printf("[%5lu.%06lu] ", \
	       (unsigned long)(_klog_ms / 1000u), \
	       (unsigned long)((_klog_ms % 1000u) * 1000u)); \
} while (0)

/**
 * @brief printf a line to the kernel console with a leading dmesg-style
 *        timestamp.
 *
 * Use for plain operator-facing output (command results, progress) that wants
 * the ring-buffer timestamp but not the source location and severity tag that
 * @ref klogf adds.
 *
 * @param str printf format string.
 * @param ... Format arguments.
 */
#define printk(str, ...) \
do { \
	klog_stamp(); \
	printf(str, ##__VA_ARGS__); \
} while (0)

/**
 * @brief Log a printf-style message tagged with a timestamp, source location
 *        and severity.
 *
 * The line is prefixed like the Linux kernel ring buffer:
 * @code
 * [   12.345000] foo.c:42 [INFO] the message
 * @endcode
 *
 * @param prio One of the @c LOG_* levels (the "LOG_" prefix is stripped for
 *             display).
 * @param str  printf format string.
 * @param ...  Format arguments.
 */
#define klogf(prio, str, ...) \
do { \
	klog_stamp(); \
	printf("%s:%d [%s] ", __FILE__, __LINE__, #prio + 4); \
	printf(str, ##__VA_ARGS__); \
} while (0)
