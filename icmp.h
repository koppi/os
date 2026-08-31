/**
 * @file icmp.h
 * @brief ICMP echo: reply to incoming pings and a small ping client.
 */
#pragma once

#include <types.h>

/** @brief Handle an ICMP message (called by @ref ipv4_input). */
void icmp_input(uint32_t src_ip, const uint8_t *p, int len);

/**
 * @brief Send @p count echo requests to @p dst_ip, printing each result and a
 *        summary. Runs on the `net` thread (via @ref net_exec).
 * @return Number of replies received.
 */
int icmp_ping(uint32_t dst_ip, int count);
