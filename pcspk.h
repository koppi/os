/**
 * @file pcspk.h
 * @brief PC speaker tone generation (PIT channel 2 gated to the speaker port).
 */
#pragma once

#include <types.h>

/** @brief Start a continuous tone at @p frq Hz (0 turns the speaker off). */
void beep(int frq);
/**
 * @brief Start the tone for a note in the equal-tempered scale.
 * @param octave Octave index (0-6).
 * @param note   Semitone within the octave (0-11).
 */
void beep_note(uint8_t octave, uint8_t note);
/** @brief Silence the speaker. */
void beep_off(void);
