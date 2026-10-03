/*
 * ChipNomad on koppi-os: process entry point.
 *
 * Replaces platforms/shared/main.cpp, which hardcodes the SDL1.2/SDL2 choice
 * (and, on the cpp-migration branch, instantiates a `TrackerApp` that branch
 * never defines -- bridge/tracker_app.cpp is that class). The structure is
 * upstream's: build the platform objects, hand them to the app, run the loop,
 * tear down in reverse.
 */
#include <stdio.h>

#include "assets_koppios.h"
#include "audio_device_koppios.h"
#include "corelib_mainloop.h"
#include "file_system_koppios.h"
#include "font_manager.h"
#include "gfx_koppios.h"
#include "input_utils_koppios.h"
#include "mainloop_koppios.h"
#include "tracker_app.h"

int main(void) {
    FontManager fontManager;
    FileSystemKoppiOS fileSystem;
    AudioDeviceKoppiOS audioDevice;
    GfxKoppiOS gfx(fontManager);
    InputUtilsKoppiOS inputUtils;
    MainLoopKoppiOS mainLoop(gfx, audioDevice);
    AssetsKoppiOS assets;

    /* The un-migrated screens reach the loop through mainLoopTriggerQuit();
     * bind it before anything can call it. */
    mainLoopBind(&mainLoop);

    TrackerApp app(gfx, fontManager, audioDevice, fileSystem, assets, inputUtils);

    if (!app.setup()) {
        /* setup() has already said why on the console it still owns -- the
         * screen grab is taken inside it and released by teardown(). */
        app.teardown();
        return 1;
    }

    mainLoop.run(app);
    mainLoop.quit();
    app.teardown();

    printf("chipnomad: bye\n");
    return 0;
}
