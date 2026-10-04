/* koppios addition, not upstream Qt: real Qt6's QStandardPaths ships a
 * platform backend per OS (qstandardpaths_unix.cpp, _win.cpp, _mac.cpp, ...)
 * providing standardLocations() -- there's no koppios one, and this kernel
 * has no real per-user config/cache/data directory convention to report
 * (see lib/qfileengine_koppios.cpp's own comment on this kernel's real,
 * narrower file model). Real callers (font/cache discovery, mostly) all
 * already handle an empty list as "nothing found here" gracefully -- same
 * honest-failure shape as this closure's other koppios stubs, just on the
 * Qt side instead of the libc side since this is a real Qt6 static member,
 * not a libc function. */
#include <QtCore/qstandardpaths.h>

QT_BEGIN_NAMESPACE

QStringList QStandardPaths::standardLocations(StandardLocation type) {
    Q_UNUSED(type);
    return QStringList();
}

QT_END_NAMESPACE
