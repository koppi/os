/**
 * @file sb16.c
 * @brief Sound Blaster 16 driver plus the glue that feeds the hxcmod MOD
 *        player's output to the card over auto-init DMA.
 */
#include <sb16.h>
#include <io.h>
#include <lib/string.h>
#include <idt.h>
#include <log.h>
#include <hda.h>
#include <snd.h>

#include <hxcmod.h>
#include <modfile.h>

#define MIXER_IRQ       0x5
#define MIXER_IRQ_DATA  0x2

// SB16 ports
#define DSP_MIXER       0x224
#define DSP_MIXER_DATA  0x225
#define DSP_RESET       0x226
#define DSP_READ        0x22A
#define DSP_WRITE       0x22C
#define DSP_READ_STATUS 0x22E
#define DSP_ACK_8       DSP_READ_STATUS
#define DSP_ACK_16      0x22F

// TODO: ???
#define DSP_PROG_16     0xB0
#define DSP_PROG_8      0xC0
#define DSP_AUTO_INIT   0x06
#define DSP_PLAY        0x00
#define DSP_RECORD      0x08
#define DSP_MONO        0x00
#define DSP_STEREO      0x20
#define DSP_UNSIGNED    0x00
#define DSP_SIGNED      0x10

#define DMA_CHANNEL_16  5
#define DMA_FLIP_FLOP   0xD8
#define DMA_BASE_ADDR   0xC4
#define DMA_COUNT       0xC6

// commands for DSP_WRITE
#define DSP_SET_TIME    0x40
#define DSP_SET_RATE    0x41
#define DSP_ON          0xD1
#define DSP_OFF         0xD3
#define DSP_OFF_8       0xD0
#define DSP_ON_8        0xD4
#define DSP_OFF_16      0xD5
#define DSP_ON_16       0xD6
#define DSP_VERSION     0xE1

// commands for DSP_MIXER
#define DSP_VOLUME  0x22
#define DSP_IRQ     0x80

#define SAMPLE_RATE     44100
#define BUFFER_MS       50

#define BUFFER_SIZE 2048*20 //((size_t) (SAMPLE_RATE * (BUFFER_MS / 1000.0)))
static __attribute__ ((aligned (4096))) short int buffer[BUFFER_SIZE];
static uint8_t buffer_flip = 0;

static modcontext modctx;

static uint8_t sound_muted = 1;

/* Set once MOD playback has been handed to the HD Audio codec (see
 * sound_hda_thread). The SB16 DMA handler then feeds the card silence so the
 * module is never heard twice on a box that has both. */
static uint8_t hda_active = 0;

/* Set once the SB16 DSP has been reset successfully (sound_init). Gates the
 * mixer writes so a box without a Sound Blaster never pokes ports 0x224/0x225. */
static uint8_t sb16_ok = 0;

/* Master playback level, 0..SOUND_VOL_MAX. The volume keys (keyboard.c) step
 * it. sb16.c drives the SB16 mixer inline (cheap port writes, IRQ-safe) and
 * raises sound_vol_dirty so the HD Audio thread re-applies the codec amp — a
 * CORB/RIRB verb must not be issued from the keyboard IRQ. */
#define SOUND_VOL_MAX 16
static int          sound_vol       = 12;   /* ~75 % at boot */
static volatile int sound_vol_dirty = 1;

/** @return the master level as a 0..100 percentage. */
static int sound_vol_pct(void) { return sound_vol * 100 / SOUND_VOL_MAX; }

/*
 * "Muted" means the *module* is not wanted, which is the state the machine
 * boots in. It must not also silence a ring-3 program that has opened the
 * PCM stream (snd.c) -- that program's audio is not the module. So the
 * output amp is only pulled to zero when nothing else is using it.
 */
static int output_silent(void) {
    return (sound_muted || sound_vol == 0) && !snd_user_active();
}

/** @brief Write the current level to the SB16 master mixer (reg 0x22: 4-bit
 *         left | 4-bit right). Muted or level 0 -> silent. IRQ-safe. */
