/**
 * @file pci.c
 * @brief PCI configuration-space access, a boot-time bus enumeration and a
 *        driver-binding registry.
 *
 * @ref pci_init walks the bus once into a static @ref pci_device_t table
 * (honouring the multifunction bit), logs every device by name via
 * [pci_ids.c](pci_ids.c), then hands each record to the first matching entry of
 * @ref drivers. A driver with a NULL probe is one that is brought up elsewhere
 * (UHCI, from the USB kernel thread) and is only recorded here.
 */
#include <pci.h>

#include <io.h>
#include <log.h>
#include <lib/string.h>
#include <pci_ids.h>

#include <pci_acpi.h>
#include <pci_piix.h>
#include <pci_vga.h>
#include <pci_ac97.h>
#include <e1000.h>
#include <ahci.h>
#include <nvme.h>
#include <hda.h>
#include <virtio_gpu.h>

#define PCI_CONFIG  0xCF8
#define PCI_DATA    0xCFC

/* A Kaby Lake ThinkPad (T470s) exposes ~30 functions once every PCH device and
 * the devices behind the PCIe root ports are counted, so 32 was not enough. */
#define PCI_MAX_DEVICES 64

/**
 * @brief Read a 32-bit configuration register through the CF8/CFC mechanism.
 * @param bus      Bus number, 0..255.
 * @param device   Device (slot) number, 0..31.
 * @param function Function number, 0..7.
 * @param offset   Register offset; forced down to a dword boundary.
 * @return The dword, or all-ones for a function that is not present.
 *
 * The offset is masked to a multiple of 4 because configuration space is only
 * addressable a dword at a time, so a sub-dword offset silently reads the
 * dword containing it. That is not a limitation to work around — it is exactly
 * what @ref pci_cfg_read8 and @ref pci_cfg_read16 build on.
 */
uint32_t
pci_read(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset) {
    uint32_t reg = 0x80000000;

    reg |= (bus & 0xFF) << 16;
    reg |= (device & 0x1F) << 11;
    reg |= (function & 0x7) << 8;
    reg |= (offset & 0xFF) & 0xFC;

    outportl(PCI_CONFIG, reg );

    return inportl(PCI_DATA);
}

/**
 * @brief Write a 32-bit configuration register through CF8/CFC.
 * @param bus      Bus number.
 * @param device   Device (slot) number.
 * @param function Function number.
 * @param offset   Register offset; forced down to a dword boundary.
 * @param data     The dword to write.
 *
 * Always writes all four bytes, so changing part of a register means reading
 * it first — which is what @ref pci_cfg_write16 does.
 */
void
pci_write(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint32_t data) {
    uint32_t reg = 0x80000000;

    reg |= (bus & 0xFF) << 16;
    reg |= (device & 0x1F) << 11;
    reg |= (function & 0x7) << 8;
    reg |= offset & 0xFC;

    outportl(PCI_CONFIG, reg );
    outportl(PCI_DATA, data);
}

/* ---- sub-dword config access for an enumerated device ---- */

/**
 * @brief Read one byte of an enumerated device's configuration space.
 * @param d   The device.
 * @param off Byte offset; any alignment.
 * @return The byte at @p off.
 */
uint8_t pci_cfg_read8(const pci_device_t *d, uint8_t off) {
    return (pci_read(d->bus, d->slot, d->func, off) >> ((off & 3) * 8)) & 0xFF;
}
/**
 * @brief Read one 16-bit word of an enumerated device's configuration space.
 * @param d   The device.
 * @param off Byte offset; assumed even, since a word straddling a dword
 *            boundary would need two reads and no register is laid out that
 *            way.
 * @return The word at @p off.
 */
uint16_t pci_cfg_read16(const pci_device_t *d, uint8_t off) {
    return (pci_read(d->bus, d->slot, d->func, off) >> ((off & 2) * 8)) & 0xFFFF;
}
/** @brief Read a 32-bit configuration register of an enumerated device.
 *         A convenience wrapper that saves unpacking bus/slot/func. */
