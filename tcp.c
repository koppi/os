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
#define TCP_NCONN     4
#define TCP_RXBUF     8192
#define TCP_MSS       1400

#define F_FIN 0x01
#define F_SYN 0x02
#define F_RST 0x04
#define F_PSH 0x08
#define F_ACK 0x10

enum {
    ST_CLOSED, ST_SYN_SENT, ST_ESTABLISHED,
    ST_FIN_WAIT_1, ST_FIN_WAIT_2, ST_CLOSING, ST_CLOSE_WAIT, ST_LAST_ACK
};

struct tcp_hdr {
    uint16_t src_port, dst_port;
    uint32_t seq, ack;
    uint8_t  off, flags;
    uint16_t window, checksum, urg;
} __attribute__((packed));

struct conn {
    int      used, state, reset;
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
static int seq_lt(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }
static int seq_le(uint32_t a, uint32_t b) { return (int32_t)(a - b) <= 0; }
static int seq_gt(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

static struct conn *find_conn(uint32_t rip, uint16_t rport, uint16_t lport) {
    for (int i = 0; i < TCP_NCONN; i++)
        if (conns[i].used && conns[i].remote_ip == rip &&
            conns[i].remote_port == rport && conns[i].local_port == lport)
            return &conns[i];
    return NULL;
}

/* ------------------------------------------------------------------ *
 *  Segment output                                                     *
 * ------------------------------------------------------------------ */
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

static void seg_ack(struct conn *c) {
    seg_send(c, F_ACK, c->snd_nxt, NULL, 0, 0);
}

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
static int alloc(void) {
    for (int i = 0; i < TCP_NCONN; i++)
        if (!conns[i].used) {
            memset(&conns[i], 0, sizeof conns[i]);
            conns[i].used = 1;
            return i;
        }
    return -1;
}

int tcp_connect(uint32_t ip, uint16_t port) {
    if (!net_is_up())
        return -1;
    int h = alloc();
    if (h < 0)
        return -1;

    struct conn *c = &conns[h];
    c->remote_ip   = ip;
    c->remote_port = port;
    c->local_port  = net_ephemeral_port();
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
    }

    for (int i = 0; i < 150 && c->state != ST_CLOSED && c->state != ST_FIN_WAIT_2 &&
                    !c->reset; i++)
        pump(20);

    c->used = 0;
    return 0;
}
