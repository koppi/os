/**
 * @file dhcp.c
 * @brief DHCP client state machine (RFC 2131), enough to lease an address from
 *        QEMU's SLIRP server and renew it.
 *
 * Timing is counted in @ref dhcp_tick calls (the `net` thread calls it about
 * every 50 ms), so no wall clock is needed. One static message buffer — only
 * the `net` thread touches it.
 */
#include <dhcp.h>
#include <net.h>

#include <log.h>
#include <rand.h>
#include <lib/string.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_MAGIC       0x63825363u

#define BOOTREQUEST 1
#define BOOTREPLY   2

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPACK      5
#define DHCPNAK      6

#define OPT_SUBNET     1
#define OPT_ROUTER     3
#define OPT_DNS        6
#define OPT_REQ_IP    50
#define OPT_LEASE     51
#define OPT_MSGTYPE   53
#define OPT_SERVER_ID 54
#define OPT_PARAM_REQ 55
#define OPT_CLIENT_ID 61
#define OPT_END      255

struct dhcp_msg {
    uint8_t  op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t cookie;
    uint8_t  options[340];
} __attribute__((packed));

enum { INIT, SELECTING, REQUESTING, BOUND, RENEWING };

static int      state;
static uint32_t xid;
static uint32_t timer;          /* ticks since dhcp_start */
static uint32_t next_action;    /* tick to (re)transmit */
static uint32_t t1;             /* tick to start renewing */
static int      retries;

static uint32_t offered_ip;
static uint32_t server_id;

static struct dhcp_msg msg;     /* assembly buffer */

static const char *state_names[] = {
    "INIT", "SELECTING", "REQUESTING", "BOUND", "RENEWING"
};
const char *dhcp_state_name(void) { return state_names[state]; }

/* ------------------------------------------------------------------ *
 *  Option helpers                                                     *
 * ------------------------------------------------------------------ */
static int opt_get(const uint8_t *opts, int len, uint8_t code,
                   uint8_t *out, int out_max) {
    int i = 0;
    while (i + 1 < len) {
        uint8_t c = opts[i];
        if (c == OPT_END)
            break;
        if (c == 0) { i++; continue; }        /* pad */
        uint8_t l = opts[i + 1];
        if (i + 2 + l > len)
            break;
        if (c == code) {
            int n = l < out_max ? l : out_max;
            memcpy(out, (void *)&opts[i + 2], n);
            return l;
        }
        i += 2 + l;
    }
    return -1;
}

