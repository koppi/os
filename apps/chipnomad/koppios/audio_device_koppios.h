/*
 * AudioDevice over the kernel's PCM ring (syscalls 28-31, snd.c).
 */
#ifndef __AUDIO_DEVICE_KOPPIOS_H__
#define __AUDIO_DEVICE_KOPPIOS_H__

#include <stdint.h>

#include "audio_device.h"

class AudioDeviceKoppiOS : public AudioDevice {
  public:
    ~AudioDeviceKoppiOS() override;

    bool setup(AudioCallbacks* callbacks, int sampleRate, int bufferSize) override;
    void pause(bool isPaused) override;
    void teardown() override;

    /*
     * Render into whatever room the kernel ring has and hand it over. The
     * main loop calls this once a frame -- see mainloop_koppios.cpp for why
     * the pull model has to become a push one here.
     */
    void pump();

    /** The rate snd_open() reported, or 0 if the device never opened. */
    int getSampleRate() const { return rate; }

  private:
    AudioCallbacks* cb = nullptr;
    int16_t* mixBuffer = nullptr;
    int mixFrames = 0;
    int rate = 0;
    bool paused = true;
    bool open = false;
};

#endif // __AUDIO_DEVICE_KOPPIOS_H__
