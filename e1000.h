/**
 * @file e1000.h
 * @brief Intel 82540EM ("e1000") gigabit NIC driver — polled, no IRQ.
 *
 * The register block (BAR0) is mapped 1:1 into the kernel address space, above
 * RAM and the low-4 MiB identity map. Descriptor rings and packet buffers are
 * 16-byte-aligned statics in `.bss`, which is identity-mapped, so `&x` is the
 * physical address handed to the card (same approach as [uhci.c](uhci.c)).
 *
 * This is just the link layer: reset, link up, read the MAC, and raw frame
 * TX/RX. The IPv4 stack and the `net` kernel thread live in [net.c](net.c),
 * which drives this through @ref e1000_send / @ref e1000_rx_poll.
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief PCI probe entry: reset, bring the link up, self-test, arm the rings. */
void e1000_probe(struct pci_device *d);

/** @return Non-zero once the NIC is initialised. */
int e1000_present(void);

/** @brief Copy the 6-byte MAC address into @p out. */
void e1000_mac(uint8_t out[6]);

/** @return Non-zero if the PHY reports link up. */
int e1000_link_up(void);

/** @brief Received / transmitted frame counters (since boot). */
uint32_t e1000_rx_count(void);
uint32_t e1000_tx_count(void);

/**
 * @brief Transmit one raw Ethernet frame (blocking, bounded wait for done).
 * @return Bytes sent, or -1 on error / no NIC.
 */
int e1000_send(const void *frame, uint16_t len);

/**
 * @brief Drain the RX ring, invoking @p cb (may be NULL) for each frame.
 * @return Number of frames processed.
 */
int e1000_rx_poll(void (*cb)(const uint8_t *frame, uint16_t len));
