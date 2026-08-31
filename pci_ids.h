/**
 * @file pci_ids.h
 * @brief Name lookups for PCI class codes and for the vendor/device pairs the
 *        QEMU machines expose (plus a few common extras).
 */
#pragma once

#include <types.h>

/**
 * @brief Describe a class code.
 * @return A static string like "Ethernet controller" or "USB controller
 *         (UHCI)"; falls back to the base-class name, then "Unknown class".
 */
const char *pci_class_name(uint8_t class, uint8_t subclass, uint8_t progif);

/** @return A static vendor name, or "Unknown vendor". */
const char *pci_vendor_name(uint16_t vendor);

/** @return A static device name, or NULL if the pair is not in the table. */
const char *pci_device_name(uint16_t vendor, uint16_t device);