static void sb16_apply_mixer(void) {
    if (!sb16_ok)
        return;
    int v = output_silent() ? 0 : (sound_vol * 15 / SOUND_VOL_MAX);
    outportb(DSP_MIXER, DSP_VOLUME);
    outportb(DSP_MIXER_DATA, (uint8_t) ((v << 4) | v));
}

/**
 * @brief Re-apply the master level on the next pass of the HD Audio thread.
 *
 * snd.c calls this when a ring-3 program takes or releases the output: the
 * codec amp has to follow, and a CORB/RIRB verb cannot be issued from
 * whatever context that syscall is running in.
 */
void sound_vol_refresh(void) {
    sound_vol_dirty = 1;
    sb16_apply_mixer();
}

/** @brief Render @p len samples of MOD audio into @p buf. */
static void fill(short int *buf, size_t len) {
    //printf("sound: fill len: %d\n", len);
    hxcmod_fillbuffer(&modctx, buf, len, NULL);
}

/** @brief Toggle MOD playback on/off (a muted transfer sends silence). */
void sound_toggle() {
    sound_muted = !sound_muted;
    sound_vol_dirty = 1;
    sb16_apply_mixer();
}

/** @brief Volume-Up key: raise the master level one step (and unmute). */
void sound_volume_up(void) {
    if (sound_vol < SOUND_VOL_MAX)
        sound_vol++;
    sound_muted = 0;
    sound_vol_dirty = 1;
    sb16_apply_mixer();
    klogf(LOG_INFO, "sound: volume %d%%\n", sound_vol_pct());
}

/** @brief Volume-Down key: lower the master level one step. */
void sound_volume_down(void) {
    if (sound_vol > 0)
        sound_vol--;
    sound_vol_dirty = 1;
    sb16_apply_mixer();
    klogf(LOG_INFO, "sound: volume %d%%\n", sound_vol_pct());
}

/** @brief Mute key: same effect as the desktop's sound on/off button. */
void sound_mute_toggle(void) {
    sound_toggle();
    klogf(LOG_INFO, "sound: %s\n", sound_muted ? "muted" : "unmuted");
}

/* ------------------------------------------------------------------ *
 *  Console control                                                    *
 *                                                                    *
 *  The volume keys above arrive through the PS/2 controller, which a  *
 *  MacBook does not have -- its keyboard is a USB HID device on the   *
 *  xHCI bus. Without these the only way to unmute on such a machine   *
 *  is the desktop's "sound on" button, which needs the mouse.         *
 * ------------------------------------------------------------------ */

/** @return Non-zero while MOD playback is muted. */
int sound_is_muted(void) { return sound_muted; }

/** @return The master level as a 0..100 percentage, 0 while muted. */
int sound_volume_pct(void) { return sound_muted ? 0 : sound_vol_pct(); }

/**
 * @brief Set the master level.
 * @param pct 0..100; 0 mutes, anything above unmutes.
 */
void sound_set_volume(int pct) {
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    sound_vol   = (pct * SOUND_VOL_MAX + 50) / 100;
    sound_muted = (pct == 0);
    sound_vol_dirty = 1;
    sb16_apply_mixer();
}

/** @brief Mute or unmute without disturbing the level. */
void sound_set_muted(int muted) {
    sound_muted = muted ? 1 : 0;
    sound_vol_dirty = 1;
    sb16_apply_mixer();
}

/** @return The title of the loaded MOD, for the console's status line. */
const char *sound_module_title(void) { return (const char *) modctx.song.title; }

/** @return Which back end MOD audio is going to: "hda", "sb16" or "none". */
const char *sound_backend(void) {
    if (hda_active) return "hda";
    if (sb16_ok)    return "sb16";
    return "none";
}

/** @brief Wait for the DSP write buffer to drain, then send command byte @p b. */
static void dsp_write(uint8_t b) {
    while (inportb(DSP_WRITE) & 0x80);
    outportb(DSP_WRITE, b);
}

/*
static void dsp_read(uint8_t b) {
    while (inportb(DSP_READ_STATUS) & 0x80);
    outportb(DSP_READ, b);
}*/

