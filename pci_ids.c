/**
 * @file pci_ids.c
 * @brief Static name tables for PCI class codes and vendor/device pairs.
 *
 * Deliberately small: it covers what the QEMU `pc`/`q35` machines put on the
 * bus, the devices this kernel drives, and a handful of alternatives someone is
 * likely to pass with `-device` (rtl8139, pcnet, virtio, xhci, ...).
 */
#include <pci_ids.h>



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
    /* NVMe SSD makers found in an M.2 ThinkPad slot */
    { 0x144d, "Samsung" },
    { 0x1179, "Toshiba / KIOXIA" },
    { 0x1c5c, "SK hynix" },
    { 0x1344, "Micron" },
    { 0x2646, "Kingston" },
    { 0x1987, "Phison" },
    { 0x126f, "Silicon Motion" },
    { 0x1cc1, "ADATA" },
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
    /* ThinkPad X220 -- Sandy Bridge + Cougar Point PCH (6 Series / QM67) */
    { 0x8086, 0x0104, "2nd Gen Core Processor DRAM Controller" },
    { 0x8086, 0x0126, "2nd Gen Core Processor HD Graphics 3000" },
    { 0x8086, 0x0116, "2nd Gen Core Processor HD Graphics 3000" },
    { 0x8086, 0x0101, "2nd Gen Core Processor PCI Express Root Port" },
    { 0x8086, 0x1c03, "6 Series/C200 SATA AHCI Controller" },
    { 0x8086, 0x1c02, "6 Series/C200 SATA IDE Controller" },
    { 0x8086, 0x1c26, "6 Series/C200 USB EHCI Controller #1" },
    { 0x8086, 0x1c2d, "6 Series/C200 USB EHCI Controller #2" },
    { 0x8086, 0x1c20, "6 Series/C200 High Definition Audio Controller" },
    { 0x8086, 0x1c22, "6 Series/C200 SMBus Controller" },
    { 0x8086, 0x1c24, "6 Series/C200 Thermal Management Controller" },
    { 0x8086, 0x1c3a, "6 Series/C200 MEI Controller #1" },
    { 0x8086, 0x1c4f, "QM67 Express LPC Controller" },
    { 0x8086, 0x1c4b, "HM65 Express LPC Controller" },
    { 0x8086, 0x1c10, "6 Series/C200 PCI Express Root Port 1" },
    { 0x8086, 0x1c12, "6 Series/C200 PCI Express Root Port 2" },
    { 0x8086, 0x1c16, "6 Series/C200 PCI Express Root Port 4" },
    { 0x8086, 0x1c1a, "6 Series/C200 PCI Express Root Port 6" },
    { 0x8086, 0x1502, "82579LM Gigabit Network Connection" },
    { 0x8086, 0x1503, "82579V Gigabit Network Connection" },
    { 0x8086, 0x0085, "Centrino Advanced-N 6205" },
    { 0x8086, 0x0084, "Centrino Advanced-N 6205" },
    { 0x10ec, 0x5209, "RTS5209 PCI Express Card Reader" },
    /* ThinkPad T470s -- Kaby Lake-U + Sunrise Point-LP PCH (100 Series) */
    { 0x8086, 0x5904, "Kaby Lake-U Host Bridge / DRAM Registers" },
    { 0x8086, 0x5914, "Kaby Lake-U Host Bridge / DRAM Registers" },
    { 0x8086, 0x5916, "HD Graphics 620 (Kaby Lake GT2)" },
    { 0x8086, 0x5917, "UHD Graphics 620 (Kaby Lake-R GT2)" },
    { 0x8086, 0x9d2f, "Sunrise Point-LP USB 3.0 xHCI Controller" },
    { 0x8086, 0x9d31, "Sunrise Point-LP Thermal subsystem" },
    { 0x8086, 0x9d3a, "Sunrise Point-LP CSME HECI #1" },
    { 0x8086, 0x9d3b, "Sunrise Point-LP CSME HECI #2" },
    { 0x8086, 0x9d03, "Sunrise Point-LP SATA Controller [AHCI mode]" },
    { 0x8086, 0x9d10, "Sunrise Point-LP PCI Express Root Port #1" },
    { 0x8086, 0x9d11, "Sunrise Point-LP PCI Express Root Port #2" },
    { 0x8086, 0x9d12, "Sunrise Point-LP PCI Express Root Port #3" },
    { 0x8086, 0x9d13, "Sunrise Point-LP PCI Express Root Port #4" },
    { 0x8086, 0x9d14, "Sunrise Point-LP PCI Express Root Port #5" },
    { 0x8086, 0x9d15, "Sunrise Point-LP PCI Express Root Port #6" },
    { 0x8086, 0x9d18, "Sunrise Point-LP PCI Express Root Port #9" },
    { 0x8086, 0x9d19, "Sunrise Point-LP PCI Express Root Port #10" },
    { 0x8086, 0x9d1b, "Sunrise Point-LP PCI Express Root Port #12" },
    { 0x8086, 0x9d21, "Sunrise Point-LP PMC" },
    { 0x8086, 0x9d23, "Sunrise Point-LP SMBus" },
    { 0x8086, 0x9d24, "Sunrise Point-LP SPI (flash) Controller" },
    { 0x8086, 0x9d27, "Sunrise Point-LP Serial IO UART #1" },
    { 0x8086, 0x9d2a, "Sunrise Point-LP Serial IO I2C #4" },
    { 0x8086, 0x9d2d, "Sunrise Point-LP Serial IO SPI #0" },
    { 0x8086, 0x9d4b, "Sunrise Point-LP LPC Controller (mHDCP Premium)" },
    { 0x8086, 0x9d4e, "Sunrise Point-LP LPC Controller (Premium)" },
    { 0x8086, 0x9d58, "Sunrise Point-LP LPC Controller" },
    { 0x8086, 0x9d71, "Sunrise Point-LP HD Audio" },
    { 0x8086, 0x15d7, "Ethernet Connection (4) I219-LM" },
    { 0x8086, 0x15d8, "Ethernet Connection (4) I219-V" },
    { 0x8086, 0x15b7, "Ethernet Connection I219-LM" },
    { 0x8086, 0x15b8, "Ethernet Connection I219-V" },
    { 0x8086, 0x15b9, "Ethernet Connection (2) I219-LM" },
    { 0x8086, 0x15e3, "Ethernet Connection (5) I219-LM" },
    { 0x8086, 0x24fd, "Wireless 8265 / 8275" },
    { 0x8086, 0x24f3, "Wireless 8260" },
    { 0x10ec, 0x522a, "RTS522A PCI Express Card Reader" },
    /* NVMe SSDs common in the T470s M.2 slot */
    { 0x144d, 0xa804, "PM961/SM961/PM963 NVMe SSD" },
    { 0x144d, 0xa808, "PM981/PM983 NVMe SSD" },
    { 0x144d, 0xa809, "PM991 NVMe SSD" },
    { 0x1179, 0x0115, "XG5 NVMe SSD" },
    { 0x1c5c, 0x1284, "PC401 NVMe SSD" },
    { 0x1c5c, 0x1327, "PC300 NVMe SSD" },
    { 0x8086, 0xf1a5, "SSD 600p / Pro 6000p Series NVMe" },
    /* MacBook Air 2013 (Haswell-ULT) */
    { 0x8086, 0x0a04, "Haswell-ULT DRAM Controller" },
    { 0x8086, 0x0a16, "Haswell-ULT Host Bridge / DRAM Registers" },
    { 0x8086, 0x0a26, "Haswell-ULT HD Graphics 5000" },
    { 0x8086, 0x9c03, "8 Series SATA Controller (AHCI mode)" },
    { 0x8086, 0x9c22, "8 Series SMBus Controller" },
    { 0x8086, 0x9c41, "8 Series LPC Controller" },
    { 0x8086, 0x9c43, "8 Series Thermal Subsystem" },
    { 0x8086, 0x9ce2, "8 Series USB EHCI #2" },
    { 0x144d, 0x1600, "S4LN053X01 AHCI PCIe SSD (-01600)" },
    { 0x14e4, 0x43a0, "BCM4360 802.11ac Wireless LAN Controller" },
    { 0x1013, 0x4208, "CS4208 HD Audio Codec" },
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
    { 0x8086, 0x9c20, "8 Series HD Audio Controller" },
    { 0x8086, 0x0a0c, "Haswell-ULT HD Audio Controller (HDMI)" },
    /* USB */
    { 0x1b36, 0x000d, "QEMU XHCI Host Controller" },
    { 0x8086, 0x9c31, "8 Series USB xHCI Host Controller" },
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
