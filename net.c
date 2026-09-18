/**
 * @file net.c
 * @brief A very small IPv4 stack — Ethernet, ARP, IPv4 and UDP — plus the `net`
 *        kernel thread that runs the DHCP client and then services the RX ring.
 *
 * Everything runs on the single `net` thread, so the assembly buffers and the
 * ARP cache need no locking. IP addresses are host byte order except on the
 * wire.
 */
#include <net.h>
#include <dhcp.h>
#include <icmp.h>
#include <ntp.h>
#include <tcp.h>
#include <nfs.h>
#include <ssh.h>
#include <e1000.h>

#include <io.h>
#include <log.h>
#include <printf.h>
#include <rand.h>
#include <lib/string.h>
#include "cpu.h"

/* ------------------------------------------------------------------ *
 *  On-wire headers                                                    *
 * ------------------------------------------------------------------ */
struct eth_hdr {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;
} __attribute__((packed));

struct arp_pkt {
    uint16_t htype, ptype;
    uint8_t  hlen, plen;
    uint16_t oper;
    uint8_t  sha[6], spa[4], tha[6], tpa[4];
} __attribute__((packed));

struct ipv4_hdr {
    uint8_t  ver_ihl, tos;
    uint16_t total_len, id, frag;
    uint8_t  ttl, proto;
    uint16_t checksum;
    uint32_t src, dst;
} __attribute__((packed));

struct udp_hdr {
    uint16_t src_port, dst_port, len, checksum;
} __attribute__((packed));

/* ------------------------------------------------------------------ *
 *  State                                                              *
 * ------------------------------------------------------------------ */
static uint8_t   my_mac[6];
static net_ipv4_t cfg;
static int        configured;

static struct { uint32_t ip; uint8_t mac[6]; int valid; } arp_cache[8];
static struct { uint16_t port; udp_handler_t fn; } udp_tab[4];

static uint8_t txbuf[1518]  __attribute__((aligned(4)));
static uint8_t ipbuf[1518]  __attribute__((aligned(4)));
static uint8_t udpbuf[1518] __attribute__((aligned(4)));

/* ------------------------------------------------------------------ *
 *  Helpers                                                            *
 * ------------------------------------------------------------------ */
/** @brief memcpy from a const source; lib/string.h's memcpy takes a
 *         non-const pointer, so the cast lives here rather than at every
 *         call site. */
static void ncpy(void *d, const void *s, int n) { memcpy(d, (void *)s, n); }

/** @brief Store a host-order uint32_t as four network-order bytes. */
static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}
/** @brief Load four network-order bytes as a host-order uint32_t. */
static uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/**
 * @brief Accumulate @p len bytes into a running one's-complement sum.
 * @param sum Sum so far; 0 to start.
 * @param buf Data.
 * @param len Length in bytes; an odd length pads with a zero byte.
 * @return The updated 32-bit accumulator, to be folded by @ref csum_fold.
 *
 * The 16-bit words are added in memory order without byte-swapping. The
 * Internet checksum is endian-neutral that way: the result is stored back in
 * the same order it was summed in, so the two swaps cancel.
 */
static uint32_t csum_add(uint32_t sum, const void *buf, int len) {
    const uint16_t *p = buf;
    for (; len > 1; len -= 2)
        sum += *p++;
    if (len)
        sum += *(const uint8_t *)p;
    return sum;
}

/**
 * @brief Fold a 32-bit accumulator to 16 bits and complement it.
 * @return The value to store in a checksum field.
 */
