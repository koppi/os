/**
 * @file e1000.c
 * @brief Intel 82540EM ("e1000") gigabit NIC driver: polled, MMIO register
 *        access, static descriptor rings, a MAC-loopback self-test and RX
 *        frame counting.
 */
#include <e1000.h>
#include <pci.h>

#include <io.h>
#include <cmdline.h>
#include <log.h>
#include <paging.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ *
 *  Register offsets                                                   *
 * ------------------------------------------------------------------ */
#define E1000_CTRL     0x0000
#define E1000_STATUS   0x0008
#define E1000_CTRL_EXT 0x0018
#define E1000_EERD     0x0014
#define E1000_MDIC     0x0020
#define E1000_ICR      0x00C0
#define E1000_IMS      0x00D0
#define E1000_IMC      0x00D8
#define E1000_RCTL     0x0100
#define E1000_TCTL     0x0400
#define E1000_TIPG     0x0410
#define E1000_RDBAL    0x2800
#define E1000_RDBAH    0x2804
#define E1000_RDLEN    0x2808
#define E1000_RDH      0x2810
#define E1000_RDT      0x2818
#define E1000_TDBAL    0x3800
#define E1000_TDBAH    0x3804
#define E1000_TDLEN    0x3808
#define E1000_TDH      0x3810
#define E1000_TDT      0x3818
#define E1000_MTA      0x5200
#define E1000_RAL0     0x5400
#define E1000_RAH0     0x5404

#define CTRL_FD        (1u << 0)
#define CTRL_ASDE      (1u << 5)
#define CTRL_SLU       (1u << 6)
#define CTRL_RST       (1u << 26)

#define STATUS_LU      (1u << 1)

#define EERD_START     0x01
#define EERD_DONE      0x10

#define MDIC_READY     (1u << 28)
#define MDIC_OP_READ   (2u << 26)
#define MDIC_PHY1      (1u << 21)   /* PHY address 1 */
#define PHY_BMSR       1            /* PHY status register */
#define BMSR_LINK      0x0004       /* link established */

#define RCTL_EN        (1u << 1)
#define RCTL_SBP       (1u << 2)   /* store bad packets */
#define RCTL_UPE       (1u << 3)   /* unicast promiscuous */
#define RCTL_MPE       (1u << 4)   /* multicast promiscuous */
#define RCTL_BAM       (1u << 15)  /* broadcast accept */
#define RCTL_BSIZE_2048 0          /* [17:16] = 00 */
#define RCTL_SECRC     (1u << 26)  /* strip the Ethernet CRC */

#define TCTL_EN        (1u << 1)
#define TCTL_PSP       (1u << 3)   /* pad short packets */
#define TCTL_CT_SHIFT  4
#define TCTL_COLD_SHIFT 12

#define TXD_CMD_EOP    (1u << 0)
#define TXD_CMD_IFCS   (1u << 1)
#define TXD_CMD_RS     (1u << 3)
#define TXD_STAT_DD    (1u << 0)

#define RXD_STAT_DD    (1u << 0)
#define RXD_STAT_EOP   (1u << 1)

/* ------------------------------------------------------------------ *
 *  Descriptor rings + buffers (identity-mapped .bss)                  *
 * ------------------------------------------------------------------ */
#define NRX       16
#define NTX       8
#define BUF_SIZE  2048

struct rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t  status;
    volatile uint8_t  errors;
    volatile uint16_t special;
} __attribute__((packed));

struct tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t  cso;
    volatile uint8_t  cmd;
    volatile uint8_t  status;
    volatile uint8_t  css;
    volatile uint16_t special;
} __attribute__((packed));