/**
 * @brief Read one DSP byte, giving up after a bounded spin.
 * @param b Out: the byte read.
 * @return 1 on success, 0 on timeout.
 */
static uint8_t dsp_detect_timeout(uint8_t* b) {
    for (size_t i = 0; i < 1000000; i++) {
        if (inportb(DSP_READ_STATUS) & 0x80) {
            *b = inportb(DSP_READ);
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief Reset the SB16 DSP and check it reports version >= 4.
 * @return 0 on success, 1 on failure (logged).
 */
static uint8_t reset() {
    uint8_t status = 0;

    outportb(DSP_RESET, 1);

    // TODO: maybe not necessary
    // ~3 microseconds?
    for (size_t i = 0; i < 1000000; i++);

    outportb(DSP_RESET, 0);

    if (!dsp_detect_timeout(&status) || status != 0xAA) {
        goto fail;
    }

    outportb(DSP_WRITE, DSP_VERSION);
    uint8_t major = inportb(DSP_READ),
       minor = inportb(DSP_READ);

    if (major < 4) {
        status = (major << 4) | minor;
        goto fail;
    }

    return 0;
fail:
    klogf(LOG_ERR, "Failed to reset SB16: %d\n", 128);
    return 1;
}

/** @brief Program the DSP output sample rate to @p hz. */
static void set_sample_rate(uint16_t hz) {
    dsp_write(DSP_SET_RATE);
    dsp_write((uint8_t) ((hz >> 8) & 0xFF));
    dsp_write((uint8_t) (hz & 0xFF));
}

/**
 * @brief Program the 16-bit DMA channel for an auto-init transfer of @p buf.
 * @param buf Physical buffer address (must not cross a 64 KiB page).
 * @param len Transfer length in bytes.
 */
static void transfer(void *buf, uint32_t len) {
    uint8_t mode = 0x48;

    // disable DMA channel
    outportb(DSP_ON_8, 4 + (DMA_CHANNEL_16 % 4));

    // clear byte-poiner flip-flop
    outportb(DMA_FLIP_FLOP, 0);

    // write DMA mode for transfer
    outportb(DSP_ON_16, (DMA_CHANNEL_16 % 4) | mode | (1 << 4));

    // write buffer offset (div 2 for 16-bit)
    unsigned short offset = (((uintptr_t) buf) / 2) & 0xFFFF;
    outportb(DMA_BASE_ADDR, (uint8_t) ((offset >> 0) & 0xFF));
    outportb(DMA_BASE_ADDR, (uint8_t) ((offset >> 8) & 0xFF));

    // write transfer length
    outportb(DMA_COUNT, (uint8_t) (((len - 1) >> 0) & 0xFF));
    outportb(DMA_COUNT, (uint8_t) (((len - 1) >> 8) & 0xFF));

    // write buffer page
    outportb(0x8B, ((uintptr_t) buf) >> 16);

    // enable DMA channel
    outportb(DSP_ON_8, DMA_CHANNEL_16 % 4);
}

/**
 * @brief DMA half/full-buffer interrupt: refill the half that just played
 *        (or zero it while muted) and acknowledge the card.
 */
void sb16_irq_handler() {
    buffer_flip = !buffer_flip;
    short int *half = &buffer[buffer_flip ? 0 : (BUFFER_SIZE / 2)];

    /* A ring-3 program that has opened the PCM stream owns the output: its
     * frames go out instead of the module's, downmixed because the card is
     * running mono. hda_active still wins -- on a box with both, the codec
     * is the one being listened to and this card is fed silence. */
    if (!hda_active && snd_user_pull_mono(half, BUFFER_SIZE / 2)) {
        /* filled from the ring */
    } else if (!sound_muted && !hda_active) {
        fill(half, (BUFFER_SIZE / 2));
    } else {
        memset(buffer, 0, BUFFER_SIZE*sizeof(short int));
    }

    inportb(DSP_READ_STATUS);
    inportb(DSP_ACK_16);
}

/** asm IRQ stub (sound_asm) that calls @ref sb16_irq_handler. */
extern void sound_int();

/** @brief Install the SB16 IRQ handler and route the card's IRQ via the mixer. */
static void configure() {
    install_ir(32 + MIXER_IRQ, 0x80 | 0x0E, 0x8, &sound_int);

    outportb(DSP_MIXER, DSP_IRQ);
    outportb(DSP_MIXER_DATA, MIXER_IRQ_DATA);
}

void sound_init() {
    //klogf(LOG_DEBUG, "sound_init()\n");
    
    hxcmod_init(&modctx);
    hxcmod_load(&modctx, modfile_0, modfile_0_size);

    install_ir(32 + MIXER_IRQ, 0x80 | 0x0E, 0x8, &sound_int);
    if (reset() == 0)
    {
        sb16_ok = 1;
        configure();

        transfer(buffer, BUFFER_SIZE);
        set_sample_rate(SAMPLE_RATE);
        sb16_apply_mixer();

        uint16_t sample_count = (BUFFER_SIZE / 2) - 1;
        dsp_write(DSP_PLAY | DSP_PROG_16 | DSP_AUTO_INIT);
        dsp_write(DSP_SIGNED | DSP_MONO);
        dsp_write((uint8_t) ((sample_count >> 0) & 0xFF));
        dsp_write((uint8_t) ((sample_count >> 8) & 0xFF));

        dsp_write(DSP_ON);
        dsp_write(DSP_ON_16);

        printf("playing module '%s'.\n", modctx.song.title);
        klogf(LOG_DEBUG, "sound_init() ok.\n");
    }
}

/* ------------------------------------------------------------------ *
 *  HD Audio path                                                      *
 *                                                                    *
 *  A real laptop has no Sound Blaster: when the PCI scan turns up an  *
 *  Azalia controller with a working codec, MOD playback moves onto it.*
 *  The hxcmod core still renders mono; we fan it out to both channels *
 *  of the codec's stereo stream.                                      *
 * ------------------------------------------------------------------ */

/** @brief Refill one half of the HD Audio ring: render mono MOD audio, then
 *         duplicate it across the interleaved stereo frames the codec wants. */
static void hda_fill(int16_t *dst, uint32_t nframes) {
    /* A ring-3 program streaming PCM (snd.c) owns the output while it is
     * open; the module is not rendered at all then, so a game is never
     * heard over the music. */
    if (snd_user_pull(dst, nframes))
        return;

    if (sound_muted) {
        memset(dst, 0, nframes * 2 * sizeof(int16_t));
        return;
    }

    static short mono[HDA_STREAM_HALF_FRAMES];
    uint32_t n = nframes > HDA_STREAM_HALF_FRAMES ? HDA_STREAM_HALF_FRAMES
                                                  : nframes;
    hxcmod_fillbuffer(&modctx, mono, n, NULL);
    for (uint32_t i = 0; i < n; i++)
        dst[2 * i] = dst[2 * i + 1] = mono[i];
}

/**
 * @brief Kernel thread: start the HD Audio output stream and keep its cyclic
 *        buffer fed from the MOD player. Started from @ref main_proc only when
 *        @ref hda_present, so a Sound Blaster box never gets here.
 */
void sound_hda_thread(void) {
    if (!hda_stream_start(SAMPLE_RATE)) {
        klogf(LOG_WARNING, "sound: HD Audio stream would not start\n");
        while (1) sleep(1000);
    }

    hda_active = 1;                    /* hush the SB16 handler, if it is live */
    klogf(LOG_INFO, "sound: MOD playback routed through HD Audio\n");
    printf("playing module '%s' through HD Audio.\n", modctx.song.title);

    for (;;) {
        if (sound_vol_dirty) {
            sound_vol_dirty = 0;       /* apply the codec amp off the IRQ path */
            hda_set_volume(output_silent() ? 0 : sound_vol_pct());
        }
        hda_stream_service(hda_fill);
        sleep(2);                      /* one half is ~23 ms; poll well inside */
    }
}