uint32_t pci_cfg_read32(const pci_device_t *d, uint8_t off) {
    return pci_read(d->bus, d->slot, d->func, off);
}
/** @brief Write a 32-bit configuration register of an enumerated device. */
void pci_cfg_write32(const pci_device_t *d, uint8_t off, uint32_t v) {
    pci_write(d->bus, d->slot, d->func, off, v);
}
/**
 * @brief Write one 16-bit word of an enumerated device's configuration space.
 * @param d   The device.
 * @param off Byte offset; assumed even.
 * @param v   The word to write.
 *
 * A read-modify-write of the containing dword, since configuration space
 * cannot be written more narrowly. The other half of the dword is rewritten
 * with what was just read, so a register whose neighbour has read-to-clear or
 * write-one-to-clear bits can be disturbed by writing this one.
 */
void pci_cfg_write16(const pci_device_t *d, uint8_t off, uint16_t v) {
    uint32_t dw = pci_read(d->bus, d->slot, d->func, off & ~3u);
    uint32_t sh = (off & 2) * 8;
    dw = (dw & ~(0xFFFFu << sh)) | ((uint32_t)v << sh);
    pci_write(d->bus, d->slot, d->func, off & ~3u, dw);
}

/**
 * @brief Brute-force search the whole bus for a vendor:device pair.
 * @param vendor   Vendor id to match.
 * @param device   Device id to match.
 * @param bus      Receives the bus number on a hit.
 * @param dev      Receives the device (slot) number.
 * @param function Receives the function number.
 * @return 1 if found, 0 otherwise.
 *
 * Probes all 65536 bus/slot/function combinations and does not honour the
 * multifunction bit, so it reads functions 1..7 of single-function devices as
 * well — harmless, since those read back all-ones, but it makes a miss cost
 * 65536 config cycles. Prefer @ref pci_get_by_id, which searches the table
 * @ref pci_init already built.
 */
uint8_t
pci_find(uint32_t vendor, uint32_t device, uint8_t* bus,  uint8_t* dev, uint8_t* function) {
  uint32_t vend_dev, b, d, f, my_vend_dev;

  my_vend_dev = ( vendor & 0xFFFF ) | (device << 16);

  for(b = 0; b < 256; b++)
    for(d = 0; d < 32; d++)
      for(f = 0; f < 8; f++){
        vend_dev = pci_read(b,d,f,PCI_VENDOR_DEVICE);
        if(vend_dev == my_vend_dev) {
          *bus = b;
          *dev = d;
          *function = f;
          return 1;
        }
      }
  return 0;
}

/* ------------------------------------------------------------------ *
 *  Enumeration                                                        *
 * ------------------------------------------------------------------ */

static pci_device_t table[PCI_MAX_DEVICES];
static int ndev;

/**
 * @brief Turn on decoding or bus mastering for a device.
 * @param d    The device.
 * @param bits Any of @ref PCI_CMD_IO, @ref PCI_CMD_MEM, @ref PCI_CMD_MASTER.
 *
 * Only ever sets bits — nothing here can turn decoding off — and skips the
 * write entirely when they are already set, so a driver can call it without
 * checking first.
 */
void pci_enable(const pci_device_t *d, uint16_t bits) {
    uint32_t cmd = pci_read(d->bus, d->slot, d->func, PCI_COMMAND) & 0xFFFF;
    if ((cmd & bits) == bits)
        return;
    pci_write(d->bus, d->slot, d->func, PCI_COMMAND, (cmd | bits));
}

/** @brief Probe each BAR of @p d for its address, kind and size. */
static void decode_bars(pci_device_t *d) {
    /* Turn off I/O + memory decode while the BARs read back as size masks. */
    uint16_t cmd = pci_read(d->bus, d->slot, d->func, PCI_COMMAND) & 0xFFFF;
    pci_write(d->bus, d->slot, d->func, PCI_COMMAND, cmd & ~(PCI_CMD_IO | PCI_CMD_MEM));

    for (int i = 0; i < 6; i++) {
        uint8_t off = PCI_BAR0 + i * 4;
        uint32_t orig  = pci_read(d->bus, d->slot, d->func, off);
        pci_write(d->bus, d->slot, d->func, off, 0xFFFFFFFF);
        uint32_t probe = pci_read(d->bus, d->slot, d->func, off);
        pci_write(d->bus, d->slot, d->func, off, orig);

        if (probe == 0 || probe == 0xFFFFFFFF)
            continue;                       /* unimplemented */

        if (orig & 1) {
            uint32_t m = probe & ~0x3u;
            d->bar[i].is_io = 1;
            d->bar[i].addr  = orig & ~0x3u;
            d->bar[i].size  = m ? (~m + 1) : 0;
        } else {
            uint32_t m = probe & ~0xFu;
            d->bar[i].is_io    = 0;
            d->bar[i].prefetch = (orig >> 3) & 1;
            d->bar[i].is64     = ((orig >> 1) & 3) == 2;
            d->bar[i].addr     = orig & ~0xFu;
            d->bar[i].size     = m ? (~m + 1) : 0;
            if (d->bar[i].is64 && i < 5)
                i++;                        /* the next slot is the high dword */
        }
    }

    pci_write(d->bus, d->slot, d->func, PCI_COMMAND, cmd);
}

