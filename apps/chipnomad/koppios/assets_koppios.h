/*
 * Assets: nothing to copy.
 *
 * The mobile builds unpack their bundled themes, instruments and demo songs
 * into writable storage on first run, because an .apk is not a filesystem.
 * Here they are already files -- staged on the boot RAM disk by hda.sh, which
 * is this system's only packaging step -- so there is nothing to unpack.
 */
#ifndef __ASSETS_KOPPIOS_H__
#define __ASSETS_KOPPIOS_H__

#include "assets.h"

class AssetsKoppiOS : public Assets {
  public:
    bool copyAssets() override { return true; }
};

#endif // __ASSETS_KOPPIOS_H__
