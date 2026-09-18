/**
 * @file pci_ac97.c
 * @brief Minimal AC'97 audio driver: locates the codec on PCI, sets up a
 *        buffer-descriptor list and plays PCM buffers via bus-master DMA.
 */
#include <pci.h>
#include <log.h>
#include <io.h>


#include <kheap.h>

#include <idt.h>

#define AC97_VENDOR_INTEL  0x8086
#define AC97_DEVICE_ICH    0x2415

#define AC97_PCI_BAR0      PCI_BAR0

// AC'97 Base Offsets (relative to BAR0)
#define AC97_REG_BDBAR     0x10  // PCM Out Buffer Descriptor List Base Address
#define AC97_REG_CIV       0x14  // PCM Out Current Index Value
#define AC97_REG_LVI       0x15  // PCM Out Last Valid Index
#define AC97_REG_SR        0x16  // PCM Out Status Register
#define AC97_REG_PICB      0x18  // PCM Out Position in Current Buffer
#define AC97_REG_CR        0x1B  // PCM Out Control Register

#define AC97_REG_MASTER_VOL  0x02
#define AC97_REG_PCM_VOL     0x18

#define AC97_REG_GCTL      0x2C  // Global Control
#define AC97_REG_GSTS      0x30  // Global Status

// GCTL bits
#define AC97_GCTL_COLD_RESET  (1 << 1)

// GSTS bits
#define AC97_GSTS_CRDY        (1 << 0) // Codec Ready

// CR bits
#define AC97_CR_START_DMA     0x01 // Start DMA
#define AC97_CR_STOP_DMA      0x02 // Stop DMA
#define AC97_CR_IOC_ENABLE    0x04 // Interrupt on Completion enable

#define BDL_ENTRIES 32

struct bdl_entry {
    uint32_t address;
    uint16_t length;
    uint16_t flags; // b[15] = 1 for Interrupt on Completion
} __attribute__((packed));

static uint32_t ac97_base_addr = 0;
static uint8_t ac97_found = 0;
static volatile struct bdl_entry* bdl;
static volatile uint8_t current_bdl_entry = 0;

extern void ac97_int();

void ac97_irq_handler() {
    
    uint16_t status = inportw(ac97_base_addr + AC97_REG_SR);

    if (status & 0x4) { // Interrupt on completion
        // Acknowledge interrupt
        outportw(ac97_base_addr + AC97_REG_SR, 0x4);
        // You could handle buffer completion here, e.g., by adding a new buffer
    } else {
        outportw(ac97_base_addr + AC97_REG_SR, 0x1F);
    }
}

void ac97_init(void) {
    uint8_t bus, dev, func;
    if (pci_find(AC97_VENDOR_INTEL, AC97_DEVICE_ICH, &bus, &dev, &func)) {
        klogf(LOG_INFO, "AC'97 (82801AA) found at %u:%u.%u\n", bus, dev, func);

        ac97_base_addr = pci_read(bus, dev, func, AC97_PCI_BAR0) & ~0xF;
        klogf(LOG_INFO, "AC'97 base address: 0x%x\n", ac97_base_addr);

        uint32_t pci_cmd = pci_read(bus, dev, func, 0x04);
        pci_cmd |= (1 << 2) | (1 << 0); // Bus Master and I/O Enable
        pci_write(bus, dev, func, 0x04, pci_cmd);

        outportl(ac97_base_addr + AC97_REG_GCTL, AC97_GCTL_COLD_RESET);

        // Set master and PCM volume to a reasonable level (0x0000 is unmuted, max volume)
        outportw(ac97_base_addr + AC97_REG_MASTER_VOL, 0x0000);
        outportw(ac97_base_addr + AC97_REG_PCM_VOL, 0x0000);

        bdl = (struct bdl_entry*)kmalloc(sizeof(struct bdl_entry) * BDL_ENTRIES);
        for (int i = 0; i < BDL_ENTRIES; ++i) {
            bdl[i].address = 0;
            bdl[i].length = 0;
            bdl[i].flags = 0;
        }

        outportl(ac97_base_addr + AC97_REG_BDBAR, (uint32_t)bdl);

        uint8_t irq = pci_read(bus, dev, func, 0x3C) & 0xFF;
        install_ir(irq, 0x8E, 0x8, &ac97_int);
        klogf(LOG_INFO, "AC'97 IRQ: %u\n", irq);

        ac97_found = 1;
    } else {
        klogf(LOG_INFO, "AC'97 (82801AA) not found.\n");
    }
}

void ac97_play_buffer(uint8_t* buf, uint32_t len) {
    if (!ac97_found) return;

    bdl[current_bdl_entry].address = (uint32_t)buf;
    bdl[current_bdl_entry].length = len;
    bdl[current_bdl_entry].flags = 0x8000; // Interrupt on completion

    outportb(ac97_base_addr + AC97_REG_LVI, current_bdl_entry);
    outportb(ac97_base_addr + AC97_REG_CR, AC97_CR_START_DMA | AC97_CR_IOC_ENABLE);

    current_bdl_entry = (current_bdl_entry + 1) % BDL_ENTRIES;
}

int ac97_present(void) { return ac97_found; }
uint32_t ac97_get_base(void) { return ac97_base_addr; }

/** PCI enumeration entry point; ac97_init() re-locates the codec itself. */
void ac97_probe(pci_device_t *d) {
    (void)d;
    ac97_init();
}
