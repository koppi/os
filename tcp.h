/**
 * @file tcp.h
 * @brief A minimal client TCP: active open, stop-and-wait send, in-order
 *        receive. Up to @c TCP_NCONN connections. No listen/accept, no
 *        congestion control, no options beyond MSS.
 *
 * Every call blocks and must run on the `net` thread (i.e. from a
 * @ref net_exec task body).
 */
#pragma once

#include <types.h>

/** @brief Handle an inbound TCP segment (called by @ref ipv4_input). */
void tcp_input(uint32_t src_ip, const uint8_t *p, int len);

/**
 * @brief Active-open a connection to @p ip : @p port (host order / host order).
 * @return A connection handle (>= 0), or -1 on failure.
 */
int tcp_connect(uint32_t ip, uint16_t port);

/**
 * @brief Like @ref tcp_connect but bind a specific local port (0 = ephemeral).
 *        NFS uses a reserved port (< 1024) so default "secure" exports accept it.
 */
int tcp_connect_lport(uint32_t ip, uint16_t port, uint16_t lport);

/** @brief Send @p len bytes, waiting for each segment's ACK. @return bytes sent, -1 on error. */
int tcp_send(int h, const void *data, int len);

/** @brief Receive into @p buf. @return >0 bytes, 0 at clean EOF, -1 on reset/error. */
int tcp_recv(int h, void *buf, int max);

/** @brief Close the connection (FIN handshake) and free the handle. */
int tcp_close(int h);
