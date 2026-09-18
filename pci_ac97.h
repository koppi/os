/**
 * @file pci_ac97.h
 * @brief Minimal AC'97 audio-codec driver (buffer playback over PCI).
 */
#pragma once

#include <stdint.h>

struct pci_device;

/** @brief PCI probe entry: bring up the AC'97 codec found during enumeration. */
void ac97_probe(struct pci_device *d);
/** @brief Find the AC'97 device on the PCI bus and program its BARs. */
void ac97_init(void);
/** @return Non-zero if an AC'97 codec was found by @ref ac97_init. */
int ac97_present(void);
/** @return The codec's NABM base I/O address. */
uint32_t ac97_get_base(void);
/**
 * @brief Play a PCM buffer through the codec via the buffer-descriptor list.
 * @param buf 16-bit little-endian stereo samples.
 * @param len Length in bytes.
 */
void ac97_play_buffer(uint8_t* buf, uint32_t len);
/** @brief AC'97 interrupt handler (buffer-completion / last-valid-entry). */
void ac97_irq_handler();
