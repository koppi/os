/**
 * @file snd.h
 * @brief PCM output for ring 3: a ring buffer a userspace program fills and
 *        whichever sound card is present drains.
 *
 * The audio drivers here are *pull* devices — the HD Audio thread refills a
 * cyclic DMA buffer, the Sound Blaster's IRQ refills half of one — and both
 * have always been fed by the in-kernel MOD player. A game cannot be driven
 * that way: it produces its audio when it produces a frame, on its own
 * schedule, in ring 3. This is the buffer between the two. A program opens
 * the stream, writes interleaved stereo frames into it whenever it has some,
 * and asks how much room is left so it knows how much to render; the driver
 * takes frames out at the sample rate and plays silence if the program falls
 * behind.
 *
 * One client at a time, and it owns the output while it is open: the module
 * is not rendered at all then, so the game is not playing over music
 * (@ref sb16.c). Single-producer / single-consumer, like the keyboard rings —
 * the producer is the syscall, the consumer is the HD Audio thread or the
 * SB16 interrupt, never both (see @c hda_active).
 */
#pragma once

#include <types.h>

/** The rate, channel count and sample format every backend here runs at. */
#define SND_RATE      44100
#define SND_CHANNELS  2

/**
 * @brief Claim the output for a ring-3 program.
 * @return The sample rate it must produce, or 0 if there is no sound card or
 *         another program already has it. Frames are interleaved stereo
 *         signed 16-bit.
 */
uint32_t snd_user_open(void);

/** @brief Release the output; the module becomes audible again. */
void snd_user_close(void);

/** @return Non-zero while a program holds the output. */
int snd_user_active(void);

/** @return Room left in the ring, in frames. */
uint32_t snd_user_avail(void);

/**
 * @brief Queue up to @p nframes interleaved stereo frames.
 * @return How many were taken; fewer than asked means the ring is full and
 *         the caller should keep the rest (or drop it and carry on).
 */
uint32_t snd_user_write(const int16_t *frames, uint32_t nframes);

/**
 * @name Driver side
 *
 * Called from whichever backend is driving the card. Both fill with silence
 * rather than returning short when the program has fallen behind: a hole in
 * the stream has to be *something*, and stale audio is worse than a gap.
 */
///@{
/** @brief Fill @p dst with @p nframes interleaved stereo frames.
 *         @return 0 if no program holds the output (fill it yourself). */
int snd_user_pull(int16_t *dst, uint32_t nframes);
/** @brief As @ref snd_user_pull, but downmixed to mono for the SB16. */
int snd_user_pull_mono(int16_t *dst, uint32_t nframes);
///@}
