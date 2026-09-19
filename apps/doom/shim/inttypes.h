/* <inttypes.h> for the koppi-os Doom port. doomtype.h includes it only to get
 * the C99 fixed-width types, which are freestanding; the PRI* macros below are
 * the handful the engine formats with. */
#ifndef DOOM_SHIM_INTTYPES_H
#define DOOM_SHIM_INTTYPES_H

#include <stdint.h>

#define PRId32 "d"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"

#endif