/** @brief Read one function into @p out. @return 1 if a device is present. */
static int read_function(uint8_t bus, uint8_t slot, uint8_t func, pci_device_t *out) {
    uint32_t id = pci_read(bus, slot, func, PCI_VENDOR_DEVICE);
    if ((id & 0xFFFF) == 0xFFFF)
        return 0;

    memset(out, 0, sizeof(*out));
    out->bus = bus; out->slot = slot; out->func = func;
    out->vendor = id & 0xFFFF;
    out->device = (id >> 16) & 0xFFFF;

    uint32_t cls = pci_read(bus, slot, func, PCI_CLASS_SUBCLASS);
    out->revision = cls & 0xFF;
    out->progif   = (cls >> 8) & 0xFF;
    out->subclass = (cls >> 16) & 0xFF;
    out->class    = (cls >> 24) & 0xFF;

    out->header_type = (pci_read(bus, slot, func, 0x0C) >> 16) & 0x7F;

    uint32_t intr = pci_read(bus, slot, func, PCI_INTERRUPT_LINE);
    out->irq_line = intr & 0xFF;
    out->irq_pin  = (intr >> 8) & 0xFF;

    if (out->header_type == 0) {
        uint32_t sub = pci_read(bus, slot, func, PCI_SUBSYSTEM);
        out->subsys_vendor = sub & 0xFFFF;
        out->subsys_device = (sub >> 16) & 0xFFFF;
        decode_bars(out);
    }

    out->name = pci_device_name(out->vendor, out->device);
    return 1;
}

/**
 * @brief Enumerate every function on every bus into the static device table.
 *
 * A flat sweep of all 256 bus numbers rather than a walk down the bridges:
 * the buses behind a bridge carry their own numbers and are found by the same
 * sweep, so nothing needs to know the topology. Functions 1..7 are probed only
 * when the header type says the device is multifunction, which keeps the scan
 * to one config read for most slots.
 *
 * Filling the table stops the scan with a warning rather than overrunning it,
 * so a machine with more functions than @ref PCI_MAX_DEVICES loses the tail of
 * its device list — which is why that limit grew when a Kaby Lake ThinkPad
 * turned up with about 30 functions.
 */
static void pci_scan(void) {
    ndev = 0;
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            uint32_t id0 = pci_read(bus, slot, 0, PCI_VENDOR_DEVICE);
            if ((id0 & 0xFFFF) == 0xFFFF)
                continue;

            int multifn = (pci_read(bus, slot, 0, 0x0C) >> 16) & 0x80;
            for (uint32_t func = 0; func < (multifn ? 8u : 1u); func++) {
                if (ndev >= PCI_MAX_DEVICES) {
                    klogf(LOG_WARNING, "PCI: device table full (%d), stopping scan\n",
                          PCI_MAX_DEVICES);
                    return;
                }
                if (read_function(bus, slot, func, &table[ndev]))
                    ndev++;
            }
        }
    }
}

/* ------------------------------------------------------------------ *
 *  Reporting                                                          *
 * ------------------------------------------------------------------ */

/** @brief Print a BAR as e.g. "io 0xc500/64" or "mem 0xfeb80000/128K". */
static void log_bar(int n, const pci_bar_t *b) {
    if (b->size == 0 && b->addr == 0)
        return;
    uint32_t sz = b->size;
    char unit = 'B';
    if (sz && (sz % (1024 * 1024)) == 0) { sz /= 1024 * 1024; unit = 'M'; }
    else if (sz && (sz % 1024) == 0)     { sz /= 1024;        unit = 'K'; }
    klogf(LOG_INFO, "      BAR%d %s 0x%x/%u%c\n",
          n, b->is_io ? "io " : "mem", b->addr, sz, unit);
}

