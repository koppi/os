/**
 * @file ntp.h
 * @brief Minimal SNTP client: one mode-3 request to an NTP server over UDP,
 *        then set the CMOS RTC from the transmit timestamp.
 */
#pragma once

#include <types.h>

/**
 * @brief Query an NTP server and set the RTC to the answer (UTC).
 *
 * Resolves @c pool.ntp.org (falling back to a fixed address if DNS fails),
 * sends one client request to UDP:123 and waits a few seconds for the reply.
 * The server's transmit timestamp is converted to Unix time and written to the
 * CMOS clock with @ref rtc_set_unix.
 *
 * Runs on the `net` thread: it is called from @ref net_thread once DHCP has a
 * lease, and by the `ntpdate` console command through @ref net_exec.
 *
 * @return 0 on success, -1 on failure (no lease, no server, no reply).
 */
int ntp_sync(void);
