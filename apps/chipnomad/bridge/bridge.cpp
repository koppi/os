/*
 * bridge.cpp -- the implementation side of the shim headers in this
 * directory. One forwarding call per entry point, plus the globals the
 * un-migrated tracker code declares `extern`. No tracker logic lives here;
 * see ../README.md for why this layer exists at all.
 */
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "corelib_gfx.h"
#include "corelib_font.h"
#include "corelib_file.h"
#include "corelib_input.h"
#include "corelib_mainloop.h"
#include "audio_init.h"
#include "audio_manager.h"
#include "tracker_state.h"

/* ------------------------------------------------------------------ *
 *  Globals the deleted src/common.h declared                          *
 * ------------------------------------------------------------------ */

AppSettings appSettings;
TrackerState* chipnomadState = nullptr;

int* pSongRow = nullptr;
int* pSongTrack = nullptr;
int* pChainRow = nullptr;

int projectModified = 0;

/* ------------------------------------------------------------------ *
 *  TrackerState's engine (declared by the patch in tracker_state.h)   *
 * ------------------------------------------------------------------ */

void TrackerState::initEngine(int sampleRate, chipnomad::ChipFactory factory) {
  delete engine;
  engine = new chipnomad::Engine(factory, sampleRate);
  engine->setProject(&project);
}

TrackerState::~TrackerState() {
  delete engine;
  engine = nullptr;
}

/* ------------------------------------------------------------------ *
 *  Gfx                                                                *
 * ------------------------------------------------------------------ */

static Gfx* theGfx = nullptr;

void gfxBind(Gfx* gfx) { theGfx = gfx; }
Gfx& gfxGet(void) { return *theGfx; }

int gfxSetup(int* w, int* h) { return theGfx->setup(w, h); }
void gfxCleanup(void) { theGfx->teardown(); }

void gfxSetFgColor(int rgb) { theGfx->setFgColor(rgb); }
void gfxSetCursorColor(int rgb) { theGfx->setCursorColor(rgb); }
void gfxSetBgColor(int rgb) { theGfx->setBgColor(rgb); }

void gfxClear(void) { theGfx->clear(); }
void gfxUpdateScreen(void) { theGfx->updateScreen(); }

void gfxClearRect(int x, int y, int w, int h) { theGfx->clearRect(x, y, w, h); }
void gfxCursor(int x, int y, int w) { theGfx->cursor(x, y, w); }
void gfxRect(int x, int y, int w, int h) { theGfx->rect(x, y, w, h); }
void gfxPrint(int x, int y, const char* text) { theGfx->print(x, y, text); }

void gfxPrintf(int x, int y, const char* format, ...) {
  va_list args;
  va_start(args, format);
  theGfx->printf(x, y, format, args);
  va_end(args);
}

void gfxPoint(int x, int y, uint32_t color) { theGfx->point(x, y, color); }

Bitmap* gfxBitmapCreate(int w, int h) { return theGfx->bitmapCreate(w, h); }
void gfxBitmapClear(Bitmap* b) { theGfx->bitmapClear(b); }
void gfxBitmapFree(Bitmap* b) { theGfx->bitmapFree(b); }
void gfxDrawBitmap(Bitmap* b, int col, int row) { theGfx->drawBitmap(b, col, row); }
void gfxDrawCharBitmap(uint8_t* b, int col, int row) { theGfx->drawCharBitmap(b, col, row); }

int gfxGetCharWidth(void) { return theGfx->getCharWidth(); }
int gfxGetCharHeight(void) { return theGfx->getCharHeight(); }
void gfxReloadFont(void) { theGfx->reloadFont(); }

void gfxDrawHUD(void) { theGfx->drawHUD(); }
void gfxSetButtonPressed(int i, int pressed) { theGfx->setButtonPressed(i, pressed); }

/* ------------------------------------------------------------------ *
 *  FontManager                                                        *
 * ------------------------------------------------------------------ */

static FontManager* theFonts = nullptr;

void fontBind(FontManager* fm) { theFonts = fm; }
FontManager& fontGet(void) { return *theFonts; }