static uint32_t opt_be32(const uint8_t *opts, int len, uint8_t code) {
    uint8_t v[4];
    if (opt_get(opts, len, code, v, 4) == 4)
        return ((uint32_t)v[0] << 24) | (v[1] << 16) | (v[2] << 8) | v[3];
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Send                                                               *
 * ------------------------------------------------------------------ */
static void send_msg(int type, int renew) {
    uint8_t mac[6];
    net_mac(mac);

    memset(&msg, 0, sizeof msg);
    msg.op = BOOTREQUEST;
    msg.htype = 1;
    msg.hlen = 6;
    msg.xid = xid;                              /* opaque; server echoes it */
    msg.flags = renew ? 0 : htons(0x8000);      /* broadcast replies pre-lease */
    msg.cookie = htonl(DHCP_MAGIC);
    memcpy(msg.chaddr, mac, 6);
    if (renew)
        msg.ciaddr = htonl(net_config()->ip);

    uint8_t *o = msg.options;
    int n = 0;
    o[n++] = OPT_MSGTYPE; o[n++] = 1; o[n++] = type;

    o[n++] = OPT_CLIENT_ID; o[n++] = 7; o[n++] = 1;
    memcpy(&o[n], mac, 6); n += 6;

    if (type == DHCPREQUEST && !renew) {
        o[n++] = OPT_REQ_IP; o[n++] = 4;
        o[n++] = offered_ip >> 24; o[n++] = offered_ip >> 16;
        o[n++] = offered_ip >> 8;  o[n++] = offered_ip;
        o[n++] = OPT_SERVER_ID; o[n++] = 4;
        o[n++] = server_id >> 24; o[n++] = server_id >> 16;
        o[n++] = server_id >> 8;  o[n++] = server_id;
    }

    o[n++] = OPT_PARAM_REQ; o[n++] = 4;
    o[n++] = OPT_SUBNET; o[n++] = OPT_ROUTER; o[n++] = OPT_DNS; o[n++] = OPT_LEASE;

    o[n++] = OPT_END;

    int msglen = (int)(sizeof msg - sizeof msg.options) + n;
    if (msglen < 300) {                         /* pad BOOTP to the classic min */
        memset(&o[n], 0, 300 - msglen);
        msglen = 300;
    }

    uint32_t dst = renew ? server_id : 0xFFFFFFFFu;
    udp_send(dst, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, &msg, msglen);
}

/* ------------------------------------------------------------------ *
 *  Receive                                                            *
 * ------------------------------------------------------------------ */
static void dhcp_input(uint32_t src_ip, uint16_t src_port,
                       const uint8_t *data, int len) {
    (void)src_port;
    if (len < (int)(sizeof(struct dhcp_msg) - sizeof(((struct dhcp_msg *)0)->options)))
        return;
    const struct dhcp_msg *m = (const struct dhcp_msg *)data;
    if (m->op != BOOTREPLY || m->xid != xid || ntohl(m->cookie) != DHCP_MAGIC)
        return;

    int optlen = len - (int)(sizeof(struct dhcp_msg) - sizeof m->options);
    uint8_t mt = 0;
    if (opt_get(m->options, optlen, OPT_MSGTYPE, &mt, 1) < 1)
        return;

    uint32_t sid = opt_be32(m->options, optlen, OPT_SERVER_ID);
    if (!sid)
        sid = src_ip;

    if (state == SELECTING && mt == DHCPOFFER) {
        offered_ip = ntohl(m->yiaddr);
        server_id  = sid;
        retries = 0;
        send_msg(DHCPREQUEST, 0);
        state = REQUESTING;
        next_action = timer + 40;
        return;
    }

    if ((state == REQUESTING || state == RENEWING) && mt == DHCPACK) {
        net_ipv4_t c;
        memset(&c, 0, sizeof c);
        c.ip         = ntohl(m->yiaddr);
        c.mask       = opt_be32(m->options, optlen, OPT_SUBNET);
        c.gw         = opt_be32(m->options, optlen, OPT_ROUTER);
        c.dns        = opt_be32(m->options, optlen, OPT_DNS);
        c.server     = sid;
        c.lease_secs = opt_be32(m->options, optlen, OPT_LEASE);
        if (!c.mask)
            c.mask = 0xFFFFFF00u;
        if (!c.lease_secs)
            c.lease_secs = 3600;

        net_set_config(&c);
        state = BOUND;
        retries = 0;
        t1 = timer + (c.lease_secs / 2) * 20;   /* 20 ticks/s */
        return;
    }

    if ((state == REQUESTING || state == RENEWING) && mt == DHCPNAK) {
        klogf(LOG_WARNING, "dhcp: NAK, restarting\n");
        net_clear_config();
        state = INIT;
        next_action = timer;
    }
}

/* ------------------------------------------------------------------ *
 *  State machine                                                      *
 * ------------------------------------------------------------------ */
static uint32_t backoff(int r) {
    uint32_t d = 40u << (r > 5 ? 5 : r);        /* 2s, 4s, ... cap 64s */
    return d;
}

void dhcp_start(void) {
    static int registered;
    if (!registered) {
        udp_listen(DHCP_CLIENT_PORT, dhcp_input);
        registered = 1;
    }
    xid = rand() ^ (rand() << 13) ^ 0xD4C9u;
    state = INIT;
    timer = next_action = t1 = 0;
    retries = 0;
    klogf(LOG_INFO, "dhcp: starting (xid 0x%x)\n", xid);
}

void dhcp_tick(void) {
    timer++;

    switch (state) {
    case INIT:
        send_msg(DHCPDISCOVER, 0);
        state = SELECTING;
        retries = 0;
        next_action = timer + backoff(0);
        break;

    case SELECTING:
        if (timer >= next_action) {
            send_msg(DHCPDISCOVER, 0);
            next_action = timer + backoff(++retries);
        }
        break;

    case REQUESTING:
        if (timer >= next_action) {
            if (++retries > 4) {
                state = INIT;
                next_action = timer;
            } else {
                send_msg(DHCPREQUEST, 0);
                next_action = timer + 40;
            }
        }
        break;

    case BOUND:
        if (timer >= t1) {
            klogf(LOG_INFO, "dhcp: renewing lease\n");
            send_msg(DHCPREQUEST, 1);
            state = RENEWING;
            retries = 0;
            next_action = timer + 40;
        }
        break;

    case RENEWING:
        if (timer >= next_action) {
            if (++retries > 4) {
                net_clear_config();
                state = INIT;
                next_action = timer;
            } else {
                send_msg(DHCPREQUEST, 1);
                next_action = timer + 40;
            }
        }
        break;
    }
}
