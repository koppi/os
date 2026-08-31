/**
 * @file ntp.c
 * @brief Minimal SNTP client — build a mode-3 request, send it to an NTP
 *        server, wait for the reply and hand the transmit timestamp to the RTC.
 *
 * Runs entirely on the `net` thread. A single query is in flight at a time.
 */
#include <ntp.h>
#include <net.h>
#include <dns.h>
#include <rtc.h>

#include <io.h>
#include <log.h>
#include <pit.h>
#include <printf.h>
#include <lib/string.h>

#define NTP_PORT        123
#define NTP_LOCAL_PORT  50123
/** Seconds between the NTP epoch (1900-01-01) and the Unix epoch (1970-01-01). */
#define NTP_EPOCH_DELTA 2208988800u
#define NTP_SERVER_NAME "pool.ntp.org"
/** Used when DNS is unavailable: time.cloudflare.com's anycast address. */
#define NTP_FALLBACK_IP IPV4(162, 159, 200, 123)

/** RFC 5905 packet header; only @c li_vn_mode and @c tx_sec matter to us. */
struct ntp_pkt {
    uint8_t  li_vn_mode;              /**< LI (2) | VN (3) | Mode (3). */
    uint8_t  stratum;
    uint8_t  poll;
    int8_t   precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_id;
    uint32_t ref_sec,  ref_frac;
    uint32_t orig_sec, orig_frac;
    uint32_t rx_sec,   rx_frac;
    uint32_t tx_sec,   tx_frac;      /**< Server clock when it sent the reply. */
} __attribute__((packed));

static volatile int ntp_have;
static uint32_t     ntp_unix;

/* ------------------------------------------------------------------ *
 *  UDP:50123 handler                                                  *
 * ------------------------------------------------------------------ */
static void ntp_recv(uint32_t src_ip, uint16_t src_port,
                     const uint8_t *data, int len) {
    (void)src_ip;
    if (src_port != NTP_PORT || len < (int)sizeof(struct ntp_pkt))
        return;
    const struct ntp_pkt *p = (const struct ntp_pkt *)data;
    if ((p->li_vn_mode & 0x07) != 4)     /* mode 4 = server */
        return;
    uint32_t secs = ntohl(p->tx_sec);
    if (secs < NTP_EPOCH_DELTA)          /* pre-1970 / unset clock */
        return;
    ntp_unix = secs - NTP_EPOCH_DELTA;
    ntp_have = 1;
}

/* ------------------------------------------------------------------ *
 *  Sync                                                               *
 * ------------------------------------------------------------------ */
int ntp_sync(void) {
    static int registered;
    if (!registered) {
        udp_listen(NTP_LOCAL_PORT, ntp_recv);
        registered = 1;
    }

    if (!net_is_up()) {
        printf("ntp: no address (DHCP not done)\n");
        return -1;
    }

    uint32_t server;
    char s[16];
    if (dns_resolve(NTP_SERVER_NAME, &server, 1) < 1) {
        server = NTP_FALLBACK_IP;
        klogf(LOG_NOTICE, "ntp: DNS failed, using %s\n", net_ip_str(server, s));
    }

    struct ntp_pkt q;
    memset(&q, 0, sizeof q);
    q.li_vn_mode = (0 << 6) | (4 << 3) | 3;   /* LI 0, VN 4, mode 3 (client) */

    ntp_have = 0;

    for (int attempt = 0; attempt < 3 && !ntp_have; attempt++) {
        /* (Re)send, tolerating an ARP round-trip. */
        int sent = 0;
        for (int t = 0; t < 12 && !sent; t++) {
            int r = udp_send(server, NTP_LOCAL_PORT, NTP_PORT, &q, sizeof q);
            if (r > 0)       sent = 1;
            else if (r == 0) { net_poll(); sleep(40); }
            else             break;
        }
        if (!sent)
            break;

        for (int w = 0; w < 50 && !ntp_have; w++) {
            net_poll();
            sleep(40);
        }
    }

    if (!ntp_have) {
        printf("ntp: no reply from %s\n", net_ip_str(server, s));
        return -1;
    }

    rtc_set_unix(ntp_unix);

    char buf[40];
    klogf(LOG_INFO, "ntp: RTC set to %s\n", unix_to_str(ntp_unix, buf, sizeof buf));
    return 0;
}