/**
 * @brief Log the enumerated device table: ids, class, driver, IRQ and BARs.
 *
 * Runs once at the end of @ref pci_init and again on the console's `pci`
 * command. A device with no bound driver is listed too, which is the quickest
 * way to see what a new machine has that this kernel does not handle.
 */
void pci_dump(void) {
    klogf(LOG_INFO, "PCI devices: %d\n", ndev);
    for (int i = 0; i < ndev; i++) {
        pci_device_t *d = &table[i];
        klogf(LOG_INFO, " * %x:%x.%u  %04x:%04x [%02x%02x]  %s: %s\n",
              d->bus, d->slot, d->func, d->vendor, d->device,
              d->class, d->subclass,
              pci_class_name(d->class, d->subclass, d->progif),
              d->name ? d->name : "(unknown device)");
        if (d->irq_line && d->irq_line != 0xFF)
            klogf(LOG_INFO, "      IRQ %u  driver: %s\n",
                  d->irq_line, d->driver ? d->driver : "-");
        else
            klogf(LOG_INFO, "      driver: %s\n", d->driver ? d->driver : "-");
        for (int b = 0; b < 6; b++)
            log_bar(b, &d->bar[b]);
    }
}

/** @brief How many PCI functions the boot-time scan recorded.
 *  @return The device count, capped at @ref PCI_MAX_DEVICES. */
int pci_count(void) { return ndev; }

/**
 * @brief Find unused 32-bit MMIO space to re-home a BAR into.
 * @param size Bytes needed; the result is aligned to it. 0 means one page.
 * @return A physical address, or 0 if there is no room.
 *
 * Returns the lowest address above every assigned memory BAR, never below
 * 0xC0000000 and never running into the LAPIC/IOAPIC/HPET block at
 * 0xFEC00000. This exists for one case: a UEFI firmware that parks a BAR
 * above 4 GiB, where a 32-bit kernel cannot reach it at all — xhci.c uses it
 * to move the controller somewhere addressable.
 *
 * Only PCI BARs are considered. Anything else the firmware placed in this
 * range and did not describe through a BAR — reserved ACPI regions, say — is
 * invisible here, so the address is "no PCI device claims it", not "provably
 * free".
 */
uint32_t pci_mmio_hole(uint32_t size) {
    /* Lowest 32-bit MMIO address above every assigned memory BAR, aligned to
     * `size`, kept below the LAPIC/IOAPIC/HPET block at 0xFEC00000. Used to
     * re-home a BAR a UEFI firmware parked above 4 GiB where a 32-bit kernel
     * cannot reach it. */
    uint32_t top = 0xC0000000;
    for (int i = 0; i < ndev; i++)
        for (int b = 0; b < 6; b++) {
            const pci_bar_t *bar = &table[i].bar[b];
            if (bar->is_io || !bar->addr || !bar->size)
                continue;
            uint32_t end = bar->addr + bar->size;
            if (end > top && end < 0xFEC00000)
                top = end;
        }
    if (size == 0) size = 0x1000;
    top = (top + size - 1) & ~(size - 1);
    if (top + size > 0xFEC00000)
        return 0;
    return top;
}

/**
 * @brief Fetch an enumerated device by index.
 * @param i Index, 0-based.
 * @return The device, or NULL if @p i is out of range.
 *
 * The pointer is into a static table that is built once at boot and never
 * moves, so it stays valid for the life of the kernel.
 */
const pci_device_t *pci_dev(int i) {
    return (i >= 0 && i < ndev) ? &table[i] : NULL;
}

/**
 * @brief Find the first enumerated device with a given vendor:device id.
 * @return The device, or NULL.
 */
const pci_device_t *pci_get_by_id(uint16_t vendor, uint16_t device) {
    for (int i = 0; i < ndev; i++)
        if (table[i].vendor == vendor && table[i].device == device)
            return &table[i];
    return NULL;
}

