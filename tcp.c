/**
 * @file tcp.c
 * @brief Minimal client TCP — active open, stop-and-wait send, in-order
 *        receive, 1 s retransmit timer. Enough for an HTTP/1.0 GET.
 *
 * All blocking calls pump the RX ring and the retransmit timer themselves via
 * @ref net_poll, so they must run on the `net` thread.
 */
#include <tcp.h>
#include <net.h>

#include <io.h>
#include <pit.h>
#include <printf.h>
#include <lib/string.h>
#include "cpu.h"

#define IPPROTO_TCP  6
#define TCP_NCONN     6
#define TCP_RXBUF     8192
#define TCP_MSS       1400

#define F_FIN 0x01
#define F_SYN 0x02
#define F_RST 0x04
#define F_PSH 0x08
#define F_ACK 0x10

enum {
    ST_CLOSED, ST_SYN_SENT, ST_ESTABLISHED,
    ST_FIN_WAIT_1, ST_FIN_WAIT_2, ST_CLOSING, ST_CLOSE_WAIT, ST_LAST_ACK,
    ST_LISTEN, ST_SYN_RCVD
};

struct tcp_hdr {
    uint16_t src_port, dst_port;
    uint32_t seq, ack;
    uint8_t  off, flags;
    uint16_t window, checksum, urg;
} __attribute__((packed));

struct conn {
    int      used, state, reset, accepted;
    uint32_t remote_ip;
    uint16_t local_port, remote_port;
    uint32_t snd_una, snd_nxt, rcv_nxt;
    uint16_t snd_wnd;

    uint8_t  rxbuf[TCP_RXBUF];
    int      rx_head, rx_len;

    int      rt_pending, rt_len, rt_tries;
    uint8_t  rt_flags;
    uint32_t rt_seq, rt_ms;
    uint8_t  rt_data[TCP_MSS];
};

static struct conn conns[TCP_NCONN];
static uint8_t segbuf[64 + TCP_MSS] __attribute__((aligned(4)));

/* ---- sequence arithmetic (mod 2^32) ---- */
/**
 * @brief Is @p a before @p b in sequence space?
 *
 * The subtraction is done in modular arithmetic and the result read as
 * signed, so the comparison stays correct across the 2^32 wrap. It is only
 * meaningful while the two values are within 2^31 of each other, which they
 * always are here: one MSS is in flight at a time.
 */
