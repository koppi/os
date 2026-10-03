/*
 * MainLoop: 60 Hz frame pacing, raw-scancode input, audio pumping.
 */
#ifndef __MAINLOOP_KOPPIOS_H__
#define __MAINLOOP_KOPPIOS_H__

#include "gfx.h"
#include "mainloop.h"

class AudioDeviceKoppiOS;

class MainLoopKoppiOS : public MainLoop {
  public:
    MainLoopKoppiOS(Gfx& gfx, AudioDeviceKoppiOS& audio)
        : MainLoop(gfx), audio(audio) {}

    void run(App& app) override;
    void quit() override;
    void triggerQuit() override;

  private:
    AudioDeviceKoppiOS& audio;
    bool quitRequested = false;
};

#endif // __MAINLOOP_KOPPIOS_H__
