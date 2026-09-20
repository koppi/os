/*
 * Sound effects for the koppi-os Doom port.
 *
 * `DG_sound_module`, the hook i_sound.c already has for a platform backend.
 * Upstream doomgeneric fills it with SDL_mixer, which does the mixing; there
 * is no mixer here, so this file is one: eight channels of 8-bit DMX samples
 * resampled and summed into the interleaved stereo frames the kernel's PCM
 * ring takes (snd.h, syscalls 28-31).
 *
 * Two things fall out of how the WAD is loaded (w_file_koppi.c maps it whole
 * and W_CacheLumpNum hands back pointers into it):
 *
 *   - a cached sound costs nothing. There is no need for the sample cache
 *     and eviction machinery the SDL backend carries, and none of the
 *     snd_cachesize accounting; a sfxinfo_t's driver_data is a handful of
 *     bytes describing where in the mapped WAD its samples are.
 *   - nothing is pre-converted to the output rate. Samples stay 8-bit at
 *     their own rate (11025 Hz, usually) and are stepped through with a
 *     16.16 phase accumulator while mixing, which costs an add and a table
 *     lookup per output sample and saves expanding every sound in the game
 *     to eight times its size.
 *
 * Music is not here at all: it is chocolate-doom's OPL support, vendored
 * into opl/ and reached directly by i_sound.c. All this file does for it is
 * mix the synthesiser's output in, which is also what advances the score.
 */
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_misc.h"
#include "deh_str.h"
#include "w_wad.h"
#include "z_zone.h"

#include "opl_koppi.h"

#include "ksys.h"

/* Referenced by i_sound.c's config binding when FEATURE_SOUND is on. The
 * SDL backend uses libsamplerate for rate conversion; this one does not. */
int use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;

/* i_sound.c calls this when native MIDI is selected. Nothing to configure. */
void I_InitTimidityConfig(void) { }

#define NUM_CHANNELS 16          /* snd_channels never exceeds 8 in practice */

/* Frames rendered per pass. 1024 at 44.1 kHz is ~23 ms, one HD Audio half. */
#define MIX_CHUNK 1024

/** A sound effect, pointing into the memory-mapped WAD. */
typedef struct {
    const uint8_t *samples;
    uint32_t       length;    /* samples, not bytes */
    uint32_t       rate;      /* Hz, as the lump declares */
} sfx_data_t;

/** One playing voice. */
typedef struct {
    const sfx_data_t *sfx;
    uint32_t pos;             /* 16.16 position within sfx->samples */
    uint32_t step;            /* 16.16 source samples per output frame */
    int      leftvol, rightvol;   /* 0..127 */
    boolean  playing;
} channel_t;

static channel_t channels[NUM_CHANNELS];
static boolean   sound_initialised;
static boolean   use_sfx_prefix;
static uint32_t  out_rate;
static int16_t   mixbuf[MIX_CHUNK * 2];

/* ------------------------------------------------------------------ *
 *  Loading                                                            *
 * ------------------------------------------------------------------ */

static void GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len) {
    if (sfx->link != NULL)
        sfx = sfx->link;

    /* Doom prefixes its sound lumps with DS; Heretic and Hexen do not. */
    if (use_sfx_prefix)
        M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
    else
        M_StringCopy(buf, DEH_String(sfx->name), buf_len);
}

/*
 * Parse one DMX sound lump: a 16-bit format tag (3), a 16-bit sample rate, a
 * 32-bit length, then that many unsigned 8-bit samples.
 *
 * The two odd-looking rules are DMX's, and chocolate-doom keeps them for the
 * same reason: a lump of 48 samples or fewer is ignored (the original
 * library does not play them), and 16 samples are trimmed from each end,
 * where WADs carry padding that would otherwise click.
 */
