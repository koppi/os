/**
 * @file ahci.h
 * @brief AHCI (SATA) block driver — polled, one command in flight, registered
 *        with the VFS as hd{a,b,...}. This is the only storage path on a
 *        machine with no legacy IDE (every recent laptop, the ThinkPad X250
 *        included).
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief PCI bind hook: record the AHCI controller for ahci_init(). */
void ahci_probe(struct pci_device *d);

/** @brief Bring up every SATA port with a disk and mount its filesystem.
 *         Call after the RAM disk has claimed device id 0 (like ata_init). */
void ahci_init(void);

/** @return Number of AHCI disks that came up. */
int ahci_disk_count(void);
