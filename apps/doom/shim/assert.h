/* <assert.h> for the koppi-os Doom port: a failed assertion prints and quits
 * (there is no abort-to-debugger here, so exiting is the honest behaviour). */
#ifndef DOOM_SHIM_ASSERT_H
#define DOOM_SHIM_ASSERT_H

#ifdef NDEBUG
#define assert(x) ((void) 0)
#else
#include <stdio.h>
#include <stdlib.h>
#define assert(x) \
    ((x) ? (void) 0 \
         : (printf("assertion failed: %s (%s:%d)\n", #x, __FILE__, __LINE__), \
            exit(1)))
#endif

#endif