const Font* fontGetDefault(void) { return theFonts->getDefault(); }
void fontSetCurrent(const Font* font) { theFonts->setCurrent(font); }
const Font* fontGetCurrent(void) { return theFonts->getCurrent(); }
Font* fontLoad(const char* path) { return theFonts->load(path); }
void fontFree(Font* font) { theFonts->freeFont(font); }

/* ------------------------------------------------------------------ *
 *  FileSystem                                                         *
 * ------------------------------------------------------------------ */

static FileSystem* theFs = nullptr;

void fileBind(FileSystem* fs) { theFs = fs; }
FileSystem& fileGet(void) { return *theFs; }

bool fileGetDefaultDirectory(char* buf, int size) { return theFs->getDefaultDirectory(buf, size); }
bool fileDirectoryExists(const char* path) { return theFs->directoryExists(path); }
bool fileCreateDirectory(const char* path) { return theFs->createDirectory(path); }
bool fileDeleteFile(const char* path) { return theFs->deleteFile(path); }

FileEntry* fileListDirectory(const char* path, const char* extensions, int* entryCount) {
  return theFs->listDirectory(path, extensions, entryCount);
}

/* ------------------------------------------------------------------ *
 *  InputUtils                                                         *
 * ------------------------------------------------------------------ */

static InputUtils* theInput = nullptr;

void inputBind(InputUtils* iu) { theInput = iu; }

void inputInitDefaultKeyMapping(void) { theInput->initDefaultKeyMapping(appSettings); }
const char* inputGetKeyName(InputCode input) { return theInput->getKeyName(input); }

/* ------------------------------------------------------------------ *
 *  MainLoop                                                           *
 * ------------------------------------------------------------------ */

static MainLoop* theMainLoop = nullptr;

void mainLoopBind(MainLoop* ml) { theMainLoop = ml; }
void mainLoopTriggerQuit(void) { theMainLoop->triggerQuit(); }

/* ------------------------------------------------------------------ *
 *  AudioManager singleton                                             *
 * ------------------------------------------------------------------ */

static AudioDevice* theAudioDevice = nullptr;
static AudioManager* theAudio = nullptr;

/*
 * `audio` is a reference, the way upstream's pre-migration header declared
 * it, so the screens' `audio.foo()` reads unchanged. The object cannot be
 * constructed at static-init time (it needs the TrackerState and the
 * platform's device), and a reference cannot be rebound later, so the
 * storage is reserved here and constructed in place by audioInit(). The
 * tracker only reaches `audio` from appSetup() onwards, after that call.
 */
alignas(AudioManager) static unsigned char audioStorage[sizeof(AudioManager)];
AudioManager& audio = *reinterpret_cast<AudioManager*>(audioStorage);
static bool audioLive = false;

void audioRegisterDevice(AudioDevice* device) { theAudioDevice = device; }

void audioInit(TrackerState* state) {
  if (audioLive) audioShutdown();
  theAudio = new (audioStorage) AudioManager(*theAudioDevice, state);
  audioLive = true;
}

void audioShutdown(void) {
  if (!audioLive) return;
  audioLive = false;
  theAudio->~AudioManager();
  theAudio = nullptr;
}

/* ------------------------------------------------------------------ *
 *  Settings, themes, paths                                            *
 * ------------------------------------------------------------------ */

int settingsSave(void) { return appSettings.saveSettings(getSettingsPath()) ? 1 : 0; }
int settingsLoad(void) { return appSettings.loadSettings(getSettingsPath()) ? 1 : 0; }

int saveTheme(const char* path) { return appSettings.saveColorTheme(path) ? 1 : 0; }
int loadTheme(const char* path) { return appSettings.loadColorTheme(path) ? 1 : 0; }

void resetToDefaultColors(void) { appSettings.resetColorTheme(); }

void extractFilenameWithoutExtension(const char* path, char* output, int maxLength) {
  theFs->extractFilenameWithoutExtension(path, output, maxLength);
}

const char* getAutosavePath(void) { return theFs->getAutosavePath(); }
const char* getSettingsPath(void) { return theFs->getSettingsPath(); }

/* Clears the note-preview column on the right of the screen, as the
 * free function of this name in the deleted src/common.cpp did. */
void clearNotePreview(void) {
  gfxClearRect(35, 3, 5, PROJECT_MAX_TRACKS);
}
