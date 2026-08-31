/**
 * @file net.h
 * @brief A very small IPv4 stack: Ethernet framing, ARP, IPv4 and UDP, driven
 *        from the `net` kernel thread on top of the polled [e1000.c](e1000.c)
 *        driver.
 *
 * IP addresses are handled in host byte order everywhere except on the wire.
 * There is one interface (the e1000); its address is learned by DHCP
 * ([dhcp.c](dhcp.c)). No fragmentation, no options, no TCP.
 */
#pragma once

#include <types.h>

/* ---- byte order (x86 is little-endian) ---- */
#define htons(x) __builtin_bswap16((uint16_t)(x))
#define ntohs(x) __builtin_bswap16((uint16_t)(x))
#define htonl(x) __builtin_bswap32((uint32_t)(x))
#define ntohl(x) __builtin_bswap32((uint32_t)(x))

/** @brief Build a host-order IPv4 address from four octets. */
#define IPV4(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IPV4  0x0800
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

/** The interface's IPv4 configuration (host byte order), filled by DHCP. */
typedef struct net_ipv4 {
    uint32_t ip;
    uint32_t mask;
    uint32_t gw;
    uint32_t dns;
    uint32_t server;      /**< DHCP server that granted the lease. */
    uint32_t lease_secs;
} net_ipv4_t;

/* ---- lower layers ---- */

/** @brief 16-bit one's-complement checksum over @p buf. */
uint16_t net_checksum(const void *buf, int len);

/** @brief Checksum over the TCP/UDP pseudo-header + @p seg (IPs in host order). */
uint16_t net_checksum_ph(uint32_t src, uint32_t dst, uint8_t proto,
                         const void *seg, int len);

/** @return This interface's IPv4 address (host order), 0 if unconfigured. */
uint32_t net_my_ip(void);

/** @brief Send an Ethernet frame (header prepended, padded to 60 bytes). */
int eth_send(const uint8_t dst_mac[6], uint16_t ethertype,
             const void *payload, int len);

/** @brief Feed one received frame to the stack (the e1000 RX callback). */
void net_input(const uint8_t *frame, uint16_t len);

/** @brief Resolve @p ip to a MAC via the ARP cache, firing a request on a miss.
 *  @return 1 and fills @p mac if known, 0 if a request was queued. */
int arp_resolve(uint32_t ip, uint8_t mac_out[6]);

/** @brief Broadcast an ARP request for @p ip. */
void arp_request(uint32_t ip);

/** @brief Send an IPv4 packet; picks the next hop and resolves its MAC.
 *  @return bytes queued, 0 if ARP is still pending, -1 on error. */
int ipv4_send(uint32_t dst_ip, uint8_t proto, const void *payload, int len);

/**
 * @brief Send a UDP datagram (checksum omitted, which IPv4 permits).
 * @param dst_ip Destination (host order); 0xFFFFFFFF broadcasts.
 */
int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
             const void *data, int len);

/** @brief Register @p fn as the handler for UDP datagrams to @p port. */
typedef void (*udp_handler_t)(uint32_t src_ip, uint16_t src_port,
                              const uint8_t *data, int len);
void udp_listen(uint16_t port, udp_handler_t fn);

/* ---- console <-> net-thread hand-off ---- */

/**
 * @brief Run @p task on the `net` thread and block until it finishes.
 *
 * The IPv4 stack is single-threaded on the `net` thread, so `ping` / `dns` /
 * `http` (which run in the console process) submit their work this way. The
 * task body may use the send helpers and call @ref net_poll in its own wait
 * loop. @return the task's return value.
 */
int net_exec(int (*task)(void));

/** @brief Drain the RX ring into the stack (call from a task's wait loop). */
void net_poll(void);

/** @return A fresh ephemeral source port (49152-65535). */
uint16_t net_ephemeral_port(void);

/* ---- interface config ---- */

/** @brief Install the address learned by DHCP and log it. */
void net_set_config(const net_ipv4_t *cfg);
/** @brief Drop the address (lease lost). */
void net_clear_config(void);
/** @return The current config (all-zero until bound). */
const net_ipv4_t *net_config(void);
/** @return Non-zero once an address is configured. */
int net_is_up(void);
/** @brief Copy the interface MAC into @p out. */
void net_mac(uint8_t out[6]);

/** @brief Format a host-order address into @p buf (>= 16 bytes). @return @p buf. */
char *net_ip_str(uint32_t ip, char *buf);

/** @brief `net` kernel-thread entry: run DHCP, then service RX + timers. */
void net_thread(void);
