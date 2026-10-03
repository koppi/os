/*
 * corelib_font.h -- free-function facade over upstream's new FontManager
 * class (src/corelib/font_manager.h). Same arrangement as corelib_gfx.h.
 */
#ifndef __CORELIB_FONT_H__
#define __CORELIB_FONT_H__

#include "font_manager.h"

/*
 * extern "C++": several un-migrated headers (src/waveform_display.h,
 * src/app.h as upstream left it) include their corelib dependencies from
 * inside an `extern "C"` block. Without this, the same function would be
 * declared with C linkage in those translation units and C++ linkage in
 * bridge.cpp, and the link would fail on half of them.
 */
extern "C++" {


/** Install the process-wide FontManager. */
void fontBind(FontManager* fm);
/** The bound FontManager, for code that wants the object. */
FontManager& fontGet(void);

const Font* fontGetDefault(void);
void fontSetCurrent(const Font* font);
const Font* fontGetCurrent(void);
Font* fontLoad(const char* path);
void fontFree(Font* font);

} // extern "C++"

#endif // __CORELIB_FONT_H__
