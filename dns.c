/**
 * @file dns.c
 * @brief Minimal DNS client: build an A query, send it to the resolver from
 *        DHCP, wait for the reply and pull the address records out.
 *
 * Runs entirely on the `net` thread. A single query is in flight at a time.
 */
#include <dns.h>
#include <net.h>

#include <io.h>
#include <pit.h>
#include <printf.h>
#include <rand.h>
#include <lib/string.h>

#define DNS_PORT      53
#define DNS_LOCALPORT 50000
#define DNS_TYPE_A    1
#define DNS_CLASS_IN  1

struct dns_hdr {
    uint16_t id, flags, qd, an, ns, ar;
} __attribute__((packed));

static uint16_t     cur_xid;
static volatile int resp_ready;
static uint8_t      resp[512];
static int          resp_len;

/* ------------------------------------------------------------------ *
 *  UDP:50000 handler                                                  *
 * ------------------------------------------------------------------ */
static void dns_reply(uint32_t src_ip, uint16_t src_port,
                      const uint8_t *data, int len) {
    (void)src_ip;
    if (src_port != DNS_PORT || len < (int)sizeof(struct dns_hdr) ||
        len > (int)sizeof resp)
        return;
    memcpy(resp, (void *)data, len);
    resp_len = len;
    resp_ready = 1;
}

/* ------------------------------------------------------------------ *
 *  Name encoding / skipping                                           *
 * ------------------------------------------------------------------ */
/** @brief Encode @p name as DNS labels into @p out. @return bytes written, or -1. */
static int encode_name(uint8_t *out, const char *name) {
    int o = 0;
    const char *p = name;
    while (*p) {
        const char *dot = p;
        while (*dot && *dot != '.')
            dot++;
        int l = (int)(dot - p);
        if (l <= 0 || l > 63 || o + l + 1 > 255)
            return -1;
        out[o++] = (uint8_t)l;
        memcpy(out + o, (void *)p, l);
        o += l;
        p = *dot ? dot + 1 : dot;
    }
    out[o++] = 0;
    return o;
}

/** @brief Advance past a (possibly compressed) name at @p off. @return next offset, or -1. */
static int skip_name(const uint8_t *msg, int len, int off) {
    while (off < len) {
        uint8_t c = msg[off];
        if (c == 0)
            return off + 1;
        if ((c & 0xC0) == 0xC0)
            return off + 2;                 /* compression pointer */
        off += 1 + c;
    }
    return -1;
}

/* ------------------------------------------------------------------ *
 *  Resolve                                                            *
 * ------------------------------------------------------------------ */
int dns_resolve(const char *name, uint32_t *out, int max) {
    static int registered;
    if (!registered) {
        udp_listen(DNS_LOCALPORT, dns_reply);
        registered = 1;
    }

    uint32_t server = net_config()->dns;
    if (!server) {
        printf("dns: no resolver configured\n");
        return 0;
    }

    /* Build the query (net thread only, so a static buffer is fine). */
    static uint8_t q[300];
    struct dns_hdr *h = (struct dns_hdr *)q;
    cur_xid = (uint16_t)(rand() ^ pit_ms());
    h->id = htons(cur_xid);
    h->flags = htons(0x0100);              /* recursion desired */
    h->qd = htons(1);
    h->an = h->ns = h->ar = 0;

    int n = encode_name(q + sizeof *h, name);
    if (n < 0) {
        printf("dns: bad name\n");
        return 0;
    }
    int qlen = sizeof *h + n;
    q[qlen++] = 0; q[qlen++] = DNS_TYPE_A;
    q[qlen++] = 0; q[qlen++] = DNS_CLASS_IN;

    resp_ready = 0;

    int found = 0;
    for (int attempt = 0; attempt < 3 && !found; attempt++) {
        /* (Re)send, tolerating an ARP round-trip. */
        int sent = 0;
        for (int t = 0; t < 12 && !sent; t++) {
            int s = udp_send(server, DNS_LOCALPORT, DNS_PORT, q, qlen);
            if (s > 0)       sent = 1;
            else if (s == 0) { net_poll(); sleep(40); }
            else             break;
        }
        if (!sent)
            break;

        for (int w = 0; w < 60 && !resp_ready; w++) {
            net_poll();
            sleep(40);
        }
        if (!resp_ready)
            continue;

        /* Parse. */
        const uint8_t *m = resp;
        int len = resp_len;
        const struct dns_hdr *rh = (const struct dns_hdr *)m;
        if (ntohs(rh->id) != cur_xid) {
            resp_ready = 0;
            continue;
        }
        if ((ntohs(rh->flags) & 0x000F) != 0) {           /* RCODE != 0 */
            printf("dns: server returned error %d\n", ntohs(rh->flags) & 0xF);
            return 0;
        }

        int off = sizeof *rh;
        int qd = ntohs(rh->qd), an = ntohs(rh->an);
        for (int i = 0; i < qd; i++) {
            off = skip_name(m, len, off);
            if (off < 0 || off + 4 > len)
                return 0;
            off += 4;                                    /* qtype + qclass */
        }

        for (int i = 0; i < an && off + 10 <= len && found < max; i++) {
            off = skip_name(m, len, off);
            if (off < 0 || off + 10 > len)
                break;
            uint16_t type = (m[off] << 8) | m[off + 1];
            uint16_t rdl  = (m[off + 8] << 8) | m[off + 9];
            off += 10;
            if (off + rdl > len)
                break;
            if (type == DNS_TYPE_A && rdl == 4)
                out[found++] = ((uint32_t)m[off] << 24) | (m[off + 1] << 16) |
                               (m[off + 2] << 8) | m[off + 3];
            off += rdl;
        }
        return found;
    }

    printf("dns: no response from %u.%u.%u.%u\n",
           (server >> 24) & 0xFF, (server >> 16) & 0xFF,
           (server >> 8) & 0xFF, server & 0xFF);
    return 0;
}
