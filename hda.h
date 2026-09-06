/**
 * @file hda.h
 * @brief Intel High Definition Audio (Azalia) — controller bring-up, codec
 *        enumeration and one-shot PCM playback. The audio path on every recent
 *        laptop (the ThinkPad X250's analog codec is 8086:9ca0).
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief PCI bind hook (class 04:03): reset the controller, set up CORB/RIRB,
 *         find a DAC -> output-pin path on the first codec and unmute it. */
void hda_probe(struct pci_device *d);

/** @return non-zero once a codec output path is ready. */
int hda_present(void);

/**
 * @brief Play @p nframes interleaved stereo 16-bit samples at @p rate Hz.
 *        Blocks for the duration of the clip. No-op if no codec came up.
 */
void hda_play_pcm(const int16_t *samples, uint32_t nframes, uint32_t rate);

/** @brief Play a @p ms-long square-wave tone at @p freq Hz through the codec. */
void hda_beep(uint32_t freq, uint32_t ms);

/* ------------------------------------------------------------------ *
 *  Continuous (streamed) playback                                     *
 *                                                                    *
 *  A cyclic double buffer the caller keeps refilling as it drains --  *
 *  the path the MOD player uses on a real laptop (no SB16 there).     *
 * ------------------------------------------------------------------ */

/** @brief Interleaved stereo 16-bit frames the codec wants filled. */
#define HDA_STREAM_HALF_FRAMES 4096

/**
 * @brief Start output stream 0 looping a silent cyclic buffer at @p rate Hz
 *        (stereo, 16-bit). @return non-zero once the DMA engine is running.
 */
int  hda_stream_start(uint32_t rate);

/** @brief Stop the streamed-playback DMA engine. */
void hda_stream_stop(void);

/**
 * @brief Poll the DMA position and, for each half the codec has finished with,
 *        call @p fill(dst, HDA_STREAM_HALF_FRAMES) to refill it (interleaved
 *        stereo 16-bit). Cheap to call often; a no-op until a half drains.
 */
void hda_stream_service(void (*fill)(int16_t *dst, uint32_t nframes));