static uint16_t csum_fold(uint32_t sum) {
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/**
 * @brief Internet checksum (RFC 1071) over one buffer.
 * @param buf Data, with its own checksum field zeroed.
 * @param len Length in bytes.
 * @return The checksum to store in that field.
 *
 * Re-running this over a buffer that already holds its checksum yields 0,
 * which is how @ref ipv4_input validates a header.
 */
uint16_t net_checksum(const void *buf, int len) {
    return csum_fold(csum_add(0, buf, len));
}

/**
 * @brief Internet checksum over a TCP/UDP segment plus its pseudo-header.
 * @param src   Source address, host order.
 * @param dst   Destination address, host order.
 * @param proto IP protocol number.
 * @param seg   The segment, with its own checksum field zeroed.
 * @param len   Segment length in bytes.
 * @return The checksum to store in the segment.
 */
uint16_t net_checksum_ph(uint32_t src, uint32_t dst, uint8_t proto,
                         const void *seg, int len) {
    /* TCP/UDP pseudo-header: src, dst (network order), 0, proto, seg length. */
    uint8_t ph[12];
    put_be32(ph + 0, src);
    put_be32(ph + 4, dst);
    ph[8]  = 0;
    ph[9]  = proto;
    ph[10] = (len >> 8) & 0xFF;
    ph[11] = len & 0xFF;
    return csum_fold(csum_add(csum_add(0, ph, 12), seg, len));
}

/**
 * @brief Format a host-order address as dotted quad.
 * @param ip The address.
 * @param b  Buffer of at least 16 bytes.
 * @return @p b, so the call can be used directly as a printf argument.
 *
 * Callers formatting several addresses in one printf need a separate buffer
 * for each: the result points into @p b, not into private storage.
 */
char *net_ip_str(uint32_t ip, char *b) {
    snprintf(b, 16, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return b;
}

/**
 * @brief Prefix length of a netmask, for "a.b.c.d/N" logging.
 * @return The number of leading 1 bits, so a non-contiguous mask counts only
 *         its leading run.
 */
static int mask_bits(uint32_t m) {
    int n = 0;
    while (m & 0x80000000u) { n++; m <<= 1; }
    return n;
}

/* ------------------------------------------------------------------ *
 *  Ethernet                                                           *
 * ------------------------------------------------------------------ */
/**
 * @brief Frame a payload and hand it to the NIC.
 * @param dst_mac Destination hardware address.
 * @param ethertype Host-order EtherType.
 * @param payload  Frame payload.
 * @param len      Payload length, 0..1500.
 * @return What @c e1000_send() returned, or -1 if @p len is out of range.
 *
 * Short frames are zero-padded to the 60-byte Ethernet minimum; the NIC
 * appends the FCS. Uses the one shared @ref txbuf, which is safe only because
 * every caller runs on the net thread.
 */
int eth_send(const uint8_t dst_mac[6], uint16_t ethertype,
             const void *payload, int len) {
    if (len < 0 || len > 1500)
        return -1;
    struct eth_hdr *e = (struct eth_hdr *)txbuf;
    ncpy(e->dst, dst_mac, 6);
    ncpy(e->src, my_mac, 6);
    e->ethertype = htons(ethertype);
    ncpy(txbuf + 14, payload, len);

    int total = 14 + len;
    if (total < 60) {
        memset(txbuf + total, 0, 60 - total);
        total = 60;
    }
    return e1000_send(txbuf, total);
}

/* ------------------------------------------------------------------ *
 *  ARP                                                                *
 * ------------------------------------------------------------------ */
/**
 * @brief Record an IP-to-MAC mapping.
 *
 * Eight entries, refreshed in place when the address is already known. There
 * is no ageing: a full cache evicts the entry at `ip % 8` — arbitrary, but
 * deterministic, and a wrongly evicted host is simply re-resolved.
 */
static void arp_cache_put(uint32_t ip, const uint8_t mac[6]) {
    if (!ip)
        return;
    int free = -1;
    for (int i = 0; i < 8; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            ncpy(arp_cache[i].mac, mac, 6);
            return;
        }
        if (!arp_cache[i].valid && free < 0)
            free = i;
    }
    if (free < 0)
        free = (int)(ip % 8);          /* evict something */
    arp_cache[free].ip = ip;
    ncpy(arp_cache[free].mac, mac, 6);
    arp_cache[free].valid = 1;
}

/**
 * @brief Look up a cached hardware address.
 * @return 1 and fills @p mac_out on a hit, 0 on a miss.
 */
static int arp_cache_get(uint32_t ip, uint8_t mac_out[6]) {
    for (int i = 0; i < 8; i++)
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            ncpy(mac_out, arp_cache[i].mac, 6);
            return 1;
        }
    return 0;
}

/**
 * @brief Send one ARP packet.
 * @param oper       1 for a request, 2 for a reply.
 * @param target_mac Ethernet destination (broadcast for a request).
 * @param tpa        Target protocol address.
 */
static void arp_send(uint16_t oper, const uint8_t target_mac[6], uint32_t tpa) {
    struct arp_pkt a;
    a.htype = htons(1);
    a.ptype = htons(ETHERTYPE_IPV4);
    a.hlen = 6;
    a.plen = 4;
    a.oper = htons(oper);
    ncpy(a.sha, my_mac, 6);
    put_be32(a.spa, cfg.ip);
    ncpy(a.tha, target_mac, 6);
    put_be32(a.tpa, tpa);
    eth_send(target_mac, ETHERTYPE_ARP, &a, sizeof a);
}

