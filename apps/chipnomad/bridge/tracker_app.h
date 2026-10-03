/*
 * tracker_app.h -- the App subclass upstream's platforms/shared/main.cpp
 * instantiates as `TrackerApp` but never defines anywhere on the
 * cpp-migration branch.
 *
 * src/app.cpp exports the pre-migration free functions (appSetup, appDraw,
 * appOnEvent, appCleanup) rather than a class; this adapts them to the
 * src/corelib/mainloop.h App interface the new MainLoop drives, and takes
 * care of binding the platform objects the shim headers in this directory
 * forward to.
 */
#ifndef __TRACKER_APP_H__
#define __TRACKER_APP_H__

#include "mainloop.h"

/*
 * extern "C++": several un-migrated headers (src/waveform_display.h,
 * src/app.h as upstream left it) include their corelib dependencies from
 * inside an `extern "C"` block. Without this, the same function would be
 * declared with C linkage in those translation units and C++ linkage in
 * bridge.cpp, and the link would fail on half of them.
 */
extern "C++" {


class Gfx;
class FontManager;
class AudioDevice;
class FileSystem;
class Assets;
class InputUtils;

class TrackerApp : public App {
  public:
    TrackerApp(Gfx& gfx, FontManager& fontManager, AudioDevice& audioDevice,
               FileSystem& fileSystem, Assets& assets, InputUtils& inputUtils);

    bool setup() override;
    void teardown() override;
    void draw() override;
    void onEvent(MainLoopEventData eventData) override;
    void onRawInput(InputCode input, int isDown) override;
};

} // extern "C++"

#endif // __TRACKER_APP_H__
