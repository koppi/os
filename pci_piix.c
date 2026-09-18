/**
 * @file pci_piix.c
 * @brief PCI-side handlers for the QEMU i440FX / PIIX3 chipset functions.
 *
 * None of these move data — [ata.c](ata.c) still does disk I/O through the
 * legacy task-file ports. They identify the parts and bring their PCI config
 * into a known state (IDE decode + bus mastering on), logging the routing and
 * timing registers that are handy when debugging IRQ delivery.
 */
#include <pci.h>
#include <pci_piix.h>

#include <io.h>
#include <log.h>

void piix_host_probe(pci_device_t *d) {
    /* The 82441FX is the DRAM controller + host bridge; nothing to program.
     * PMC revision 0x02 is what QEMU reports. */
    klogf(LOG_INFO, "piix: 82441FX host bridge, rev %x\n", d->revision);
}

void piix_isa_probe(pci_device_t *d) {
    /* PIRQRC[A-D] at config 0x60-0x63: bit 7 set => that PIRQ line is not
     * routed; otherwise bits [3:0] are the ISA IRQ it lands on. */
    uint32_t pirq = pci_cfg_read32(d, 0x60);
    for (int i = 0; i < 4; i++) {
        uint8_t v = (pirq >> (i * 8)) & 0xFF;
        if (v & 0x80)
            klogf(LOG_INFO, "piix: PIRQ%c -> disabled\n", 'A' + i);
        else
            klogf(LOG_INFO, "piix: PIRQ%c -> IRQ%u\n", 'A' + i, v & 0x0F);
    }

    /* 8259 Edge/Level Control Register (PIIX3 at I/O 0x4D0/0x4D1): a set bit
     * marks that IRQ level-triggered, as PCI interrupts must be. */
    uint16_t elcr = inportb(0x4D0) | (inportb(0x4D1) << 8);
    klogf(LOG_INFO, "piix: ELCR = 0x%x (level-triggered IRQ mask)\n", elcr);
}

void piix_ide_probe(pci_device_t *d) {
    pci_enable(d, PCI_CMD_IO | PCI_CMD_MASTER);

    /* IDETIM: primary at config 0x40, secondary at 0x42. Bit 15 = decode
     * enable for that channel. */
    uint16_t tim_pri = pci_cfg_read16(d, 0x40);
    uint16_t tim_sec = pci_cfg_read16(d, 0x42);
    klogf(LOG_INFO, "piix: IDE primary %s, secondary %s\n",
          (tim_pri & 0x8000) ? "enabled" : "disabled",
          (tim_sec & 0x8000) ? "enabled" : "disabled");

    /* BAR4 is the bus-master IDE I/O window (16 bytes); ata.c can use it later
     * for DMA transfers. */
    if (d->bar[4].addr)
        klogf(LOG_INFO, "piix: IDE bus-master I/O at 0x%x\n", d->bar[4].addr);
}
