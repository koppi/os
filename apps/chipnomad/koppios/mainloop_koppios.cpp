/*
 * The frame loop on koppi-os.
 *
 * Four things per frame, in this order:
 *
 *   1. drain the keyboard. The kernel's getscan (#26) is non-blocking and
 *      hands back one make/break event per call out of a 64-entry ring, so
 *      the ring is emptied every frame rather than left to overflow while a
 *      project loads. A grabbed screen puts the keyboard in raw mode for us
 *      (sys_gfx_open does both), which is what makes key *releases* visible
 *      -- ChipNomad needs them: its chords and its key repeat are built on
 *      press-and-hold.
 *   2. tick the app, which is where ChipNomad runs its key repeat, multi-tap
 *      timers and autosave.
 *   3. draw and present.
 *   4. pump audio, filling whatever room the PCM ring has.
 *
 * Audio last and unconditionally: it is the one step that must happen even on
 * a frame where nothing is drawn, because the ring drains in real time
 * whether or not the UI had work to do.
 *
 * Pacing is `clock` (#15, the PIT millisecond counter) plus `msleep` (#27),
 * the same pair apps/doom uses. Escape is the way out -- there is no window
 * to close -- and it is handled here rather than in the tracker so that it
 * works from any screen, including one that has swallowed the key.
 */
#include <stdint.h>

#include "mainloop_koppios.h"

#include "audio_device_koppios.h"
#include "input_utils_koppios.h"
#include "ksys.h"

#define FPS 60

/*
 * Hold-to-quit. A single Escape would be too easy to hit while editing --
 * nothing in the tracker uses it, but a tracker that exits on a stray
 * keystroke and loses the unsaved project is worse than one that asks for a
 * deliberate press. Escape held for about a third of a second quits; a tap
 * does nothing.
 */
#define QUIT_HOLD_FRAMES 20
#define SC_ESCAPE_MAKE 0x01

void MainLoopKoppiOS::run(App& app) {
    const uint32_t frameMs = 1000 / FPS;
    MainLoopEventData eventData;
    int escapeFrames = 0;

    quitRequested = false;

    while (!quitRequested) {
        uint32_t start = (uint32_t) ksys0(SYS_CLOCK);

        /* 1. keyboard */
        for (;;) {
            unsigned long ev = ksys0(SYS_GETSCAN);
            if (!(ev & KSCAN_VALID))
                break;

            int code = (int) (ev & 0x7F);
            bool release = (ev & KSCAN_BREAK) != 0;
            if (ev & KSCAN_E0)
                code |= KOPPIOS_KEY_E0;

            if (code == SC_ESCAPE_MAKE) {
                escapeFrames = release ? 0 : (escapeFrames ? escapeFrames : 1);
                continue;
            }

            InputCode input;
            input.deviceType = InputDeviceType::keyboard;
            input.code = code;

            eventData.type = release ? MainLoopEvent::keyUp : MainLoopEvent::keyDown;
            eventData.data.input = input;
            app.onEvent(eventData);
        }

        if (escapeFrames) {
            if (++escapeFrames > QUIT_HOLD_FRAMES)
                triggerQuit();
        }

        /* 2. tick */
        eventData.type = MainLoopEvent::tick;
        eventData.data.value = 0;
        app.onEvent(eventData);

        /* 3. draw */
        app.draw();
        gfx.updateScreen();

        /* 4. audio */
        audio.pump();

        uint32_t busy = (uint32_t) ksys0(SYS_CLOCK) - start;
        if (busy < frameMs)
            ksys1(SYS_MSLEEP, frameMs - busy);
    }

    /* The tracker autosaves the project and the settings from here. */
    eventData.type = MainLoopEvent::exit;
    eventData.data.value = 0;
    app.onEvent(eventData);
}

void MainLoopKoppiOS::quit() {
    /* Nothing to shut down that the Gfx and AudioDevice teardowns do not
     * already cover: the screen grab and the PCM claim are the only global
     * state this program takes. */
}

void MainLoopKoppiOS::triggerQuit() {
    quitRequested = true;
}
