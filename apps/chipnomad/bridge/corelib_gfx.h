/*
 * corelib_gfx.h -- free-function facade over upstream's new Gfx class.
 *
 * The migration turned src/corelib/corelib_gfx.h's C API into the abstract
 * class in src/corelib/gfx.h, but the ~700 call sites under src/screens/
 * still call the free functions. Each one here forwards to the single Gfx
 * the platform backend installed with gfxBind(); the signatures are
 * upstream's own, unchanged.
 */
#ifndef __CORELIB_GFX_H__
#define __CORELIB_GFX_H__

#include <stdint.h>
#include "gfx.h"

/*
 * extern "C++": several un-migrated headers (src/waveform_display.h,
 * src/app.h as upstream left it) include their corelib dependencies from
 * inside an `extern "C"` block. Without this, the same function would be
 * declared with C linkage in those translation units and C++ linkage in
 * bridge.cpp, and the link would fail on half of them.
 */
extern "C++" {


/** Install the platform's Gfx. Called once, before any drawing. */
void gfxBind(Gfx* gfx);
/** The bound Gfx, for code that wants the object (the mainloop does). */
Gfx& gfxGet(void);

int gfxSetup(int* screenWidth, int* screenHeight);
void gfxCleanup(void);

void gfxSetFgColor(int rgb);
void gfxSetCursorColor(int rgb);
void gfxSetBgColor(int rgb);

void gfxClear(void);
void gfxUpdateScreen(void);

void gfxClearRect(int x, int y, int w, int h);
void gfxCursor(int x, int y, int w);
void gfxRect(int x, int y, int w, int h);
void gfxPrint(int x, int y, const char* text);
void gfxPrintf(int x, int y, const char* format, ...);
void gfxPoint(int x, int y, uint32_t color);

Bitmap* gfxBitmapCreate(int widthChars, int heightChars);
void gfxBitmapClear(Bitmap* bitmap);
void gfxBitmapFree(Bitmap* bitmap);
void gfxDrawBitmap(Bitmap* bitmap, int col, int row);
void gfxDrawCharBitmap(uint8_t* bitmap, int col, int row);

int gfxGetCharWidth(void);
int gfxGetCharHeight(void);
void gfxReloadFont(void);

void gfxDrawHUD(void);
void gfxSetButtonPressed(int buttonIndex, int pressed);

} // extern "C++"

#endif // __CORELIB_GFX_H__
