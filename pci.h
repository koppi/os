/**
 * @file pci.h
 * @brief PCI configuration-space access (the 0xCF8/0xCFC I/O mechanism).
 */
#pragma once

#include <types.h>

/** @name PCI config-space register offsets */
///@{
#define PCI_VENDOR_DEVICE   0x00
#define PCI_CLASS_SUBCLASS  0x08
#define PCI_BAR0    0x10
#define PCI_BAR1    PCI_BAR0 + 4
#define PCI_BAR2    PCI_BAR1 + 4
#define PCI_BAR3    PCI_BAR2 + 4
#define PCI_BAR4    PCI_BAR3 + 4
#define PCI_BAR5    PCI_BAR4 + 4
///@}

/**
 * @brief Scan every bus/slot/function for a vendor:device pair.
 * @param vendor,device Identifiers to match.
 * @param bus,dev,function Out params for the location if found.
 * @return Non-zero if a match was found.
 */
uint8_t pci_find(uint32_t vendor, uint32_t device, uint8_t* bus,  uint8_t* dev, uint8_t* function);

/** @brief Read a 32-bit config-space register. */
uint32_t pci_read(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset);

/** @brief Write a 32-bit config-space register. */
void pci_write(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint32_t data);

/** @brief Enumerate the bus and log every device found. */
void pci_test();
