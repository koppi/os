/*
 * common.h -- the half of upstream's C++ migration that the cpp-migration
 * branch has not done yet.
 *
 * Upstream deleted src/common.{h,cpp} and split what was in them between the
 * new src/app_settings.{h,cpp} (an AppSettings *class*) and
 * src/tracker_state.{h,cpp} (a TrackerState class). The ~37 files under
 * src/screens/ and src/import/, plus src/app.cpp itself, were not converted
 * with them: they still reach for the free functions and globals the deleted
 * header declared, and for members (TrackerState::engine, AppSettings::
 * colorScheme) the new classes do not have. That is why the branch does not
 * compile for any of its own platforms, this one included -- see
 * ../README.md.
 *
 * Rather than rewrite 15k lines of working UI code to guess at a design
 * upstream has not published, this header restores the names that code
 * expects and defines them in terms of what the new classes actually offer.
 * Everything here is an adapter: no tracker logic lives in this file.
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <stdlib.h>

#include "chipnomad_lib.h"
#include "app_settings.h"
#include "tracker_state.h"
#include "input_utils.h"
#include "file_system.h"
#include "mainloop.h"

/*
 * extern "C++": several un-migrated headers (src/waveform_display.h,
 * src/app.h as upstream left it) include their corelib dependencies from
 * inside an `extern "C"` block. Without this, the same function would be
 * declared with C linkage in those translation units and C++ linkage in
 * bridge.cpp, and the link would fail on half of them.
 */
extern "C++" {


/*
 * Pre-migration spellings the un-migrated code still uses. The types
 * themselves are upstream's new ones; only the old names are added back.
 */
#define AUTOSAVE_FILENAME  "autosave.cnm"
#define FILENAME_LENGTH    kFileNameLength
#define THEME_NAME_LENGTH  kThemeNameLength

using ColorScheme = ColorTheme;
using ChipNomadQuality = chipnomad::EmulationQuality;

extern AppSettings appSettings;
extern TrackerState* chipnomadState;

/*
 * Cursor positions shared between the song/chain/phrase screens, and the
 * dirty flag. Upstream's TrackerState declares all four as members, but
 * screen_song.cpp assigns `pSongRow = &screen.cursorRow` and twenty-odd
 * sites write a bare `projectModified = 1`: unqualified, i.e. still the
 * globals the deleted common.h had. They stay globals until the screens are
 * converted, which leaves TrackerState's own four members unreferenced.
 */
extern int* pSongRow;
extern int* pSongTrack;
extern int* pChainRow;

extern int projectModified;

/* Settings / theme entry points, each one line over the AppSettings methods
 * the migration introduced. (The deleted common.h also declared
 * initDefaultAppSettings, initDefaultKeyMapping and
 * resetKeyMappingToDefaults; nothing in the tracker calls those, and the
 * AppSettings constructor and InputUtils::initDefaultKeyMapping now cover
 * what they did, so they are not reinstated.) */
int settingsSave(void);
int settingsLoad(void);
int saveTheme(const char* path);
int loadTheme(const char* path);
void resetToDefaultColors(void);

/* Paths and small utilities that used to be free functions and are now
 * FileSystem methods. */
void extractFilenameWithoutExtension(const char* path, char* output, int maxLength);
const char* getAutosavePath(void);
const char* getSettingsPath(void);
void clearNotePreview(void);

} // extern "C++"

#endif // __COMMON_H__
