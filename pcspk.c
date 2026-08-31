/**
 * @file pcspk.c
 * @brief PC-speaker tones: PIT channel 2 in square-wave mode gated to the
 *        speaker via port 0x61.
 */
#include <pcspk.h>

#include <io.h>

/** Equal-tempered note frequencies (Hz), [octave 0-6][semitone 0-11]. */
static float notes[7][12] = {
    { 130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.0,
        196.0, 207.65, 220.0, 227.31, 246.96 },
    { 261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.63,
        392.0, 415.3, 440.0, 454.62, 493.92 },
    { 523.25, 554.37, 587.33, 622.25, 659.26, 698.46, 739.99,
        783.99, 830.61, 880.0, 909.24, 987.84 },
    { 1046.5, 1108.73, 1174.66, 1244.51, 1328.51, 1396.91, 1479.98,
        1567.98, 1661.22, 1760.0, 1818.48, 1975.68 },
    { 2093.0, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96,
        3135.96, 3322.44, 3520.0, 3636.96, 3951.36 },
    { 4186.0, 4434.92, 4698.64, 4978.04, 5274.04, 5587.86, 5919.92,
        6271.92, 6644.88, 7040.0, 7273.92, 7902.72 },
    { 8372.0, 8869.89, 9397.28,9956.08,10548.08,11175.32, 11839.84,
        12543.84, 13289.76, 14080.0, 14547.84, 15805.44 }
};

/**
 * @brief Sound the note (@p octave, @p note) on the PC speaker.
 * @param octave 0-6.
 * @param note   0-11 (C..B).
 */
void beep_note(uint8_t octave, uint8_t note) {
    beep((int) notes[octave][note]);
}

/**
 * @brief Start a continuous tone.
 * @param value Frequency in Hz; out-of-range values leave the speaker silent
 *              but still gated on.
 */
void beep(int value) {
  unsigned int count = 0;

#define PIT_TICK_RATE 1193182ul
  if (value > 20 && value < 32767)
    count = PIT_TICK_RATE / value;

  outportb(0x43, 0xB6);
  /* Select desired HZ. */
  outportb(0x42, count & 0xff);
  outportb(0x42, (count >> 8) & 0xff);
  /* Enable counter 2. */
  outportb(0x61, inportb(0x61) | 3);
}

/** @brief Silence the speaker (clear the port 0x61 gate bits). */
void beep_off(void) {
  outportb(0x61, inportb(0x61) & 0xFC);
}
