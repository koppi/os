#include <pci.h>
#include <log.h>
#include <io.h>
#include <stddef.h>
#include <stdint.h>

#define AC97_VENDOR_INTEL  0x8086
#define AC97_DEVICE_ICH    0x2415

#define AC97_PCI_BAR0      PCI_BAR0

// AC'97 Base Offsets (relative to BAR0)
#define AC97_REG_BDBAR     0x00  // PCM Output Buffer Descriptor List Base Address Register
#define AC97_REG_CIV       0x04  // Current Index Value Register
#define AC97_REG_LVI       0x05  // Last Valid Index Register
#define AC97_REG_SR        0x06  // Status Register
#define AC97_REG_PICB      0x08  // Position in Current Buffer
#define AC97_REG_CR        0x0B  // Control Register
#define AC97_REG_GCTL      0x2C  // Global Control Register
#define AC97_REG_GSTS      0x30  // Global Status Register

// GCTL bits
#define AC97_GCTL_COLD_RESET  (1 << 1)
#define AC97_GCTL_WARM_RESET  (1 << 0)

// GSTS bits
#define AC97_GSTS_CRDY        (1 << 0) // Codec Ready

static uint32_t ac97_base_addr = 0;
static uint8_t ac97_found = 0;

static int ac97_wait_codec_ready(uint32_t base) {
    // Wait for Codec Ready (CRDY) bit in GSTS
    for (int i = 0; i < 10000; ++i) {
        if (inportl(base + AC97_REG_GSTS) & AC97_GSTS_CRDY)
            return 1;
        for (volatile int delay = 0; delay < 1000; ++delay); // crude delay
    }
    return 0;
}

static void ac97_codec_reset(uint32_t base) {
    // Cold reset
    outportl(base + AC97_REG_GCTL, AC97_GCTL_COLD_RESET);
    for (volatile int i = 0; i < 100000; ++i);
    outportl(base + AC97_REG_GCTL, 0);
    for (volatile int i = 0; i < 100000; ++i);
    // Optionally do a warm reset
    outportl(base + AC97_REG_GCTL, AC97_GCTL_WARM_RESET);
    for (volatile int i = 0; i < 100000; ++i);
    outportl(base + AC97_REG_GCTL, 0);
}

void ac97_init(void) {
    uint8_t bus, dev, func;
    if (pci_find(AC97_VENDOR_INTEL, AC97_DEVICE_ICH, &bus, &dev, &func)) {
        klogf(LOG_INFO, "AC'97 (82801AA) found at %u:%u.%u\n", bus, dev, func);

        // Read BAR0 (base address)
        ac97_base_addr = pci_read(bus, dev, func, AC97_PCI_BAR0) & ~0xF;
        klogf(LOG_INFO, "AC'97 base address: 0x%x\n", ac97_base_addr);

        // Enable Bus Mastering and I/O space in PCI Command register (offset 0x04)
        uint32_t pci_cmd = pci_read(bus, dev, func, 0x04);
        pci_cmd |= (1 << 2) | (1 << 0); // Bus Master and I/O Enable
        pci_write(bus, dev, func, 0x04, pci_cmd);

        // Reset the codec
        ac97_codec_reset(ac97_base_addr);

        // Wait for codec ready
        if (!ac97_wait_codec_ready(ac97_base_addr)) {
            klogf(LOG_EMERG, "AC'97 codec not ready after reset!\n");
            return;
        } else {
            klogf(LOG_INFO, "AC'97 codec ready.\n");
        }

        // --- Advanced: Setup for PCM output (buffer descriptor list) ---
        // (This does not play sound yet, just initializes the DMA engine.)

        // Allocate a simple static Buffer Descriptor List (BDL) in low memory
        // In a real OS, you would allocate this dynamically and ensure it's DMA-safe
        static uint32_t ac97_bdl[32 * 2] __attribute__((aligned(8))); // 32 entries, 8 bytes each (addr, length)
        for (int i = 0; i < 32 * 2; ++i) ac97_bdl[i] = 0;

        // Write BDL base physical address to BDBAR
        outportl(ac97_base_addr + AC97_REG_BDBAR, (uint32_t)ac97_bdl);
        klogf(LOG_INFO, "AC'97 PCM BDL addr set: 0x%x\n", (uint32_t)ac97_bdl);

        // Set Last Valid Index (LVI) to 0 (no buffers yet)
        outportb(ac97_base_addr + AC97_REG_LVI, 0);

        // Clear status
        outportw(ac97_base_addr + AC97_REG_SR, 0xFF);

        // Enable PCM output DMA channel (set Run bit in Control Register)
        outportb(ac97_base_addr + AC97_REG_CR, 0x1);

        klogf(LOG_INFO, "AC'97 PCM output DMA channel enabled.\n");

        ac97_found = 1;
    } else {
        klogf(LOG_INFO, "AC'97 (82801AA) not found.\n");
    }
}

int ac97_present(void) { return ac97_found; }
uint32_t ac97_get_base(void) { return ac97_base_addr; }