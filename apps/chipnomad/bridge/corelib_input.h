/*
 * corelib_input.h -- free-function facade over upstream's new InputUtils
 * class (src/corelib/input_utils.h), plus the Key/InputCode enums the
 * screens use. Same arrangement as corelib_gfx.h.
 */
#ifndef __CORELIB_INPUT_H__
#define __CORELIB_INPUT_H__

#include "input_utils.h"

/*
 * extern "C++": several un-migrated headers (src/waveform_display.h,
 * src/app.h as upstream left it) include their corelib dependencies from
 * inside an `extern "C"` block. Without this, the same function would be
 * declared with C linkage in those translation units and C++ linkage in
 * bridge.cpp, and the link would fail on half of them.
 */
extern "C++" {


/** Install the platform's InputUtils. */
void inputBind(InputUtils* iu);

void inputInitDefaultKeyMapping(void);
const char* inputGetKeyName(InputCode input);

} // extern "C++"

#endif // __CORELIB_INPUT_H__
