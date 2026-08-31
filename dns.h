/**
 * @file dns.h
 * @brief Tiny DNS resolver — one A-record query at a time over UDP.
 */
#pragma once

#include <types.h>

/**
 * @brief Resolve @p name to IPv4 address(es) via the DHCP-supplied resolver.
 *        Runs on the `net` thread; blocks up to a few seconds.
 * @param out  Filled with up to @p max host-order addresses.
 * @return Number of addresses found (0 on failure / timeout).
 */
int dns_resolve(const char *name, uint32_t *out, int max);
