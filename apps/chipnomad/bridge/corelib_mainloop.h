/*
 * corelib_mainloop.h -- free-function facade over upstream's new MainLoop
 * class (src/corelib/mainloop.h). Same arrangement as corelib_gfx.h.
 */
#ifndef __CORELIB_MAINLOOP_H__
#define __CORELIB_MAINLOOP_H__

#include "gfx.h"
#include "mainloop.h"

/*
 * extern "C++": several un-migrated headers (src/waveform_display.h,
 * src/app.h as upstream left it) include their corelib dependencies from
 * inside an `extern "C"` block. Without this, the same function would be
 * declared with C linkage in those translation units and C++ linkage in
 * bridge.cpp, and the link would fail on half of them.
 */
extern "C++" {


/** Install the platform's MainLoop. */
void mainLoopBind(MainLoop* ml);

/** Ask the loop to exit (posts a quit event). */
void mainLoopTriggerQuit(void);

} // extern "C++"

#endif // __CORELIB_MAINLOOP_H__
