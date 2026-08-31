/**
 * @file icmp.c
 * @brief ICMP echo — replies to inbound pings and a blocking ping client that
 *        runs on the `net` thread.
 */
#include <icmp.h>
#include <net.h>

#include <io.h>
#include <pit.h>
#include <printf.h>
#include <lib/string.h>

#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

struct icmp_hdr {
    uint8_t  type, code;
    uint16_t checksum;
    uint16_t id, seq;
} __attribute__((packed));

/* Where icmp_ping() waits for its reply. */
static uint16_t     exp_id, exp_seq;
static volatile int got;
static uint32_t     got_ms;

void icmp_input(uint32_t src_ip, const uint8_t *p, int len) {
    if (len < (int)sizeof(struct icmp_hdr) || net_checksum(p, len) != 0)
        return;
    const struct icmp_hdr *h = (const struct icmp_hdr *)p;

    if (h->type == ICMP_ECHO_REQUEST && net_is_up()) {
        static uint8_t reply[1500];     /* net thread only */
        if (len > (int)sizeof reply)
            len = sizeof reply;
        memcpy(reply, (void *)p, len);
        struct icmp_hdr *r = (struct icmp_hdr *)reply;
        r->type = ICMP_ECHO_REPLY;
        r->checksum = 0;
        r->checksum = net_checksum(reply, len);
        ipv4_send(src_ip, 1 /* IPPROTO_ICMP */, reply, len);
        return;
    }

    if (h->type == ICMP_ECHO_REPLY &&
        ntohs(h->id) == exp_id && ntohs(h->seq) == exp_seq) {
        got_ms = pit_ms();
        got = 1;
    }
}

int icmp_ping(uint32_t dst_ip, int count) {
    char a[16];
    net_ip_str(dst_ip, a);

    if (!net_is_up()) {
        printf("ping: no address (DHCP not done)\n");
        return 0;
    }
    printf("PING %s (32 data bytes)\n", a);

    uint16_t id = (uint16_t)(pit_ms() ^ 0x5150);
    int replies = 0;

    for (int seq = 1; seq <= count; seq++) {
        uint8_t pkt[8 + 32];
        struct icmp_hdr *h = (struct icmp_hdr *)pkt;
        h->type = ICMP_ECHO_REQUEST;
        h->code = 0;
        h->checksum = 0;
        h->id = htons(id);
        h->seq = htons(seq);
        for (int i = 0; i < 32; i++)
            pkt[8 + i] = (uint8_t)('a' + i % 26);
        h->checksum = net_checksum(pkt, sizeof pkt);

        exp_id = id;
        exp_seq = seq;
        got = 0;
        uint32_t t0 = pit_ms();

        /* Send, tolerating an ARP round-trip on the first packet. */
        int sent = 0;
        for (int t = 0; t < 12 && !sent; t++) {
            int s = ipv4_send(dst_ip, 1, pkt, sizeof pkt);
            if (s > 0)      sent = 1;
            else if (s == 0) { net_poll(); sleep(40); }   /* ARP pending */
            else            break;
        }
        if (!sent) {
            printf("  seq=%d  send failed\n", seq);
            continue;
        }

        for (int w = 0; w < 25 && !got; w++) {
            net_poll();
            sleep(40);
        }

        if (got) {
            printf("  seq=%d  time=%ums\n", seq, got_ms - t0);
            replies++;
        } else {
            printf("  seq=%d  timeout\n", seq);
        }
        sleep(300);
    }

    printf("--- %s ping: %d/%d received ---\n", a, replies, count);
    return replies;
}
