/**
 * @file pci.h
 * @brief PCI configuration-space access (the 0xCF8/0xCFC I/O mechanism), a
 *        boot-time bus enumeration and a small driver-binding registry.
 *
 * @ref pci_init walks every bus/slot/function once into @ref pci_device_t
 * records, logs them by name (see [pci_ids.c](pci_ids.c)) and hands each to the
 * first matching entry of the driver table. Individual drivers live in
 * `pci_*.c` / `e1000.c` and expose a `*_probe(pci_device_t *)` entry point.
 */
#pragma once

#include <types.h>

/** @name PCI config-space register offsets */
///@{
#define PCI_VENDOR_DEVICE   0x00
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_CLASS_SUBCLASS  0x08
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0    0x10
#define PCI_BAR1    (PCI_BAR0 + 4)
#define PCI_BAR2    (PCI_BAR1 + 4)
#define PCI_BAR3    (PCI_BAR2 + 4)
#define PCI_BAR4    (PCI_BAR3 + 4)
#define PCI_BAR5    (PCI_BAR4 + 4)
#define PCI_SUBSYSTEM      0x2C
#define PCI_INTERRUPT_LINE 0x3C
#define PCI_INTERRUPT_PIN  0x3D
///@}

/** @name PCI command-register bits (offset @ref PCI_COMMAND) */
///@{
#define PCI_CMD_IO      (1u << 0)  /**< Respond to I/O-space accesses. */
#define PCI_CMD_MEM     (1u << 1)  /**< Respond to memory-space accesses. */
#define PCI_CMD_MASTER  (1u << 2)  /**< Act as a bus master (DMA). */
///@}

/** One decoded base address register. */
typedef struct pci_bar {
    uint32_t addr;      /**< Base (I/O port or physical address); low flag bits cleared. */
    uint32_t size;      /**< Region size in bytes, 0 if the BAR is unimplemented. */
    uint8_t  is_io;     /**< 1 = I/O space, 0 = memory space. */
    uint8_t  prefetch;  /**< Memory BAR: prefetchable. */
    uint8_t  is64;      /**< Memory BAR: 64-bit (consumes the following slot). */
} pci_bar_t;

/** A device discovered on the bus. */
typedef struct pci_device {
    uint8_t  bus, slot, func;
    uint16_t vendor, device;
    uint8_t  class, subclass, progif, revision;
    uint8_t  header_type;                /**< Offset 0x0E, multifunction bit masked off. */
    uint8_t  irq_line, irq_pin;
    uint16_t subsys_vendor, subsys_device;
    pci_bar_t bar[6];
    const char *name;                    /**< Human-readable name, or NULL. */
    const char *driver;                  /**< Bound driver name, or NULL. */
} pci_device_t;

/** A driver's match rule and entry point for the @ref pci_init binding pass. */
typedef struct pci_driver {
    const char *name;
    int32_t vendor, device;              /**< -1 matches any. */
    int32_t class, subclass;             /**< -1 matches any. */
    void  (*probe)(pci_device_t *dev);   /**< NULL: matched, but brought up elsewhere. */
} pci_driver_t;

/* ---- raw config-space access ---- */

/** @brief Read a 32-bit config-space register. */
uint32_t pci_read(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset);
/** @brief Write a 32-bit config-space register. */
void pci_write(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint32_t data);

/** @brief Read an 8/16/32-bit config register of an enumerated device. */
uint8_t  pci_cfg_read8(const pci_device_t *d, uint8_t off);
uint16_t pci_cfg_read16(const pci_device_t *d, uint8_t off);
uint32_t pci_cfg_read32(const pci_device_t *d, uint8_t off);
/** @brief Write a 16/32-bit config register of an enumerated device. */
void pci_cfg_write16(const pci_device_t *d, uint8_t off, uint16_t v);
void pci_cfg_write32(const pci_device_t *d, uint8_t off, uint32_t v);

/**
 * @brief Scan every bus/slot/function for a vendor:device pair.
 * @param vendor,device Identifiers to match.
 * @param bus,dev,function Out params for the location if found.
 * @return Non-zero if a match was found.
 */
uint8_t pci_find(uint32_t vendor, uint32_t device, uint8_t *bus, uint8_t *dev, uint8_t *function);

/* ---- enumeration + driver binding ---- */

/** @brief Enumerate the bus, log every device by name and bind drivers. */
void pci_init(void);
/** @brief Re-print the enumerated device table (the console `pci` command). */
void pci_dump(void);
/** @return Number of devices found by @ref pci_init. */
int pci_count(void);
/** @return Device @p i (0-based), or NULL. */
const pci_device_t *pci_dev(int i);
/** @return The first device matching @p vendor:@p device, or NULL. */
const pci_device_t *pci_get_by_id(uint16_t vendor, uint16_t device);
/** @return The first device matching @p class:@p subclass, or NULL. */
const pci_device_t *pci_get_by_class(uint8_t class, uint8_t subclass);

/** @brief OR @p bits into the device's command register (I/O / MEM / bus master). */
void pci_enable(const pci_device_t *d, uint16_t bits);