/**
 * @brief Broadcast "who has @p ip". Ignores address 0.
 *
 * Fire and forget: the answer arrives later through @ref arp_input and lands
 * in the cache. Nothing retransmits, so a lost request is retried only when
 * something asks for the address again.
 */
void arp_request(uint32_t ip) {
    static const uint8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    if (ip)
        arp_send(1, bcast, ip);
}

/**
 * @brief Resolve @p ip from the cache, starting a request if it is unknown.
 * @return 1 with @p mac_out filled, or 0 if a request has just gone out.
 *
 * Never blocks. A 0 means "ask again shortly", not failure.
 */
int arp_resolve(uint32_t ip, uint8_t mac_out[6]) {
    if (arp_cache_get(ip, mac_out))
        return 1;
    arp_request(ip);
    return 0;
}

/**
 * @brief Handle an inbound ARP packet.
 *
 * Learns the sender's mapping from every well-formed packet, requests and
 * replies alike, which is how the gateway's address usually turns up without
 * a lookup. Answers a request only once configured and only when it targets
 * this host's address.
 */
static void arp_input(const uint8_t *p, int len) {
    if (len < (int)sizeof(struct arp_pkt))
        return;
    const struct arp_pkt *a = (const struct arp_pkt *)p;
    if (ntohs(a->htype) != 1 || ntohs(a->ptype) != ETHERTYPE_IPV4)
        return;

    uint32_t spa = get_be32(a->spa);
    uint32_t tpa = get_be32(a->tpa);
    arp_cache_put(spa, a->sha);

    if (ntohs(a->oper) == 1 && configured && tpa == cfg.ip)
        arp_send(2, a->sha, spa);       /* who-has us -> reply */
}

/* ------------------------------------------------------------------ *
 *  IPv4                                                               *
 * ------------------------------------------------------------------ */
static void udp_input(uint32_t src_ip, const uint8_t *p, int len);

/**
 * @brief Send an IPv4 datagram.
 * @param dst_ip  Destination, host order; 255.255.255.255 broadcasts.
 * @param proto   IP protocol number.
 * @param payload The transport segment.
 * @param len     Payload length.
 * @return What @ref eth_send returned, -1 on a bad length or no route, and
 *         0 when the next hop's hardware address is not yet known.
 *
 * That 0 is the one to watch: nothing was sent and nothing is queued, so the
 * caller has to retry once ARP has answered. An on-subnet destination goes
 * direct, anything else via the gateway. No fragmentation: a payload that
 * does not fit the buffer is refused rather than split.
 */
int ipv4_send(uint32_t dst_ip, uint8_t proto, const void *payload, int len) {
    if (len < 0 || len + 20 > (int)sizeof ipbuf)
        return -1;

    uint8_t dmac[6];
    if (dst_ip == 0xFFFFFFFFu) {
        memset(dmac, 0xFF, 6);
    } else {
        uint32_t nh = ((dst_ip & cfg.mask) == (cfg.ip & cfg.mask) && cfg.mask)
                          ? dst_ip : cfg.gw;
        if (!nh)
            return -1;
        if (!arp_resolve(nh, dmac))
            return 0;                   /* ARP pending — caller retries */
    }

    static uint16_t ip_id = 1;
    struct ipv4_hdr *h = (struct ipv4_hdr *)ipbuf;
    h->ver_ihl   = 0x45;
    h->tos       = 0;
    h->total_len = htons(20 + len);
    h->id        = htons(ip_id++);
    h->frag      = 0;
    h->ttl       = 64;
    h->proto     = proto;
    h->checksum  = 0;
    h->src       = htonl(cfg.ip);
    h->dst       = htonl(dst_ip);
    h->checksum  = net_checksum(h, 20);
    ncpy(ipbuf + 20, payload, len);

    return eth_send(dmac, ETHERTYPE_IPV4, ipbuf, 20 + len);
}

/**
 * @brief Validate an inbound IPv4 datagram and dispatch it by protocol.
 *
 * Checks the version, the header length and the header checksum, and clamps a
 * total length that overruns the frame. Fragments are not reassembled: a
 * fragment is handed to the transport as if it were whole. The destination
 * address is not checked, so anything the NIC accepts is processed.
 */
