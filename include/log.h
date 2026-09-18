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

/** @brief Clamp a running snprintf offset to inside a buffer of size @p n. */
#define KLOG_CLAMP(off, n) ((off) > (int)((n) - 1) ? (int)((n) - 1) : (off))

/**
 * @brief printf a line to the kernel console with a leading dmesg-style
 *        timestamp.
 *
 * The whole line is assembled in a stack buffer and emitted with a single
 * printf so it stays atomic across CPUs (see @c con_lock in printf.c).
 *
 * @param str printf format string.
 * @param ... Format arguments.
 */
#define printk(str, ...) \
do { \
	char _klbuf[256]; \
	uint32_t _klms = pit_ms(); \
	int _klo = snprintf(_klbuf, sizeof _klbuf, "[%5lu.%06lu] ", \
	                    (unsigned long)(_klms / 1000u), \
	                    (unsigned long)((_klms % 1000u) * 1000u)); \
	_klo = KLOG_CLAMP(_klo, sizeof _klbuf); \
	snprintf(_klbuf + _klo, sizeof _klbuf - _klo, str, ##__VA_ARGS__); \
	printf("%s", _klbuf); \
} while (0)

/**
 * @brief Log a printf-style message tagged with a timestamp, source location
 *        and severity.
 *
 * The line is prefixed like the Linux kernel ring buffer:
 * @code
 * [   12.345000] foo.c:42 [INFO] the message
 * @endcode
 * Assembled in a stack buffer and emitted with one printf (atomic across CPUs).
 *
 * @param prio One of the @c LOG_* levels (the "LOG_" prefix is stripped for
 *             display).
 * @param str  printf format string.
 * @param ...  Format arguments.
 */
#define klogf(prio, str, ...) \
do { \
	char _klbuf[256]; \
	uint32_t _klms = pit_ms(); \
	int _klo = snprintf(_klbuf, sizeof _klbuf, "[%5lu.%06lu] ", \
	                    (unsigned long)(_klms / 1000u), \
	                    (unsigned long)((_klms % 1000u) * 1000u)); \
	_klo = KLOG_CLAMP(_klo, sizeof _klbuf); \
	_klo += snprintf(_klbuf + _klo, sizeof _klbuf - _klo, \
	                 "%s:%d [%s] ", __FILE__, __LINE__, #prio + 4); \
	_klo = KLOG_CLAMP(_klo, sizeof _klbuf); \
	snprintf(_klbuf + _klo, sizeof _klbuf - _klo, str, ##__VA_ARGS__); \
	printf("%s", _klbuf); \
} while (0)
