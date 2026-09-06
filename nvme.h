/**
 * @file nvme.h
 * @brief NVM Express (PCIe) block driver — polled, admin + one I/O queue pair,
 *        512-byte sector read/write behind the @ref device_t interface.
 *
 * The storage path on a machine whose M.2 slot carries an NVMe SSD rather than
 * a SATA one (the ThinkPad T470s ships this way). No MSI/MSI-X, no interrupts,
 * a single command in flight, namespace 1 only — enough to mount a FAT volume
 * and read/write it from a ring-3 process.
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief PCI bind hook (class 01:08 = NVMe): record the controller for
 *         nvme_init(). */
void nvme_probe(struct pci_device *d);

/** @brief Bring up the controller, create the I/O queue pair and mount the
 *         first namespace as hd{a,b,...}. Call from main_proc after the RAM
 *         disk has claimed device id 0 (like ahci_init / ata_init). */
void nvme_init(void);

/** @return Number of NVMe namespaces that came up as block devices. */
int nvme_disk_count(void);