static void ipv4_input(const uint8_t *p, int len) {
    if (len < 20)
        return;
    const struct ipv4_hdr *h = (const struct ipv4_hdr *)p;
    if ((h->ver_ihl >> 4) != 4)
        return;
    int ihl = (h->ver_ihl & 0x0F) * 4;
    if (ihl < 20 || len < ihl || net_checksum(h, ihl) != 0)
        return;

    int total = ntohs(h->total_len);
    if (total > len || total < ihl)
        total = len;

    uint32_t src = ntohl(h->src);
    const uint8_t *pl = p + ihl;
    int pllen = total - ihl;
    if (pllen < 0)
        return;

    switch (h->proto) {
    case IPPROTO_UDP:  udp_input(src, pl, pllen); break;
    case IPPROTO_ICMP: icmp_input(src, pl, pllen); break;
    case IPPROTO_TCP:  tcp_input(src, pl, pllen); break;
    }
}

/* ------------------------------------------------------------------ *
 *  UDP                                                                *
 * ------------------------------------------------------------------ */
/**
 * @brief Register a handler for a UDP port.
 *
 * Four slots, no unregister, and no check for a duplicate port: a second
 * handler for a port already claimed takes a slot it will never be called
 * from, and a fifth registration is dropped silently.
 */
void udp_listen(uint16_t port, udp_handler_t fn) {
    for (int i = 0; i < 4; i++)
        if (!udp_tab[i].fn) {
            udp_tab[i].port = port;
            udp_tab[i].fn = fn;
            return;
        }
}

/**
 * @brief Send a UDP datagram.
 * @param dst_ip   Destination, host order.
 * @param src_port Source port, host order.
 * @param dst_port Destination port, host order.
 * @param data     Payload.
 * @param len      Payload length.
 * @return As @ref ipv4_send, including the 0 that means "ARP pending, retry".
 *
 * The checksum is left zero, which IPv4 allows and means "not computed".
 */
int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
             const void *data, int len) {
    if (len < 0 || len + 8 > (int)sizeof udpbuf)
        return -1;
    struct udp_hdr *u = (struct udp_hdr *)udpbuf;
    u->src_port = htons(src_port);
    u->dst_port = htons(dst_port);
    u->len      = htons(8 + len);
    u->checksum = 0;                     /* optional in IPv4 */
    ncpy(udpbuf + 8, data, len);
    return ipv4_send(dst_ip, IPPROTO_UDP, udpbuf, 8 + len);
}

/**
 * @brief Deliver an inbound datagram to the handler registered for its port.
 *
 * First matching slot wins, and the source is not filtered — a handler sees
 * datagrams from any host, so it must check the source itself if it cares.
 * A datagram for an unclaimed port is dropped without an ICMP error.
 */
static void udp_input(uint32_t src_ip, const uint8_t *p, int len) {
    if (len < 8)
        return;
    const struct udp_hdr *u = (const struct udp_hdr *)p;
    int ulen = ntohs(u->len);
    if (ulen < 8 || ulen > len)
        ulen = len;
    uint16_t dport = ntohs(u->dst_port);
    uint16_t sport = ntohs(u->src_port);
    for (int i = 0; i < 4; i++)
        if (udp_tab[i].fn && udp_tab[i].port == dport) {
            udp_tab[i].fn(src_ip, sport, p + 8, ulen - 8);
            return;
        }
}

/* ------------------------------------------------------------------ *
 *  Dispatch + config                                                  *
 * ------------------------------------------------------------------ */
/**
 * @brief Entry point for a received frame, called from @ref net_poll.
 * @param frame The whole Ethernet frame, header included.
 * @param len   Frame length.
 *
 * Only ARP and IPv4 are recognised; anything else is dropped.
 */
void net_input(const uint8_t *frame, uint16_t len) {
    if (len < 14)
        return;
    const struct eth_hdr *e = (const struct eth_hdr *)frame;
    uint16_t et = ntohs(e->ethertype);
    if (et == ETHERTYPE_ARP)
        arp_input(frame + 14, len - 14);
    else if (et == ETHERTYPE_IPV4)
        ipv4_input(frame + 14, len - 14);
}

/** @brief Copy this host's hardware address into @p out. Zero until
 *         @ref net_thread has read it from the NIC. */
void net_mac(uint8_t out[6])            { ncpy(out, my_mac, 6); }
/** @brief The live IPv4 configuration. Not a copy: it changes under the
 *         caller when DHCP renews or the lease is dropped. */
const net_ipv4_t *net_config(void)      { return &cfg; }
/** @brief Is an address configured? @return Non-zero once DHCP has bound. */
int net_is_up(void)                     { return configured; }
/** @brief This host's address, host order, or 0 when not configured. */
uint32_t net_my_ip(void)               { return cfg.ip; }

