#ifndef __APP_H__
#define __APP_H__

/*
 * koppios: upstream wraps everything below in `extern "C" { ... }`, from
 * when the tracker was C. That gives appSetup() and friends C language
 * linkage -- src/app.cpp defines them under that declaration, and anything
 * including this header any other way disagrees -- and puts chipnomad_lib's
 * namespaces, classes and templates inside a C-linkage block. The
 * declarations themselves are upstream's, unchanged.
 */
#include "common.h"
#include "corelib_mainloop.h"
#include "corelib_input.h"

void appSetup(void);
void appCleanup(void);
void appDraw(void);
void appOnEvent(MainLoopEventData eventData);

// Raw input callback for key mapping screen
extern void (*inputRawCallback)(InputCode input, int isDown);

#endif
