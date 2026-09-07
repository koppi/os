/**
 * @file sb16.h
 * @brief Sound Blaster 16 bring-up and the MOD-music playback glue, plus the
 *        musical-note / waveform constants used by the synth.
 */
#pragma once

#include <types.h>

#define NUM_NOTES 8

#define NUM_OCTAVES 7
#define OCTAVE_SIZE 12

#define OCTAVE_1 0
#define OCTAVE_2 1
#define OCTAVE_3 2
#define OCTAVE_4 3
#define OCTAVE_5 4
#define OCTAVE_6 5
#define OCTAVE_7 6

#define NOTE_C      0
#define NOTE_CS     1
#define NOTE_DF     NOTE_CS
#define NOTE_D      2
#define NOTE_DS     3
#define NOTE_EF     NOTE_DS
#define NOTE_E      4
#define NOTE_F      5
#define NOTE_FS     6
#define NOTE_GF     NOTE_FS
#define NOTE_G      7
#define NOTE_GS     8
#define NOTE_AF     NOTE_GS
#define NOTE_A      9
#define NOTE_AS     10
#define NOTE_BF     NOTE_AS
#define NOTE_B      11

#define NOTE_NONE   12

#define WAVE_SIN        0
#define WAVE_SQUARE     1
#define WAVE_NOISE      2
#define WAVE_TRIANGLE   3

/** @brief Reset the SB16, start MOD playback and install its IRQ handler. */
void sound_init();
/** @brief Pause / resume MOD playback. */
void sound_toggle();
/** @brief SB16 DMA half/full-buffer interrupt handler. */
void sb16_irq_handler();

/** @name Volume / mute keys (driven from the keyboard IRQ, keyboard.c).
 *  Each adjusts the SB16 mixer inline and flags the HD Audio thread to
 *  re-apply the codec amp. @{ */
void sound_volume_up(void);
void sound_volume_down(void);
void sound_mute_toggle(void);
/** @} */
/** @brief Kernel thread: stream MOD playback through the HD Audio codec.
 *         Start it (from @ref main_proc) only when @ref hda_present. */
void sound_hda_thread(void);
