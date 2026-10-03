/*
 * audio_init.h -- construction of the AudioManager singleton.
 *
 * src/app.cpp needs an AudioManager but has no way to reach the platform's
 * AudioDevice (upstream's unmigrated line tried to construct one from the
 * TrackerState alone). The platform backend registers its device here at
 * startup; app.cpp calls audioInit(), which constructs the manager and
 * binds the `audio` reference the screens use.
 */
#ifndef __AUDIO_INIT_H__
#define __AUDIO_INIT_H__

/*
 * extern "C++": several un-migrated headers (src/waveform_display.h,
 * src/app.h as upstream left it) include their corelib dependencies from
 * inside an `extern "C"` block. Without this, the same function would be
 * declared with C linkage in those translation units and C++ linkage in
 * bridge.cpp, and the link would fail on half of them.
 */
extern "C++" {


class AudioDevice;
class TrackerState;

/** Register the platform's AudioDevice. Called before appSetup(). */
void audioRegisterDevice(AudioDevice* device);

/** Construct the AudioManager over the registered device and bind `audio`. */
void audioInit(TrackerState* state);

/** Destroy it again (appCleanup() goes through audio.stop() first). */
void audioShutdown(void);

} // extern "C++"

#endif // __AUDIO_INIT_H__
