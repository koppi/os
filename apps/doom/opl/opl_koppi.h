/*
 * The one call this port adds to the OPL library's interface.
 *
 * Upstream's software backend is an SDL_mixer post-mix hook: SDL_mixer owns
 * the clock and calls it. Here the game's own mixer owns the clock
 * (i_sound_koppi.c), so the relationship is inverted and it asks the
 * synthesiser for samples instead.
 */
#ifndef OPL_KOPPI_H
#define OPL_KOPPI_H

#include <inttypes.h>

/**
 * Render @p nsamples of OPL output into @p dst as interleaved stereo,
 * replacing whatever was there, and run any sequencer callbacks that fall
 * inside the interval. Silence if the synthesiser is not initialised.
 *
 * This is also what advances musical time: nothing else does. A song only
 * progresses as fast as its samples are consumed, which is the property that
 * keeps it in step with the sound effects mixed alongside it.
 */
void OPL_Koppi_Render(int16_t *dst, unsigned int nsamples);

#endif