/**
 * @brief Drop the IPv4 configuration, marking the stack down.
 *
 * Called when a DHCP lease expires or is refused. The ARP cache is left
 * alone; its entries stay valid for whatever address is configured next.
 */
void net_clear_config(void) {
    memset(&cfg, 0, sizeof cfg);
    configured = 0;
}

/**
 * @brief Adopt an IPv4 configuration and mark the stack up.
 *
 * Logs the binding and immediately ARPs the gateway, so the first packet out
 * does not have to eat the resolve round-trip and return 0 from
 * @ref ipv4_send.
 */
void net_set_config(const net_ipv4_t *c) {
    cfg = *c;
    configured = 1;

    char a[16], g[16], d[16];
    klogf(LOG_INFO, "net: %s/%d via %s, dns %s, lease %us\n",
          net_ip_str(cfg.ip, a), mask_bits(cfg.mask),
          net_ip_str(cfg.gw, g), net_ip_str(cfg.dns, d), cfg.lease_secs);

    arp_request(cfg.gw);                 /* learn the gateway's MAC */
}

/* ------------------------------------------------------------------ *
 *  Console -> net-thread task hand-off                                *
 * ------------------------------------------------------------------ */
static int (* volatile net_task)(void);
static volatile int net_task_rc;
static volatile int net_task_done;

/**
 * @brief Run @p task on the net thread and wait for its result.
 * @param task Function to run there.
 * @return Whatever @p task returned.
 *
 * The socket calls are not thread-safe and the stack's buffers are single
 * instances, so console-driven work (nfs, wget, ssh) is marshalled here
 * instead of touching them directly. One task slot: a caller arriving while
 * another task is queued sleeps until it clears. Blocks the calling thread,
 * polling every 20 ms, so it must never be called from the net thread itself.
 */
int net_exec(int (*task)(void)) {
    while (net_task)                     /* a previous task is still queued */
        sleep(20);
    net_task_done = 0;
    net_task = task;
    while (!net_task_done)
        sleep(20);
    return net_task_rc;
}

/**
 * @brief Hand out the next ephemeral source port.
 * @return A port from 49152 upward, wrapping back to 49152 at 65535.
 *
 * A plain counter: it does not check whether the port is already in use, so
 * with enough live connections it can hand out a duplicate. In practice the
 * table of open connections is far smaller than the range.
 */
uint16_t net_ephemeral_port(void) {
    static uint16_t p = 49152;
    if (++p < 49152)
        p = 49152;
    return p;
}

/* ------------------------------------------------------------------ *
 *  Thread                                                             *
 * ------------------------------------------------------------------ */
/**
 * @brief Drain the NIC's receive ring into @ref net_input.
 *
 * The one place frames enter the stack. Also called from tcp.c's wait loops
 * so a blocking read keeps the ring moving.
 */
void net_poll(void) {
    e1000_rx_poll(net_input);
}

/**
 * @brief The `net` kernel thread: bring the link up, then service it forever.
 *
 * Returns immediately — ending the thread — when there is no e1000, which is
 * what keeps a machine with no supported NIC from carrying a spinning thread.
 * Otherwise it seeds the RNG from the TSC and the MAC (so two identical VMs
 * do not pick the same DHCP XIDs and ports), starts DHCP, and then loops:
 * run any queued @ref net_exec task, poll the ring, tick DHCP, sync the RTC
 * over NTP (three attempts, then it gives up), then let NFS and SSH run their
 * periodic work. Sleeps 50 ms per pass, which sets the latency floor for
 * everything the stack does.
 */
void net_thread(void) {
    if (!e1000_present())
        return;

    e1000_mac(my_mac);
    maxrand((uint32_t)rdtsc() ^ (my_mac[4] << 8) ^ my_mac[5], 0xFFFF);

    dhcp_start();
    int ntp_left = 3;                    /* boot-time RTC sync attempts */
    while (1) {
        if (net_task) {
            net_task_rc = net_task();
            net_task = 0;
            net_task_done = 1;
        }
        net_poll();
        dhcp_tick();

        if (ntp_left && net_is_up()) {
            if (ntp_sync() == 0)
                ntp_left = 0;
            else if (--ntp_left == 0)
                klogf(LOG_WARNING, "ntp: giving up, RTC left unsynced\n");
        }

        nfs_boot_tick();
        ssh_tick();

        sleep(50);
    }
}
