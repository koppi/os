#include "tracker_app.h"

#include "app.h"
#include "assets.h"
#include "audio_init.h"
#include "corelib_file.h"
#include "corelib_font.h"
#include "corelib_gfx.h"
#include "corelib_input.h"
#include "file_system.h"
#include "font_manager.h"
#include "gfx.h"
#include "input_utils.h"
#include "screens.h"

TrackerApp::TrackerApp(Gfx& gfx, FontManager& fontManager, AudioDevice& audioDevice,
                       FileSystem& fileSystem, Assets& assets, InputUtils& inputUtils) {
  /* The shim headers forward to whatever is bound here, so this has to
   * happen before anything draws, reads a file or asks for a key name. */
  gfxBind(&gfx);
  fontBind(&fontManager);
  fileBind(&fileSystem);
  inputBind(&inputUtils);
  audioRegisterDevice(&audioDevice);

  assets.copyAssets();
}

bool TrackerApp::setup() {
  /* Settings first: appSetup() reads the colour theme and the audio rate out
   * of them, and gfxSetup() picks a font resolution from the screen size
   * they carry. A missing settings file is not an error -- AppSettings's
   * constructor has already filled in the defaults. */
  settingsLoad();

  /* A custom font has to be current before gfxSetup(), which picks the
   * resolution to build its glyph cache from. Same order as upstream's
   * pre-migration platforms/shared/main.cpp. */
  if (appSettings.fontPath[0] != '\0') {
    Font* font = fontLoad(appSettings.fontPath);
    if (font) {
      fontSetCurrent(font);
    } else {
      appSettings.fontPath[0] = '\0';
      fontSetCurrent(nullptr);
    }
  }

  if (!gfxSetup(&appSettings.screenWidth, &appSettings.screenHeight)) return false;

  appSetup();
  return true;
}

void TrackerApp::teardown() {
  appCleanup();
  audioShutdown();
  gfxCleanup();
}

void TrackerApp::draw() {
  appDraw();
}

void TrackerApp::onEvent(MainLoopEventData eventData) {
  appOnEvent(eventData);
}

void TrackerApp::onRawInput(InputCode input, int isDown) {
  if (inputRawCallback) inputRawCallback(input, isDown);
}
