/**
 * @file pci_ids.c
 * @brief Static name tables for PCI class codes and vendor/device pairs.
 *
 * Deliberately small: it covers what the QEMU `pc`/`q35` machines put on the
 * bus, the devices this kernel drives, and a handful of alternatives someone is
 * likely to pass with `-device` (rtl8139, pcnet, virtio, xhci, ...).
 */
#include <pci_ids.h>

#include <lib/string.h>

/* ------------------------------------------------------------------ *
 *  Class codes                                                        *
 * ------------------------------------------------------------------ */

/** Base-class names, indexed by class code (0x00-0x13). */
static const char *base_class[] = {
    "Unclassified device",
    "Mass storage controller",
    "Network controller",
    "Display controller",
    "Multimedia controller",
    "Memory controller",
    "Bridge",
    "Communication controller",
    "Base system peripheral",
    "Input device controller",
    "Docking station",
    "Processor",
    "Serial bus controller",
    "Wireless controller",
    "Intelligent controller",
    "Satellite communications controller",
    "Encryption controller",
    "Signal processing controller",
    "Processing accelerator",
    "Non-essential instrumentation",
};

/** Fully-qualified names for the class:subclass[:progif] triples we care about. */
static const struct {
    uint8_t class, subclass, progif, has_progif;
    const char *name;
} class_full[] = {
    { 0x01, 0x01, 0x00, 0, "IDE controller" },
    { 0x01, 0x06, 0x00, 0, "SATA controller (AHCI)" },
    { 0x01, 0x08, 0x02, 1, "NVMe controller" },
    { 0x02, 0x00, 0x00, 0, "Ethernet controller" },
    { 0x03, 0x00, 0x00, 0, "VGA controller" },
    { 0x03, 0x80, 0x00, 0, "Display controller" },
    { 0x04, 0x00, 0x00, 0, "Multimedia video controller" },
    { 0x04, 0x01, 0x00, 0, "Multimedia audio controller" },
    { 0x04, 0x03, 0x00, 0, "Audio device (HD Audio)" },
    { 0x05, 0x00, 0x00, 0, "RAM memory controller" },
    { 0x06, 0x00, 0x00, 0, "Host bridge" },
    { 0x06, 0x01, 0x00, 0, "ISA bridge" },
    { 0x06, 0x04, 0x00, 0, "PCI-to-PCI bridge" },
    { 0x06, 0x80, 0x00, 0, "Bridge (other)" },
    { 0x0C, 0x03, 0x00, 1, "USB controller (UHCI)" },
    { 0x0C, 0x03, 0x10, 1, "USB controller (OHCI)" },
    { 0x0C, 0x03, 0x20, 1, "USB controller (EHCI)" },
    { 0x0C, 0x03, 0x30, 1, "USB controller (xHCI)" },
    { 0x0C, 0x05, 0x00, 0, "SMBus controller" },
};

const char *pci_class_name(uint8_t class, uint8_t subclass, uint8_t progif) {
    for (unsigned i = 0; i < sizeof(class_full) / sizeof(class_full[0]); i++) {
        if (class_full[i].class == class && class_full[i].subclass == subclass &&
            (!class_full[i].has_progif || class_full[i].progif == progif))
            return class_full[i].name;
    }
    if (class < sizeof(base_class) / sizeof(base_class[0]))
        return base_class[class];
    if (class == 0xFF)
        return "Unassigned class";
    return "Unknown class";
}

/* ------------------------------------------------------------------ *
 *  Vendors / devices                                                  *
 * ------------------------------------------------------------------ */

static const struct { uint16_t id; const char *name; } vendors[] = {
    { 0x8086, "Intel" },
    { 0x1234, "QEMU" },
    { 0x1af4, "Red Hat / Virtio" },
    { 0x1b36, "Red Hat / QEMU" },
    { 0x10ec, "Realtek" },
    { 0x1022, "AMD" },
    { 0x1013, "Cirrus Logic" },
    { 0x15ad, "VMware" },
    { 0x80ee, "InnoTek / VirtualBox" },
};

