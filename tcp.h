/**
 * @file tcp.h
 * @brief A minimal TCP: active open, passive open (listen/accept), stop-
 *        and-wait send, in-order receive. Up to @c TCP_NCONN connections, no
 *        congestion control, no options beyond MSS.
 *
 * Every call blocks (except @ref tcp_accept and @ref tcp_recv_nb, which poll
 * once and return) and must run on the `net` thread (i.e. from a
 * @ref net_exec task body, or a tick function called directly from
 * @ref net_thread like @ref nfs_boot_tick).
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

/**
 * @brief Passive-open: start listening on @p port (host order).
 * @return A listener handle (>= 0), or -1 if the connection table is full.
 */
int tcp_listen(uint16_t port);

/**
 * @brief Poll a listener for a completed inbound handshake.
 * @return A new, established connection handle (>= 0), or -1 if none is
 *         waiting. Never blocks.
 */
int tcp_accept(int listen_h);

/** @return Non-zero if @p h is established and more data could still
 *  arrive from the peer (false once the peer has sent FIN, even though we
 *  may still be able to write). */
int tcp_is_open(int h);

/** @brief Send @p len bytes, waiting for each segment's ACK. @return bytes sent, -1 on error. */
int tcp_send(int h, const void *data, int len);

/** @brief Receive into @p buf. @return >0 bytes, 0 at clean EOF, -1 on reset/error. */
int tcp_recv(int h, void *buf, int max);

/**
 * @brief Like @ref tcp_recv but never blocks: drains only what has already
 *        arrived. @return >0 bytes, 0 if none available right now, -1 on
 *        reset/error.
 */
int tcp_recv_nb(int h, void *buf, int max);

/** @brief Close the connection (FIN handshake) and free the handle. */
int tcp_close(int h);