static struct rx_desc rx_ring[NRX] __attribute__((aligned(4096)));
static struct tx_desc tx_ring[NTX] __attribute__((aligned(4096)));
static uint8_t rx_buf[NRX][BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buf[NTX][BUF_SIZE] __attribute__((aligned(16)));

static volatile uint8_t *mmio;   /* mapped BAR0 (identity phys==virt) */
static int      have_nic;
static uint8_t  mac[6];
static int      rx_cur, tx_cur;
static uint32_t rx_frames, tx_frames;

/* ------------------------------------------------------------------ *
 *  Register / EEPROM helpers                                          *
 * ------------------------------------------------------------------ */
static void     io_wait(void)            { inportb(0x80); }
static uint32_t reg_rd(uint32_t r)       { return *(volatile uint32_t *)(mmio + r); }
static void     reg_wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

static void spin(int loops) { for (int i = 0; i < loops; i++) io_wait(); }

/** @brief Read EEPROM word @p addr. @return 1 on success, 0 on timeout. */
static int eeprom_read(uint8_t addr, uint16_t *out) {
    reg_wr(E1000_EERD, ((uint32_t)addr << 8) | EERD_START);
    for (int i = 0; i < 100000; i++) {
        uint32_t v = reg_rd(E1000_EERD);
        if (v & EERD_DONE) {
            *out = v >> 16;
            return 1;
        }
        io_wait();
    }
    return 0;
}

/** @brief Read an internal PHY register through MDIC. */
static uint16_t phy_read(uint8_t reg) {
    reg_wr(E1000_MDIC, ((uint32_t)reg << 16) | MDIC_PHY1 | MDIC_OP_READ);
    for (int i = 0; i < 10000; i++) {
        uint32_t v = reg_rd(E1000_MDIC);
        if (v & MDIC_READY)
            return v & 0xFFFF;
        io_wait();
    }
    return 0;
}

static int mac_from_eeprom = 1;   /* cleared for parts with no EERD interface */

static void read_mac(void) {
    uint16_t w0, w1, w2;
    if (mac_from_eeprom &&
        eeprom_read(0, &w0) && eeprom_read(1, &w1) && eeprom_read(2, &w2)) {
        mac[0] = w0;      mac[1] = w0 >> 8;
        mac[2] = w1;      mac[3] = w1 >> 8;
        mac[4] = w2;      mac[5] = w2 >> 8;
    } else {
        uint32_t ral = reg_rd(E1000_RAL0), rah = reg_rd(E1000_RAH0);
        mac[0] = ral;       mac[1] = ral >> 8;
        mac[2] = ral >> 16; mac[3] = ral >> 24;
        mac[4] = rah;       mac[5] = rah >> 8;
    }
}

/* ------------------------------------------------------------------ *
 *  Ring setup                                                         *
 * ------------------------------------------------------------------ */
static void rx_init(void) {
    memset(rx_ring, 0, sizeof(rx_ring));
    for (int i = 0; i < NRX; i++)
        rx_ring[i].addr = (uint32_t)&rx_buf[i][0];

    reg_wr(E1000_RDBAL, (uint32_t)&rx_ring[0]);
    reg_wr(E1000_RDBAH, 0);
    reg_wr(E1000_RDLEN, sizeof(rx_ring));
    reg_wr(E1000_RDH, 0);
    reg_wr(E1000_RDT, NRX - 1);
    rx_cur = 0;
    /* Accept unicast (matched against RAL0), broadcast (ARP, DHCP offers) and
     * strip the CRC. Not promiscuous — the stack only wants its own traffic. */
    reg_wr(E1000_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048);
}

static void tx_init(void) {
    memset(tx_ring, 0, sizeof(tx_ring));
    reg_wr(E1000_TDBAL, (uint32_t)&tx_ring[0]);
    reg_wr(E1000_TDBAH, 0);
    reg_wr(E1000_TDLEN, sizeof(tx_ring));
    reg_wr(E1000_TDH, 0);
    reg_wr(E1000_TDT, 0);
    tx_cur = 0;
    reg_wr(E1000_TIPG, 0x0060200A);
    reg_wr(E1000_TCTL, TCTL_EN | TCTL_PSP |
                       (0x0F << TCTL_CT_SHIFT) | (0x40 << TCTL_COLD_SHIFT));
}

/* ------------------------------------------------------------------ *
 *  Public TX / RX                                                     *
 * ------------------------------------------------------------------ */
int e1000_send(const void *frame, uint16_t len) {
    if (!have_nic || len == 0 || len > BUF_SIZE)
        return -1;

    int t = tx_cur;
    memcpy(tx_buf[t], (void *)frame, len);
    tx_ring[t].addr   = (uint32_t)&tx_buf[t][0];
    tx_ring[t].length = len;
    tx_ring[t].cmd    = TXD_CMD_EOP | TXD_CMD_IFCS | TXD_CMD_RS;
    tx_ring[t].status = 0;

    tx_cur = (t + 1) % NTX;
    reg_wr(E1000_TDT, tx_cur);

    for (int i = 0; i < 200000 && !(tx_ring[t].status & TXD_STAT_DD); i++)
        io_wait();

    if (!(tx_ring[t].status & TXD_STAT_DD))
        return -1;
    tx_frames++;
    return len;
}

int e1000_rx_poll(void (*cb)(const uint8_t *frame, uint16_t len)) {
    int got = 0;
    while (rx_ring[rx_cur].status & RXD_STAT_DD) {
        uint16_t len = rx_ring[rx_cur].length;
        if (cb)
            cb(rx_buf[rx_cur], len);
        rx_frames++;
        got++;

        rx_ring[rx_cur].status = 0;
        int old = rx_cur;
        rx_cur = (rx_cur + 1) % NRX;
        reg_wr(E1000_RDT, old);
    }
    return got;
}

/* ------------------------------------------------------------------ *
 *  Self-test                                                          *
 * ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ *
 *  Probe                                                              *
 * ------------------------------------------------------------------ */
/* Intel "PCH LAN" parts (I217/I218/...): the PHY hangs off the chipset over an
 * internal link and needs a semaphore dance for a clean MAC reset. Firmware has
 * already brought the PHY up, so we skip the disruptive reset and just arm the
 * rings. Everything else in the register model is shared with the 8254x. */
static int is_pch_lan(uint16_t dev) {
    switch (dev) {
    case 0x153A: case 0x153B:            /* I217-LM / I217-V           */
    case 0x15A0: case 0x15A1: case 0x15A2: case 0x15A3:  /* I218 LM/V  */
    case 0x1502: case 0x1503:            /* 82579LM / 82579V           */
    case 0x1570:                         /* I219 (early)               */
    case 0x15B7: case 0x15B8: case 0x15B9:               /* I219 / (2) */
    case 0x15D6: case 0x15D7: case 0x15D8:  /* I219 (4/5) -- T470s     */
    case 0x15E3: case 0x15E1: case 0x15E2:               /* I219 (5/6) */
    case 0x0DC5: case 0x0DC6: case 0x0DC7:               /* I219 (later) */
        return 1;
    default:
        return 0;
    }
}

void e1000_probe(pci_device_t *d) {
    if (cmdline_has("nonet"))
        return;

    uint32_t base = 0, span = 0;
    for (int i = 0; i < 6; i++)
        if (!d->bar[i].is_io && d->bar[i].addr) {
            base = d->bar[i].addr;
            span = d->bar[i].size;
            break;
        }
    if (!base) {
        klogf(LOG_ERR, "e1000: no memory BAR\n");
        return;
    }
    if (span == 0)
        span = 0x10000;              /* 64K identity fallback */
    if (span > 0x100000)
        span = 0x100000;             /* clamp to 1 MiB */

    int pch = is_pch_lan(d->device);
    if (pch)
        mac_from_eeprom = 0;

    /* BARs above 4 GiB would need a 64-bit TDBAL/TDBAL+4; the MBA's 82579LM
     * MMIO is below the 4 GiB fence so a 1:1 32-bit identity map suffices. */
    for (uint32_t off = 0; off < span; off += PAGE_SIZE)
        if (!vmm_map_phys(get_kern_directory(), base + off, base + off,
                              PAGE_PRESENT | PAGE_RW | 0x10)) {
            klogf(LOG_WARNING, "e1000: cannot map MMIO 0x%x, skipping\n",
                  base + off);
            return;
        }
    mmio = (volatile uint8_t *)base;

    pci_enable(d, PCI_CMD_MEM | PCI_CMD_MASTER);

    /* Mask interrupts. */
    reg_wr(E1000_IMC, 0xFFFFFFFF);
    (void)reg_rd(E1000_ICR);

    if (!pch) {
        /* Full MAC reset (safe on the 8254x / 82574). */
        reg_wr(E1000_CTRL, reg_rd(E1000_CTRL) | CTRL_RST);
        spin(2000);
        for (int i = 0; i < 1000 && (reg_rd(E1000_CTRL) & CTRL_RST); i++)
            io_wait();
        reg_wr(E1000_IMC, 0xFFFFFFFF);
        (void)reg_rd(E1000_ICR);
    }

    /* Link up, auto speed/duplex. Preserve the bits firmware set on PCH LAN. */
    if (pch)
        reg_wr(E1000_CTRL, reg_rd(E1000_CTRL) | CTRL_SLU | CTRL_ASDE);
    else
        reg_wr(E1000_CTRL, CTRL_SLU | CTRL_ASDE | CTRL_FD);

    /* Clear the multicast table. */
    for (int i = 0; i < 128; i++)
        reg_wr(E1000_MTA + i * 4, 0);

    read_mac();
    reg_wr(E1000_RAL0, mac[0] | (mac[1] << 8) | (mac[2] << 16) | ((uint32_t)mac[3] << 24));
    reg_wr(E1000_RAH0, mac[4] | (mac[5] << 8) | (1u << 31) /* Address Valid */);

    tx_init();
    rx_init();
    have_nic = 1;

    /* Give auto-negotiation a moment (instant under QEMU). */
    for (int i = 0; i < 2000 && !(reg_rd(E1000_STATUS) & STATUS_LU); i++)
        io_wait();

    int link = (reg_rd(E1000_STATUS) & STATUS_LU) ||
               (phy_read(PHY_BMSR) & BMSR_LINK);
    klogf(LOG_INFO, "e1000: MAC %02x:%02x:%02x:%02x:%02x:%02x, mmio 0x%x, link %s\n",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (uint32_t)base,
          link ? "up" : "down");
}

/* ------------------------------------------------------------------ *
 *  Accessors                                                          *
 * ------------------------------------------------------------------ */
int      e1000_present(void)  { return have_nic; }
int      e1000_link_up(void)  {
    return have_nic && ((reg_rd(E1000_STATUS) & STATUS_LU) ||
                        (phy_read(PHY_BMSR) & BMSR_LINK));
}
uint32_t e1000_rx_count(void) { return rx_frames; }
uint32_t e1000_tx_count(void) { return tx_frames; }

void e1000_mac(uint8_t out[6]) { memcpy(out, mac, 6); }