static const struct { uint16_t vendor, device; const char *name; } devices[] = {
    /* QEMU i440FX / PIIX3 / PIIX4 chipset */
    { 0x8086, 0x1237, "440FX - 82441FX PMC [Natoma]" },
    { 0x8086, 0x7000, "82371SB PIIX3 ISA [Natoma/Triton II]" },
    { 0x8086, 0x7010, "82371SB PIIX3 IDE [Natoma/Triton II]" },
    { 0x8086, 0x7020, "82371SB PIIX3 USB [Natoma/Triton II]" },
    { 0x8086, 0x7113, "82371AB/EB/MB PIIX4 ACPI" },
    /* QEMU q35 chipset */
    { 0x8086, 0x29c0, "82G33/G31/P35/P31 [MCH]" },
    { 0x8086, 0x2918, "82801IB (ICH9) LPC Interface Controller" },
    { 0x8086, 0x2922, "82801IR/IO/IH (ICH9R/DO/DH) SATA AHCI Controller" },
    { 0x8086, 0x2930, "82801I (ICH9) SMBus Controller" },
    /* ThinkPad X250 -- Broadwell-U + Wildcat Point-LP PCH */
    { 0x8086, 0x1604, "Broadwell-U Host Bridge -OPI" },
    { 0x8086, 0x1616, "HD Graphics 5500 (Broadwell GT2)" },
    { 0x8086, 0x160c, "Broadwell-U Audio Controller (HDMI)" },
    { 0x8086, 0x9cb1, "Wildcat Point-LP USB xHCI Controller" },
    { 0x8086, 0x9cba, "Wildcat Point-LP MEI Controller #1" },
    { 0x8086, 0x15a2, "Ethernet Connection I218-LM" },
    { 0x8086, 0x15a1, "Ethernet Connection I218-V" },
    { 0x8086, 0x153a, "Ethernet Connection I217-LM" },
    { 0x8086, 0x153b, "Ethernet Connection I217-V" },
    { 0x8086, 0x9ca0, "Wildcat Point-LP High Definition Audio Controller" },
    { 0x8086, 0x9c9a, "Wildcat Point-LP PCI Express Root Port #3" },
    { 0x8086, 0x9c94, "Wildcat Point-LP PCI Express Root Port #1" },
    { 0x8086, 0x9c90, "Wildcat Point-LP PCI Express Root Port #1" },
    { 0x8086, 0x9c98, "Wildcat Point-LP PCI Express Root Port #5" },
    { 0x8086, 0x9ca6, "Wildcat Point-LP USB EHCI Controller #1" },
    { 0x8086, 0x9cc3, "Wildcat Point-LP LPC Controller (Premium SKU)" },
    { 0x8086, 0x9c83, "Wildcat Point-LP SATA Controller [AHCI Mode]" },
    { 0x8086, 0x9ca2, "Wildcat Point-LP SMBus Controller" },
    { 0x8086, 0x9ca4, "Wildcat Point-LP Thermal Management Controller" },
    { 0x10ec, 0x5227, "RTS5227 PCI Express Card Reader" },
    { 0x8086, 0x095b, "Wireless 7265 (Stone Peak 2)" },
    { 0x8086, 0x095a, "Wireless 7265 (Stone Peak 2)" },
    /* Display */
    { 0x1234, 0x1111, "QEMU/Bochs Virtual Video Controller" },
    { 0x1013, 0x00b8, "GD 5446 (Cirrus)" },
    { 0x1b36, 0x0100, "QXL paravirtual graphic card" },
    { 0x1af4, 0x1050, "Virtio GPU" },
    /* Network */
    { 0x8086, 0x100e, "82540EM Gigabit Ethernet Controller" },
    { 0x8086, 0x100f, "82545EM Gigabit Ethernet Controller" },
    { 0x8086, 0x10d3, "82574L Gigabit Network Connection" },
    { 0x10ec, 0x8139, "RTL-8139/8139C/8139C+ Fast Ethernet" },
    { 0x1022, 0x2000, "79c970 [PCnet32 LANCE]" },
    { 0x1af4, 0x1000, "Virtio network device" },
    /* Audio */
    { 0x8086, 0x2415, "82801AA AC'97 Audio Controller" },
    { 0x8086, 0x2668, "82801FB/FBM/FR/FW/FRW (ICH6) HD Audio Controller" },
    /* USB */
    { 0x1b36, 0x000d, "QEMU XHCI Host Controller" },
    { 0x8086, 0x24cd, "82801DB/DBM (ICH4) USB2 EHCI Controller" },
    /* Storage / misc */
    { 0x1af4, 0x1001, "Virtio block device" },
    { 0x1af4, 0x1002, "Virtio memory balloon" },
    { 0x1af4, 0x1005, "Virtio RNG" },
    { 0x1b36, 0x0010, "NVMe controller" },
};

const char *pci_vendor_name(uint16_t vendor) {
    for (unsigned i = 0; i < sizeof(vendors) / sizeof(vendors[0]); i++)
        if (vendors[i].id == vendor)
            return vendors[i].name;
    return "Unknown vendor";
}

const char *pci_device_name(uint16_t vendor, uint16_t device) {
    for (unsigned i = 0; i < sizeof(devices) / sizeof(devices[0]); i++)
        if (devices[i].vendor == vendor && devices[i].device == device)
            return devices[i].name;
    return NULL;
}