/**
 * @brief Find the first enumerated device of a given class:subclass.
 * @return The device, or NULL.
 *
 * First in enumeration order, which is not necessarily the one wanted: a
 * laptop has two class 04:03 audio controllers, and the digital-only HDMI one
 * usually enumerates first. A driver that cares has to filter for itself, as
 * @ref hda_probe does.
 */
const pci_device_t *pci_get_by_class(uint8_t class, uint8_t subclass) {
    for (int i = 0; i < ndev; i++)
        if (table[i].class == class && table[i].subclass == subclass)
            return &table[i];
    return NULL;
}

/* ------------------------------------------------------------------ *
 *  Driver binding                                                     *
 * ------------------------------------------------------------------ */

#define ANY (-1)

static const pci_driver_t drivers[] = {
    { "piix-host", 0x8086, 0x1237, ANY,  ANY,  piix_host_probe },
    { "piix-isa",  0x8086, 0x7000, ANY,  ANY,  piix_isa_probe  },
    { "piix-ide",  0x8086, 0x7010, ANY,  ANY,  piix_ide_probe  },
    { "acpi",      0x8086, 0x7113, ANY,  ANY,  acpi_probe      },
    { "usb",       ANY,    ANY,    0x0C, 0x03, NULL /* USB thread */ },
    { "ahci",      ANY,    ANY,    0x01, 0x06, ahci_probe      },
    /* Any NVM Express controller (class 01:08): the M.2 SSD in a Kaby Lake
     * ThinkPad (T470s) is NVMe, not SATA. nvme_init() creates one I/O queue
     * pair and mounts namespace 1 as the next free hd{a,b,...}. */
    { "nvme",      ANY,    ANY,    0x01, 0x08, nvme_probe      },
    { "bochs-vga", 0x1234, 0x1111, ANY,  ANY,  bochs_vga_probe },
    /* QEMU `-vga virtio`: the modern virtio-gpu 2D device. Drives scanout 0
     * from the framebuffer shadow and follows host window resizes. */
    { "virtio-gpu", 0x1AF4, 0x1050, ANY, ANY,  virtio_gpu_probe },
    /* Any Intel Ethernet controller (82540 'e1000', 82574L 'e1000e', the
     * I217/I218 PCH-LAN in a ThinkPad, ...): the register model is shared and
     * e1000_probe special-cases the PCH parts. */
    { "e1000",     0x8086, ANY,    0x02, 0x00, e1000_probe     },
    { "ac97",      0x8086, 0x2415, ANY,  ANY,  ac97_probe      },
    { "hda",       ANY,    ANY,    0x04, 0x03, hda_probe       },
};

/** @brief Match @p d against the driver table and run the first probe. */
static void bind_driver(pci_device_t *d) {
    for (unsigned i = 0; i < sizeof(drivers) / sizeof(drivers[0]); i++) {
        const pci_driver_t *drv = &drivers[i];
        if (drv->vendor   != ANY && drv->vendor   != d->vendor)   continue;
        if (drv->device   != ANY && drv->device   != d->device)   continue;
        if (drv->class    != ANY && drv->class    != d->class)    continue;
        if (drv->subclass != ANY && drv->subclass != d->subclass) continue;

        d->driver = drv->name;
        if (drv->probe) {
            drv->probe(d);
        } else {
            klogf(LOG_INFO, "PCI: %x:%x.%u bound to '%s' (deferred)\n",
                  d->bus, d->slot, d->func, drv->name);
        }
        return;
    }
    klogf(LOG_INFO, "PCI: %x:%x.%u %x:%x has no driver\n",
          d->bus, d->slot, d->func, d->vendor, d->device);
}

/**
 * @brief Enumerate the bus, bind drivers and log the result.
 *
 * Called once at boot. Every device is offered to the driver table in order
 * and taken by the first entry that matches, so a specific vendor:device rule
 * must be listed ahead of the class-wide one it would otherwise lose to.
 * Probing happens inline here, so a driver that blocks during probe delays
 * the rest of boot.
 */
void pci_init(void) {
    pci_scan();
    klogf(LOG_INFO, "PCI: %d devices, binding drivers\n", ndev);
    for (int i = 0; i < ndev; i++)
        bind_driver(&table[i]);
    pci_dump();
}
