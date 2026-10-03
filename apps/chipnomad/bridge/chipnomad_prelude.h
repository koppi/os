/*
 * chipnomad_prelude.h -- force-included (-include) into every translation
 * unit of this port.
 *
 * Two order-dependency problems in the cpp-migration branch that a prelude
 * fixes once instead of in thirty-odd places:
 *
 *  - Headers that use types they do not include. chips/chip_ay.h names
 *    ChipSetup and StereoModeAY (project.h), src/project_utils.h names
 *    Project, src/import/import_common.h names TableRow and PhraseRow.
 *    They compile only in translation units that happened to include
 *    project.h first.
 *  - Unqualified uses of names the migration moved into `namespace
 *    chipnomad`. src/screens/screens.h already pulls the namespace in at
 *    global scope for exactly this reason, with a comment calling it a
 *    bridge "until the tracker is migrated to explicit qualification"; the
 *    headers above are simply the ones that are included before it.
 *
 * So: the library's public surface first, then upstream's own
 * using-directive, hoisted to where every file sees it.
 *
 * Not force-included into chipnomad_lib/external/ayumi/: that is Peter
 * Sovietov's emulator, vendored by upstream, and it needs no bridging --
 * ayumi_filters.cpp redeclares FIR_SIZE/DECIMATE_FACTOR locally, which
 * conflicts with ayumi.h once something else has pulled that in.
 */
#ifndef __CHIPNOMAD_PRELUDE_H__
#define __CHIPNOMAD_PRELUDE_H__

#include "chipnomad_constants.h"
#include "project.h"
#include "project_instruments.h"
#include "chips/chips.h"
#include "chips/chip_ay.h"   /* src/screens/project_ay.cpp names SoundChipAY */
#include "playback.h"
#include "utils.h"
#include "chipnomad_lib.h"

/* Same story on the platform-abstraction side: src/corelib/mainloop.h takes
 * a `Gfx&` and puts an InputCode in a union without including either
 * header. */
#include "gfx.h"
#include "input_utils.h"
#include "file_system.h"
#include "font_manager.h"
#include "mainloop.h"

using namespace chipnomad;

#endif // __CHIPNOMAD_PRELUDE_H__