static int seq_lt(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }
/** @brief Is @p a at or before @p b in sequence space? See @ref seq_lt. */
static int seq_le(uint32_t a, uint32_t b) { return (int32_t)(a - b) <= 0; }
/** @brief Is @p a after @p b in sequence space? See @ref seq_lt. */
static int seq_gt(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

/**
 * @brief Find the connection for an inbound segment's four-tuple.
 * @return The connection, or NULL — which is how @ref tcp_input decides
 *         between a passive open and an RST.
 */
static struct conn *find_conn(uint32_t rip, uint16_t rport, uint16_t lport) {
    for (int i = 0; i < TCP_NCONN; i++)
        if (conns[i].used && conns[i].remote_ip == rip &&
            conns[i].remote_port == rport && conns[i].local_port == lport)
            return &conns[i];
    return NULL;
}

/**
 * @brief Find a listening socket bound to @p lport.
 * @return The listener, or NULL.
 *
 * A listener has no remote end, so it is matched on the local port alone and
 * can never be found by @ref find_conn.
 */
static struct conn *find_listener(uint16_t lport) {
    for (int i = 0; i < TCP_NCONN; i++)
        if (conns[i].used && conns[i].state == ST_LISTEN && conns[i].local_port == lport)
            return &conns[i];
    return NULL;
}

static int alloc(void);

/* ------------------------------------------------------------------ *
 *  Segment output                                                     *
 * ------------------------------------------------------------------ */
/**
 * @brief Build and transmit one segment. Pure output: no state is touched.
 * @param c        The connection, for the ports, the peer and @c rcv_nxt.
 * @param flags    TCP flag bits.
 * @param seq      Sequence number to stamp on it.
 * @param data     Payload, or NULL.
 * @param dlen     Payload length.
 * @param with_mss Non-zero to append the MSS option, which only a SYN wants.
 *
 * The advertised window is whatever is free in the receive buffer, so a
 * consumer that stops reading closes the window and the peer stalls rather
 * than overruns. Nothing here advances @c snd_nxt or arms the retransmit
 * timer — use @ref seg_send_tracked for a segment that consumes sequence
 * space. Writes through the one shared @ref segbuf, which is safe only
 * because the whole stack runs on the net thread.
 */
static void seg_send(struct conn *c, uint8_t flags, uint32_t seq,
                     const void *data, int dlen, int with_mss) {
    struct tcp_hdr *h = (struct tcp_hdr *)segbuf;
    int hlen = 20 + (with_mss ? 4 : 0);

    h->src_port = htons(c->local_port);
    h->dst_port = htons(c->remote_port);
    h->seq      = htonl(seq);
    h->ack      = htonl(c->rcv_nxt);
    h->off      = (hlen / 4) << 4;
    h->flags    = flags;
    h->window   = htons(TCP_RXBUF - c->rx_len);
    h->checksum = 0;
    h->urg      = 0;

    if (with_mss) {
        segbuf[20] = 2; segbuf[21] = 4;
        segbuf[22] = TCP_MSS >> 8; segbuf[23] = TCP_MSS & 0xFF;
    }
    if (dlen > 0)
        memcpy(segbuf + hlen, (void *)data, dlen);

    h->checksum = net_checksum_ph(net_my_ip(), c->remote_ip, IPPROTO_TCP,
                                  segbuf, hlen + dlen);
    ipv4_send(c->remote_ip, IPPROTO_TCP, segbuf, hlen + dlen);
}

/** @brief Send a segment that consumes sequence space and arm the RT timer. */
static void seg_send_tracked(struct conn *c, uint8_t flags,
                             const void *data, int dlen, int with_mss) {
    uint32_t seq = c->snd_nxt;
    seg_send(c, flags, seq, data, dlen, with_mss);

    c->rt_pending = 1;
    c->rt_flags   = flags;              /* retransmit without the MSS option */
    c->rt_seq     = seq;
    c->rt_len     = dlen;
    if (dlen > 0)
        memcpy(c->rt_data, (void *)data, dlen);
    c->rt_ms    = pit_ms();
    c->rt_tries = 0;

    c->snd_nxt += dlen;
    if (flags & F_SYN) c->snd_nxt++;
    if (flags & F_FIN) c->snd_nxt++;
}

/**
 * @brief Send a bare ACK.
 *
 * Carries no data and consumes no sequence space, so it is never tracked for
 * retransmission: if it is lost, the peer's own retransmit produces another.
 */
static void seg_ack(struct conn *c) {
    seg_send(c, F_ACK, c->snd_nxt, NULL, 0, 0);
}

/**
 * @brief Send an RST to a peer this host has no connection with.
 * @param rip   Remote address.
 * @param rport Remote port.
 * @param lport Local port the segment was addressed to.
 * @param seq   Sequence number for the RST: the segment's ACK field if it had
 *              one, otherwise 0.
 *
 * Takes no @c conn because there is none — this is the reply to a segment
 * that matched neither a connection nor a listener.
 */
static void seg_rst(uint32_t rip, uint16_t rport, uint16_t lport, uint32_t seq) {
    struct tcp_hdr *h = (struct tcp_hdr *)segbuf;
    memset(h, 0, 20);
    h->src_port = htons(lport);
    h->dst_port = htons(rport);
    h->seq      = htonl(seq);
    h->off      = 5 << 4;
    h->flags    = F_RST | F_ACK;
    h->checksum = net_checksum_ph(net_my_ip(), rip, IPPROTO_TCP, segbuf, 20);
    ipv4_send(rip, IPPROTO_TCP, segbuf, 20);
}

/* ------------------------------------------------------------------ *
 *  Retransmit timer + pump                                            *
 * ------------------------------------------------------------------ */
/**
 * @brief Retransmit any segment unacknowledged for more than a second.
 *
 * A flat 1 s timer with no backoff and no RTT estimate, and six attempts
 * before the connection is marked reset — about seven seconds to notice a
 * dead peer. A retransmitted SYN goes out bare: the original carried the MSS
 * option and no ACK, and repeating the option is pointless while an ACK on a
 * SYN would make it a SYN-ACK.
 */
static void tcp_retransmit(void) {
    for (int i = 0; i < TCP_NCONN; i++) {
        struct conn *c = &conns[i];
        if (!c->used || !c->rt_pending)
            continue;
        if (pit_ms() - c->rt_ms < 1000)
            continue;
        if (++c->rt_tries > 6) {
            c->reset = 1;
            c->state = ST_CLOSED;
            c->rt_pending = 0;
            continue;
        }
        /* A retransmitted SYN carries no ACK; everything else does. */
        uint8_t f = (c->rt_flags & F_SYN) ? F_SYN : (c->rt_flags | F_ACK);
        seg_send(c, f, c->rt_seq, c->rt_len ? c->rt_data : NULL, c->rt_len, 0);
        c->rt_ms = pit_ms();
    }
}

/**
 * @brief Drive the stack for @p ms milliseconds: poll the ring, run the timer.
 *
 * The primitive under every blocking call in this file. Because the loop is
 * do-while it always makes one pass, so @c pump(0) still services the ring
 * once. Sleeps 20 ms per pass, so @p ms is a floor rather than a deadline,
 * and it must only ever run on the net thread.
 */
static void pump(int ms) {
    uint32_t start = pit_ms();
    do {
        net_poll();
        tcp_retransmit();
        sleep(20);
    } while ((uint32_t)(pit_ms() - start) < (uint32_t)ms);
}

/* ------------------------------------------------------------------ *
 *  Input                                                              *
 * ------------------------------------------------------------------ */
/**
 * @brief Process one inbound TCP segment: the whole state machine.
 * @param src_ip Source address, host order.
 * @param p      The segment, header included.
 * @param len    Segment length.
 *
 * Validates the checksum and data offset, then:
 *   - no matching connection: a bare SYN to a listening port opens a new
 *     connection passively, anything else earns an RST;
 *   - RST marks the connection reset and closed;
 *   - in SYN_SENT, a SYN-ACK acknowledging our SYN establishes it;
 *   - an ACK advances @c snd_una, clears the retransmit timer once it covers
 *     the tracked segment, and drives the closing states forward;
 *   - data is accepted only at @c rcv_nxt and only as far as the receive
 *     buffer has room, and @c rcv_nxt advances by what was actually buffered.
 *     An out-of-order or window-overflowing segment is dropped, but still
 *     ACKed: that duplicate ACK is what prompts the peer to retransmit.
 *   - a FIN ending exactly at @c rcv_nxt is acknowledged and moves the state
 *     to CLOSE_WAIT, CLOSING or CLOSED.
 *
 * There is no reassembly queue and no TIME_WAIT: a closed connection's slot
 * is reused at once, which is why callers that reconnect quickly should vary
 * their local port (nfs.c does).
 */
void tcp_input(uint32_t src_ip, const uint8_t *p, int len) {
    if (len < 20)
        return;
    if (net_checksum_ph(src_ip, net_my_ip(), IPPROTO_TCP, p, len) != 0)
        return;

    const struct tcp_hdr *h = (const struct tcp_hdr *)p;
    int doff = (h->off >> 4) * 4;
    if (doff < 20 || doff > len)
        return;

    uint16_t sport = ntohs(h->src_port);
    uint16_t dport = ntohs(h->dst_port);
    uint32_t seq   = ntohl(h->seq);
    uint32_t ack   = ntohl(h->ack);
    uint8_t  fl    = h->flags;
    const uint8_t *data = p + doff;
    int dlen = len - doff;

    struct conn *c = find_conn(src_ip, sport, dport);
    if (!c) {
        if ((fl & (F_SYN | F_ACK)) == F_SYN && find_listener(dport)) {
            int h = alloc();
            if (h >= 0) {
                struct conn *nc = &conns[h];
                nc->remote_ip   = src_ip;
                nc->remote_port = sport;
                nc->local_port  = dport;
                nc->rcv_nxt     = seq + 1;
                uint32_t isn    = (uint32_t)rdtsc();
                nc->snd_una = nc->snd_nxt = isn;
                nc->state = ST_SYN_RCVD;
                seg_send_tracked(nc, F_SYN | F_ACK, NULL, 0, 1);
            }
            return;
        }
        if (!(fl & F_RST))
            seg_rst(src_ip, sport, dport, (fl & F_ACK) ? ack : 0);
        return;
    }

    if (fl & F_RST) {
        c->reset = 1;
        c->state = ST_CLOSED;
        c->rt_pending = 0;
        return;
    }

    if (c->state == ST_SYN_SENT) {
        if ((fl & (F_SYN | F_ACK)) == (F_SYN | F_ACK) && ack == c->snd_nxt) {
            c->rcv_nxt = seq + 1;
            c->snd_una = ack;
            c->snd_wnd = ntohs(h->window);
            c->rt_pending = 0;
            c->state = ST_ESTABLISHED;
            seg_ack(c);
        }
        return;
    }

    /* ACK processing (common to the established / closing states). */
    if (fl & F_ACK) {
        if (seq_gt(ack, c->snd_una) && seq_le(ack, c->snd_nxt))
            c->snd_una = ack;
        if (c->rt_pending) {
            uint32_t end = c->rt_seq + c->rt_len +
                           ((c->rt_flags & F_SYN) ? 1 : 0) +
                           ((c->rt_flags & F_FIN) ? 1 : 0);
            if (seq_le(end, c->snd_una))
                c->rt_pending = 0;
        }
        if (c->state == ST_FIN_WAIT_1 && !c->rt_pending)
            c->state = ST_FIN_WAIT_2;
        else if (c->state == ST_CLOSING && !c->rt_pending)
            c->state = ST_CLOSED;
        else if (c->state == ST_LAST_ACK && !c->rt_pending)
            c->state = ST_CLOSED;
        else if (c->state == ST_SYN_RCVD && !c->rt_pending)
            c->state = ST_ESTABLISHED;
    }

    /* In-order data. */
    if (dlen > 0) {
        if (seq == c->rcv_nxt) {
            int space = TCP_RXBUF - c->rx_len;
            int n = dlen < space ? dlen : space;
            for (int i = 0; i < n; i++)
                c->rxbuf[(c->rx_head + c->rx_len + i) % TCP_RXBUF] = data[i];
            c->rx_len += n;
            c->rcv_nxt += n;                 /* only ACK what we buffered */
        }
        seg_ack(c);                          /* also nudges a retransmit if OOO */
    }

    /* FIN. */
    if ((fl & F_FIN) && seq_le(seq, c->rcv_nxt) && seq + dlen == c->rcv_nxt) {
        c->rcv_nxt++;
        seg_ack(c);
        if (c->state == ST_ESTABLISHED)      c->state = ST_CLOSE_WAIT;
        else if (c->state == ST_FIN_WAIT_1)  c->state = ST_CLOSING;
        else if (c->state == ST_FIN_WAIT_2)  c->state = ST_CLOSED;
    }
}

/* ------------------------------------------------------------------ *
 *  Public API                                                         *
 * ------------------------------------------------------------------ */
/**
 * @brief Claim a free connection slot, zeroed.
 * @return The slot index, or -1 when all @ref TCP_NCONN are in use.
 *
 * Each slot carries its own 8 KiB receive buffer and a retransmit copy, so
 * the table is the largest thing this file puts in BSS — which is why there
 * are six slots and not sixty. A listener occupies one of them.
 */
static int alloc(void) {
    for (int i = 0; i < TCP_NCONN; i++)
        if (!conns[i].used) {
            memset(&conns[i], 0, sizeof conns[i]);
            conns[i].used = 1;
            return i;
        }
    return -1;
}

/**
 * @brief Open a listening socket.
 * @param port Local port, host order.
 * @return A handle for @ref tcp_accept, or -1 if no slot is free.
 *
 * The listener holds a slot of its own for as long as it exists, leaving that
 * many fewer for the connections it accepts.
 */
int tcp_listen(uint16_t port) {
    int h = alloc();
    if (h < 0)
        return -1;
    conns[h].state = ST_LISTEN;
    conns[h].local_port = port;
    return h;
}

/**
 * @brief Take the next connection established on a listener's port.
 * @param listen_h Handle from @ref tcp_listen.
 * @return A connection handle, or -1 if none is waiting.
 *
 * Does not block and does not pump the stack, so a caller polls it from its
 * own loop. A -1 is the normal "nothing yet", not an error — the handshake
 * itself is completed by @ref tcp_input as segments arrive.
 */
int tcp_accept(int listen_h) {
    if (listen_h < 0 || listen_h >= TCP_NCONN || !conns[listen_h].used ||
        conns[listen_h].state != ST_LISTEN)
        return -1;
    uint16_t port = conns[listen_h].local_port;
    for (int i = 0; i < TCP_NCONN; i++) {
        struct conn *c = &conns[i];
        if (c->used && c->state == ST_ESTABLISHED && c->local_port == port && !c->accepted) {
            c->accepted = 1;
            return i;
        }
    }
    return -1;
}

/**
 * @brief Can more data still arrive on this connection?
 * @return Non-zero only in ESTABLISHED.
 *
 * CLOSE_WAIT is deliberately excluded: the peer has sent its FIN, so no
 * further data is coming even though this end may still be able to write.
 */
int tcp_is_open(int h) {
    if (h < 0 || h >= TCP_NCONN || !conns[h].used || conns[h].reset)
        return 0;
    /* ST_CLOSE_WAIT deliberately excluded: the peer has sent FIN, so no more
     * data will ever arrive, even though we may still be able to write. */
    return conns[h].state == ST_ESTABLISHED;
}

/**
 * @brief Active open from an ephemeral local port.
 * @param ip   Remote address, host order.
 * @param port Remote port.
 * @return A connection handle, or -1.
 */
int tcp_connect(uint32_t ip, uint16_t port) {
    return tcp_connect_lport(ip, port, 0);
}

/**
 * @brief Active open, optionally from a chosen local port.
 * @param ip    Remote address, host order.
 * @param port  Remote port.
 * @param lport Local port, or 0 for an ephemeral one.
 * @return A connection handle, or -1 if the link is down, no slot is free, or
 *         the handshake did not complete.
 *
 * Blocks, pumping the stack, for up to about six seconds. The explicit local
 * port exists for servers that insist on a reserved source port; nfs.c uses
 * it, and varies the port precisely because this stack keeps no TIME_WAIT.
 * A failed open releases its slot.
 */
int tcp_connect_lport(uint32_t ip, uint16_t port, uint16_t lport) {
    if (!net_is_up())
        return -1;
    int h = alloc();
    if (h < 0)
        return -1;

    struct conn *c = &conns[h];
    c->remote_ip   = ip;
    c->remote_port = port;
    c->local_port  = lport ? lport : net_ephemeral_port();
    uint32_t isn   = (uint32_t)rdtsc();
    c->snd_una = c->snd_nxt = isn;
    c->state   = ST_SYN_SENT;

    seg_send_tracked(c, F_SYN, NULL, 0, 1);

    for (int i = 0; i < 300 && c->state == ST_SYN_SENT && !c->reset; i++)
        pump(20);

    if (c->state != ST_ESTABLISHED) {
        c->used = 0;
        return -1;
    }
    return h;
}

/**
 * @brief Send data, waiting for each piece to be acknowledged.
 * @param h    Connection handle.
 * @param data Bytes to send.
 * @param len  Length.
 * @return Bytes actually sent — which may be fewer than @p len — or -1 if
 *         nothing at all went out.
 *
 * Stop-and-wait: one MSS in flight at a time, each chunk waited on for about
 * six seconds before giving up and returning short. Callers must check the
 * count rather than assume the whole buffer went. Sending continues in
 * CLOSE_WAIT, since a peer that has finished sending can still receive.
 */
int tcp_send(int h, const void *data, int len) {
    if (h < 0 || h >= TCP_NCONN || !conns[h].used)
        return -1;
    struct conn *c = &conns[h];
    const uint8_t *d = data;
    int off = 0;

    while (off < len) {
        if (c->reset || (c->state != ST_ESTABLISHED && c->state != ST_CLOSE_WAIT))
            return off ? off : -1;

        int chunk = len - off;
        if (chunk > TCP_MSS)
            chunk = TCP_MSS;

        seg_send_tracked(c, F_PSH | F_ACK, d + off, chunk, 0);
        uint32_t want = c->snd_nxt;
        for (int i = 0; i < 300 && seq_lt(c->snd_una, want) && !c->reset; i++)
            pump(20);

        if (c->reset)
            return off ? off : -1;
        if (seq_lt(c->snd_una, want))
            return off;                     /* gave up on this chunk */
        off += chunk;
    }
    return off;
}

/**
 * @brief Take whatever is already buffered, without waiting.
 * @param h   Connection handle.
 * @param buf Destination.
 * @param max Capacity of @p buf.
 * @return Bytes copied, 0 if nothing has arrived yet, -1 on a bad handle or
 *         a reset connection.
 *
 * A 0 means "not yet", never end of stream, and the caller is responsible for
 * pumping the stack — this neither polls the ring nor runs the timer.
 */
int tcp_recv_nb(int h, void *buf, int max) {
    if (h < 0 || h >= TCP_NCONN || !conns[h].used)
        return -1;
    struct conn *c = &conns[h];
    if (c->reset)
        return -1;
    if (c->rx_len == 0)
        return 0;

    int n = c->rx_len < max ? c->rx_len : max;
    uint8_t *o = buf;
    for (int i = 0; i < n; i++)
        o[i] = c->rxbuf[(c->rx_head + i) % TCP_RXBUF];
    c->rx_head = (c->rx_head + n) % TCP_RXBUF;
    c->rx_len -= n;
    return n;
}

/**
 * @brief Wait for data and copy it out.
 * @param h   Connection handle.
 * @param buf Destination.
 * @param max Capacity of @p buf.
 * @return Bytes copied, 0, or -1 on a bad handle or a reset connection.
 *
 * Pumps the stack for up to about ten seconds, returning early once the peer
 * closes. The 0 is ambiguous — end of stream and "nothing arrived in ten
 * seconds" look the same — so a caller reading to completion should treat 0
 * as the end, as nfs.c and wget do, and rely on the -1 for a real failure.
 */
int tcp_recv(int h, void *buf, int max) {
    if (h < 0 || h >= TCP_NCONN || !conns[h].used)
        return -1;
    struct conn *c = &conns[h];

    for (int i = 0; i < 500 && c->rx_len == 0; i++) {
        if (c->reset)
            return -1;
        if (c->state == ST_CLOSE_WAIT || c->state == ST_CLOSED ||
            c->state == ST_LAST_ACK || c->state == ST_CLOSING)
            break;
        pump(20);
    }
    if (c->rx_len == 0)
        return c->reset ? -1 : 0;

    int n = c->rx_len < max ? c->rx_len : max;
    uint8_t *o = buf;
    for (int i = 0; i < n; i++)
        o[i] = c->rxbuf[(c->rx_head + i) % TCP_RXBUF];
    c->rx_head = (c->rx_head + n) % TCP_RXBUF;
    c->rx_len -= n;
    return n;
}

/**
 * @brief Close a connection and release its slot.
 * @param h Connection handle.
 * @return 0, or -1 for a handle that was not open.
 *
 * Sends a FIN from ESTABLISHED or CLOSE_WAIT and an RST from a half-open
 * handshake, then pumps for up to about three seconds for the shutdown to
 * settle. The slot is freed either way: there is no TIME_WAIT, so a segment
 * still in flight for this four-tuple could be delivered to whatever reuses
 * the slot next.
 */
int tcp_close(int h) {
    if (h < 0 || h >= TCP_NCONN || !conns[h].used)
        return -1;
    struct conn *c = &conns[h];

    if (c->state == ST_ESTABLISHED) {
        seg_send_tracked(c, F_FIN | F_ACK, NULL, 0, 0);
        c->state = ST_FIN_WAIT_1;
    } else if (c->state == ST_CLOSE_WAIT) {
        seg_send_tracked(c, F_FIN | F_ACK, NULL, 0, 0);
        c->state = ST_LAST_ACK;
    } else if (c->state == ST_SYN_RCVD || c->state == ST_SYN_SENT) {
        seg_rst(c->remote_ip, c->remote_port, c->local_port, 0);
        c->state = ST_CLOSED;
        c->rt_pending = 0;
    }

    for (int i = 0; i < 150 && c->state != ST_CLOSED && c->state != ST_FIN_WAIT_2 &&
                    !c->reset; i++)
        pump(20);

    c->used = 0;
    return 0;
}
