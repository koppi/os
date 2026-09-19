/**
 * @file hda.h
 * @brief Intel High Definition Audio (Azalia) — controller bring-up, codec
 *        enumeration and PCM playback. The audio path on every recent laptop:
 *        the ThinkPad X250's analog controller is 8086:9ca0, the MacBook Air
 *        6,2's is 8086:9c20 with a Cirrus CS4208 codec behind it.
 */
#pragma once

#include <types.h>

struct pci_device;

/**
 * @brief PCI bind hook (class 04:03).
 *
 * Resets the controller, sets up CORB/RIRB, picks the best analog output on
 * the first codec by the pins' default configuration, routes it back to a DAC
 * and unmutes every stage. Digital-only HDMI controllers are skipped so the
 * analog one gets the binding, and on Intel the NoSnoop bit is cleared so
 * stream DMA sees cached writes. Boot with `hdadebug` to log every output
 * pin's configuration, or `nosound` / `nohda` to skip the controller.
 */
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

/** @brief Set the codec's output volume, 0..100 % (0 mutes), by writing the
 *         DAC (or output-pin) gain/mute amp. No-op if no codec came up. */
void hda_set_volume(int pct);

/** @return Non-zero if the codec is a Cirrus part in a Mac, whose speaker amp
 *          is driven from a GPIO rather than the pin's EAPD bit. */
int hda_is_apple_cirrus(void);

/* ------------------------------------------------------------------ *
 *  Continuous (streamed) playback                                     *
 *                                                                    *
 *  A cyclic double buffer the caller keeps refilling as it drains --  *
 *  the path the MOD player uses on a real laptop (no SB16 there).     *
 * ------------------------------------------------------------------ */

/**
 * Interleaved stereo 16-bit frames the codec wants filled, per half.
 *
 * 1024 frames is ~23 ms at 44.1 kHz, so the DMA ring holds ~46 ms and that
 * is the floor on output latency. It used to be 4096 (~186 ms), which is
 * fine for music and far too much for a game: apps/doom would fire a shot
 * and hear it a fifth of a second later. The thread that refills this polls
 * every 2 ms, an order of magnitude inside one half, so the smaller window
 * keeps a wide margin against a missed crossing.
 */
#define HDA_STREAM_HALF_FRAMES 1024

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
