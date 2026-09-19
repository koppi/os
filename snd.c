/**
 * @file snd.c
 * @brief The ring-3 PCM ring described in snd.h.
 */
#include <snd.h>
#include <sb16.h>
#include <hda.h>
#include <lib/string.h>
#include <log.h>

/*
 * The ring size *is* the latency budget. A client fills whatever
 * snd_user_avail reports, so in steady state the ring sits nearly full and
 * everything in it is delay between the moment the program decided to make a
 * noise and the moment it is heard -- on top of the ~46 ms the HD Audio DMA
 * buffer costs (HDA_STREAM_HALF_FRAMES).
 *
 * 4096 frames is 93 ms at 44.1 kHz. Smaller would be tighter, but it also has
 * to cover the gap between two of the producer's writes, and a game on slow
 * hardware may only get round to it every other frame; underrunning sounds
 * far worse than being a frame or two late. This is the knob to turn if the
 * audio feels detached from the action.
 */
#define SND_RING_FRAMES 4096
#define SND_RING_MASK   (SND_RING_FRAMES - 1)

static int16_t  ring[SND_RING_FRAMES * SND_CHANNELS];
static volatile uint32_t ring_head;   /* next write slot  (producer only) */
static volatile uint32_t ring_tail;   /* next read slot   (consumer only) */
static volatile int      user_on;
/*
 * Bumped by every open. The consumer can be part-way through draining when a
 * program closes the stream and the next one opens it -- it would then write
 * back a tail from the previous client, leaving the ring apparently full of
 * whatever was in it. Comparing the generation at the end of a drain with
 * the one at the start turns that into a discarded buffer.
 */
static volatile uint32_t ring_gen;

int snd_user_active(void) { return user_on; }

uint32_t snd_user_open(void) {
    if (user_on)
        return 0;
    /* No card, no stream. sound_backend() reports what the MOD player found:
     * "hda", "sb16" or "none". */
    const char *be = sound_backend();
    if (be[0] == 'n')
        return 0;

    ring_head = ring_tail = 0;
    ring_gen++;
    __sync_synchronize();
    user_on = 1;
    /* The master amp is held at zero while the module is muted, which it is
     * from boot. A program's own audio is not the module, so ask the HD Audio
     * thread to re-evaluate now that one is attached. */
    sound_vol_refresh();
    klogf(LOG_INFO, "snd: ring-3 PCM stream opened on %s\n", be);
    return SND_RATE;
}

void snd_user_close(void) {
    if (!user_on)
        return;
    user_on = 0;
    sound_vol_refresh();
}

uint32_t snd_user_avail(void) {
    if (!user_on)
        return 0;
    uint32_t used = (ring_head - ring_tail) & SND_RING_MASK;
    /* One slot is always left free so head == tail means empty, not full. */
    return SND_RING_FRAMES - 1 - used;
}

uint32_t snd_user_write(const int16_t *frames, uint32_t nframes) {
    if (!user_on || !frames)
        return 0;

    uint32_t room = snd_user_avail();
    if (nframes > room)
        nframes = room;

    uint32_t head = ring_head;
    for (uint32_t i = 0; i < nframes; i++) {
        ring[head * SND_CHANNELS]     = frames[i * SND_CHANNELS];
        ring[head * SND_CHANNELS + 1] = frames[i * SND_CHANNELS + 1];
        head = (head + 1) & SND_RING_MASK;
    }
    /* The samples must be visible before the index that publishes them. x86
     * would not reorder the stores, but nothing stops the compiler from
     * sinking them past a volatile write. */
    __sync_synchronize();
    ring_head = head;
    return nframes;
}

/** @brief Take @p nframes out of the ring, padding with silence. */
static void drain(int16_t *dst, uint32_t nframes, int mono) {
    uint32_t gen  = ring_gen;
    uint32_t tail = ring_tail;
    uint32_t used = (ring_head - tail) & SND_RING_MASK;
    uint32_t n = nframes < used ? nframes : used;

    for (uint32_t i = 0; i < n; i++) {
        int16_t l = ring[tail * SND_CHANNELS];
        int16_t r = ring[tail * SND_CHANNELS + 1];
        if (mono)
            dst[i] = (int16_t) (((int32_t) l + r) / 2);
        else {
            dst[2 * i]     = l;
            dst[2 * i + 1] = r;
        }
        tail = (tail + 1) & SND_RING_MASK;
    }
    __sync_synchronize();          /* reads done before the slots are freed */

    if (ring_gen != gen) {
        /* The stream was reopened under us: what we just read belongs to the
         * previous client, and the tail we would write back would put the
         * new one's ring out of step. Drop the buffer. */
        memset(dst, 0, nframes * (mono ? 1u : 2u) * sizeof(int16_t));
        return;
    }
    ring_tail = tail;

    if (n < nframes) {
        /* Underrun: the program did not keep up. */
        uint32_t rest = nframes - n;
        memset(mono ? (void *) (dst + n) : (void *) (dst + 2 * n), 0,
               rest * (mono ? 1u : 2u) * sizeof(int16_t));
    }
}

int snd_user_pull(int16_t *dst, uint32_t nframes) {
    if (!user_on || !dst)
        return 0;
    drain(dst, nframes, 0);
    return 1;
}

int snd_user_pull_mono(int16_t *dst, uint32_t nframes) {
    if (!user_on || !dst)
        return 0;
    drain(dst, nframes, 1);
    return 1;
}
