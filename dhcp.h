/**
 * @file dhcp.h
 * @brief Minimal DHCP client: the DISCOVER / OFFER / REQUEST / ACK handshake
 *        plus a basic lease renewal at T1, driven by the `net` thread.
 */
#pragma once

#include <types.h>

/** @brief Reset to INIT and register the UDP:68 handler. */
void dhcp_start(void);

/** @brief Advance the state machine. Call at a steady cadence (~50 ms). */
void dhcp_tick(void);

/** @return A short name for the current state ("INIT", "BOUND", ...). */
const char *dhcp_state_name(void);