static boolean CacheSFX(sfxinfo_t *sfxinfo) {
    if (sfxinfo->driver_data != NULL)
        return true;

    int lumpnum = sfxinfo->lumpnum;
    const uint8_t *data = W_CacheLumpNum(lumpnum, PU_STATIC);
    unsigned int lumplen = W_LumpLength(lumpnum);

    if (lumplen < 8 || data[0] != 0x03 || data[1] != 0x00)
        return false;

    uint32_t rate   = (uint32_t) ((data[3] << 8) | data[2]);
    uint32_t length = ((uint32_t) data[7] << 24) | ((uint32_t) data[6] << 16) |
                      ((uint32_t) data[5] << 8)  |  (uint32_t) data[4];

    if (length > lumplen - 8 || length <= 48)
        return false;
    if (rate == 0)
        return false;

    sfx_data_t *sfx = Z_Malloc(sizeof(sfx_data_t), PU_STATIC, NULL);
    sfx->samples = data + 8 + 16;
    sfx->length  = length - 32;
    sfx->rate    = rate;

    sfxinfo->driver_data = sfx;
    return true;
}

/* ------------------------------------------------------------------ *
 *  Mixing                                                             *
 * ------------------------------------------------------------------ */

/*
 * Vanilla's panning curve, kept because it is what the game was voiced for:
 * sep runs 0 (hard left) to 254 (hard right) and each side falls off with
 * the square of the distance from it.
 */
static void SetChannelVolume(channel_t *ch, int vol, int sep) {
    int s = sep + 1;
    ch->leftvol = vol - ((vol * s * s) >> 16);
    s = sep - 257;
    ch->rightvol = vol - ((vol * s * s) >> 16);

    if (ch->leftvol  < 0) ch->leftvol  = 0;
    if (ch->rightvol < 0) ch->rightvol = 0;
}

static int16_t clip16(int32_t v) {
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return (int16_t) v;
}

/** @brief Sum the music and every playing channel into @p nframes of
 *         interleaved stereo. */
static void MixChunk(int16_t *dst, uint32_t nframes) {
    static int32_t accl[MIX_CHUNK], accr[MIX_CHUNK];
    static int16_t oplbuf[MIX_CHUNK * 2];

    /* Music first, and it starts the accumulator rather than being added to
     * it. No attenuation here: the OPL module already scales note velocities
     * by the music volume the player set, and a second fixed reduction on
     * top would just move the balance away from what the engine intends.
     *
     * Rendering is also what moves the sequencer forward, so it happens
     * every chunk whether or not a song is playing. */
    OPL_Koppi_Render(oplbuf, nframes);
    for (uint32_t i = 0; i < nframes; i++) {
        accl[i] = oplbuf[2 * i];
        accr[i] = oplbuf[2 * i + 1];
    }

    for (int c = 0; c < NUM_CHANNELS; c++) {
        channel_t *ch = &channels[c];
        if (!ch->playing)
            continue;

        const uint8_t *src = ch->sfx->samples;
        uint32_t end = ch->sfx->length << 16;
        uint32_t pos = ch->pos, step = ch->step;
        int lv = ch->leftvol, rv = ch->rightvol;

        uint32_t i = 0;
        for (; i < nframes && pos < end; i++, pos += step) {
            /* 8-bit unsigned -> signed 16-bit, then scaled by the 0..127
             * volume the engine computed for this side. */
            int32_t s = ((int32_t) src[pos >> 16] - 128) << 8;
            accl[i] += (s * lv) >> 7;
            accr[i] += (s * rv) >> 7;
        }

        ch->pos = pos;
        if (pos >= end)
            ch->playing = false;
    }

    for (uint32_t i = 0; i < nframes; i++) {
        dst[2 * i]     = clip16(accl[i]);
        dst[2 * i + 1] = clip16(accr[i]);
    }
}

/* ------------------------------------------------------------------ *
 *  The module                                                         *
 * ------------------------------------------------------------------ */

static boolean I_Koppi_InitSound(boolean sfx_prefix) {
    use_sfx_prefix = sfx_prefix;

    out_rate = (uint32_t) ksys0(SYS_SND_OPEN);
    if (out_rate == 0) {
        printf("I_InitSound: no sound card, or another program has it\n");
        return false;
    }

    memset(channels, 0, sizeof(channels));
    sound_initialised = true;
    printf("I_InitSound: %u Hz stereo through the kernel PCM ring\n", out_rate);
    return true;
}

static void I_Koppi_ShutdownSound(void) {
    if (!sound_initialised)
        return;
    sound_initialised = false;
    ksys0(SYS_SND_CLOSE);
}

