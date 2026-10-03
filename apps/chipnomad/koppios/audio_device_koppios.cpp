/*
 * ChipNomad's AudioDevice on koppi-os.
 *
 * The kernel's PCM output is a push device: a program claims it, asks how
 * much room the ring has (snd_avail), and writes interleaved stereo frames
 * into it (snd_write); whichever card is present drains the ring at
 * 44.1 kHz and plays silence if the program falls behind (snd.h). ChipNomad's
 * AudioDevice is the opposite shape -- it expects to be *called* with a
 * buffer to fill, the way SDL's audio thread calls it.
 *
 * pump() is the adapter, and the ring is the clock: render exactly as much as
 * there is room for and no more, which is the same pacing policy
 * apps/doom/i_sound_koppi.c arrived at. The output latency is then the ring's
 * size, decided in snd.c, not here.
 *
 * There is no audio thread. This kernel has userspace threads (syscalls
 * 32-35) and the tracker would be a reasonable use for one, but a process
 * gets a single CPU (lib/pthread.c's note on process_t::cpu), so a render
 * thread would be interleaved with the UI by the scheduler rather than run
 * beside it -- the same total work, with a shared Engine to protect. Pumping
 * from the frame loop keeps the engine single-threaded, which is what
 * ChipNomad's own comments say it assumes.
 */
#include <stdio.h>
#include <stdlib.h>

#include "audio_device_koppios.h"
#include "ksys.h"

AudioDeviceKoppiOS::~AudioDeviceKoppiOS() {
    teardown();
}

bool AudioDeviceKoppiOS::setup(AudioCallbacks* callbacks, int sampleRate, int bufferSize) {
    if (open)
        teardown();

    cb = callbacks;

    rate = (int) ksys0(SYS_SND_OPEN);
    if (rate <= 0) {
        fprintf(stderr, "chipnomad: no sound card (or another program has it); "
                        "running silent\n");
        rate = 0;
        return false;
    }

    /*
     * The kernel fixes the rate at SND_RATE; ChipNomad asks for whatever its
     * settings say. Report back what the hardware actually runs at rather
     * than resampling: the engine is told this rate and generates at it.
     */
    if (sampleRate != rate) {
        fprintf(stderr, "chipnomad: sound card runs at %d Hz, not %d; using %d\n",
                rate, sampleRate, rate);
    }

    mixFrames = bufferSize > 0 ? bufferSize : 1024;
    mixBuffer = (int16_t*) malloc((size_t) mixFrames * 2 * sizeof(int16_t));
    if (!mixBuffer) {
        ksys0(SYS_SND_CLOSE);
        rate = 0;
        return false;
    }

    open = true;
    paused = false;
    return true;
}

void AudioDeviceKoppiOS::pause(bool isPaused) {
    paused = isPaused;
}

void AudioDeviceKoppiOS::teardown() {
    if (open) {
        ksys0(SYS_SND_CLOSE);
        open = false;
    }
    free(mixBuffer);
    mixBuffer = nullptr;
    cb = nullptr;
    rate = 0;
}

void AudioDeviceKoppiOS::pump() {
    if (!open || paused || !cb || !mixBuffer)
        return;

    unsigned room = (unsigned) ksys0(SYS_SND_AVAIL);
    while (room > 0) {
        unsigned n = room > (unsigned) mixFrames ? (unsigned) mixFrames : room;
        cb->onAudioOutput(mixBuffer, (int) n);
        unsigned wrote = (unsigned) ksys2(SYS_SND_WRITE,
                                         (unsigned long) mixBuffer,
                                         (unsigned long) n);
        if (wrote == 0)
            break;
        room -= wrote;
    }
}
