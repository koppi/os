/**
 * @file pci_piix.h
 * @brief PCI-side bring-up for the QEMU i440FX / PIIX3 chipset functions: the
 *        82441FX host bridge, the 82371SB ISA bridge and the 82371SB IDE
 *        controller. Disk transfers stay in [ata.c](ata.c); these handlers only
 *        identify the parts and log/enable their PCI configuration.
 */
#pragma once

struct pci_device;

/** @brief 82441FX host bridge (8086:1237): log revision / DRAM controller. */
void piix_host_probe(struct pci_device *d);

/** @brief 82371SB PIIX3 ISA bridge (8086:7000): log PIRQ[A-D] routing + ELCR. */
void piix_isa_probe(struct pci_device *d);

/** @brief 82371SB PIIX3 IDE (8086:7010): enable I/O + bus master, log IDETIM. */
void piix_ide_probe(struct pci_device *d);
