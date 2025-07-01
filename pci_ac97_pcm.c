#include <pci.h>
#include <log.h>
#include <io.h>
#include <stddef.h>
#include <stdint.h>
#include "pci_ac97.h"

// AC'97 PCM DMA registers (offsets from BAR0)
#define AC97_REG_BDBAR     0x00  // Buffer Descriptor List Base Address
#define AC97_REG_CIV       0x04  // Current Index Value
#define AC97_REG_LVI       0x05  // Last Valid Index
#define AC97_REG_SR        0x06  // Status Register
#define AC97_REG_PICB      0x08  // Position In Current Buffer
#define AC97_REG_CR        0x0B  // Control Register

// Buffer Descriptor: 8 bytes (addr, length/status)
struct ac97_bdl_entry {
    uint32_t addr;
    uint32_t length_status;
} __attribute__((packed));

// PCM buffer: 16-bit mono, 48kHz, 1/16th of a second
static uint16_t pcm_buffer[3000] __attribute__((aligned(4)));

// Descriptor list for 1 buffer
static struct ac97_bdl_entry bdl[1] __attribute__((aligned(8)));

void ac97_play_pcm_beep(void) {
    uint32_t base = ac97_get_base();
    if (!ac97_present() || !base) {
        klogf(LOG_INFO, "AC'97 not present, cannot play PCM.\n");
        return;
    }

    // Generate 1/16 sec square wave, 48kHz, ~440Hz
    int samples = sizeof(pcm_buffer) / sizeof(pcm_buffer[0]);
    int period = 48000 / 440 / 2; // 440Hz, half-period
    for (int i = 0; i < samples; ++i)
        pcm_buffer[i] = (i / period) % 2 ? 0xC000 : 0x4000;

    // Set up BDL entry
    bdl[0].addr = (uint32_t)pcm_buffer;
    bdl[0].length_status = (samples * sizeof(uint16_t)) | (1 << 31); // IOC interrupt on completion

    // Write BDL base
    outportl(base + AC97_REG_BDBAR, (uint32_t)bdl);

    // LVI = 0 (only one buffer)
    outportb(base + AC97_REG_LVI, 0);

    // Clear status
    outportw(base + AC97_REG_SR, 0xFF);

    // Start playback: set RUN bit
    outportb(base + AC97_REG_CR, 0x1);

    klogf(LOG_INFO, "PCM beep playback started.\n");

    // Wait for playback to finish (poll LVI wrap)
    int timeout = 48000 / 16; // ~1/16s worth of samples
    while (timeout-- > 0) {
        uint8_t civ = inportb(base + AC97_REG_CIV);
        if (civ == 0) break;
        for (volatile int d = 0; d < 500; ++d);
    }

    // Stop playback
    outportb(base + AC97_REG_CR, 0x0);

    klogf(LOG_INFO, "PCM beep playback done.\n");
}