static int I_Koppi_GetSfxLumpNum(sfxinfo_t *sfx) {
    char namebuf[9];
    GetSfxLumpName(sfx, namebuf, sizeof(namebuf));
    return W_GetNumForName(namebuf);
}

/*
 * Render exactly as much as the kernel ring has room for and no more. That is
 * the whole pacing policy -- the ring drains at the sample rate, so it is the
 * clock, and the mixer follows it at whatever frame rate the game happens to
 * be running. How far ahead this ends up working is the ring's size, which is
 * where the output latency is decided (snd.c), not here.
 */
static void I_Koppi_UpdateSound(void) {
    if (!sound_initialised)
        return;

    uint32_t room = (uint32_t) ksys0(SYS_SND_AVAIL);
    while (room > 0) {
        uint32_t n = room > MIX_CHUNK ? MIX_CHUNK : room;
        MixChunk(mixbuf, n);
        uint32_t wrote = (uint32_t) ksys2(SYS_SND_WRITE,
                                          (unsigned long) mixbuf, n);
        if (wrote == 0)
            break;
        room -= wrote;
    }
}

static void I_Koppi_UpdateSoundParams(int handle, int vol, int sep) {
    if (!sound_initialised || handle < 0 || handle >= NUM_CHANNELS)
        return;
    SetChannelVolume(&channels[handle], vol, sep);
}

static int I_Koppi_StartSound(sfxinfo_t *sfxinfo, int channel,
                              int vol, int sep) {
    if (!sound_initialised || channel < 0 || channel >= NUM_CHANNELS)
        return -1;
    if (!CacheSFX(sfxinfo))
        return -1;

    const sfx_data_t *sfx = sfxinfo->driver_data;
    channel_t *ch = &channels[channel];

    ch->sfx  = sfx;
    ch->pos  = 0;
    /* 16.16 source samples per output frame: at 11025 Hz into 44100 Hz this
     * is 0.25, so each source sample is held for four frames. */
    ch->step = (uint32_t) (((uint64_t) sfx->rate << 16) / out_rate);
    if (ch->step == 0)
        ch->step = 1;
    SetChannelVolume(ch, vol, sep);
    ch->playing = true;

    /* The engine uses the return value as the handle it passes back to
     * StopSound / SoundIsPlaying / UpdateSoundParams. */
    return channel;
}

static void I_Koppi_StopSound(int handle) {
    if (!sound_initialised || handle < 0 || handle >= NUM_CHANNELS)
        return;
    channels[handle].playing = false;
}

static boolean I_Koppi_SoundIsPlaying(int handle) {
    if (!sound_initialised || handle < 0 || handle >= NUM_CHANNELS)
        return false;
    return channels[handle].playing;
}

/*
 * Nothing to precache: W_CacheLumpNum on a memory-mapped WAD is a pointer
 * into the mapping, so a sound costs nothing until it is first played and
 * nothing after. Left as a no-op rather than walking every sound at startup
 * for no gain.
 */
static void I_Koppi_PrecacheSounds(sfxinfo_t *sounds, int num_sounds) {
    (void) sounds;
    (void) num_sounds;
}

static snddevice_t sound_devices[] = {
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_AWE32,
};

sound_module_t DG_sound_module = {
    sound_devices,
    arrlen(sound_devices),
    I_Koppi_InitSound,
    I_Koppi_ShutdownSound,
    I_Koppi_GetSfxLumpNum,
    I_Koppi_UpdateSound,
    I_Koppi_UpdateSoundParams,
    I_Koppi_StartSound,
    I_Koppi_StopSound,
    I_Koppi_SoundIsPlaying,
    I_Koppi_PrecacheSounds,
};

/* ------------------------------------------------------------------ *
 *  Music                                                              *
 * ------------------------------------------------------------------ */

/*
 * Doom's music is MUS -- a packed MIDI -- and what turned it into sound was
 * an OPL2 chip. chocolate-doom's emulation of one is vendored in opl/ and
 * used as it stands; i_sound.c reaches for music_opl_module directly, so
 * there is no DG_music_module here to forward to it.
 *
 * All this file does for music is let the synthesiser into the mix, which
 * MixChunk does above. Musical time advances as those samples are rendered,
 * so the score cannot drift away from the shots fired over it.
 */
