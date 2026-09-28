# koppios/ — this OS's own Qt6 build glue

Everything in `../src/` and `../mkspecs/` is real Qt 6.8.4 source (see
`../README.md` for exactly which files, why, and under what license).
Everything in *this* directory is **not** Qt source — it's the small
adaptation layer a real Qt build gets for free from CMake (generated
per-module config/export headers) or from `mkspecs/<platform>/`, hand-written
here because there is no CMake configure step against this target.

- `qconfig.h` — the handful of macros CMake normally generates from
  `.cmake.conf`'s `QT_REPO_MODULE_VERSION` (`QT_VERSION_MAJOR`/`MINOR`/
  `PATCH`/`STR`, `QT_EDITION`).
- `qtdeprecationdefinitions.h` — `QT_DEPRECATED_SINCE(major, minor)`, real
  working definition, also normally CMake-generated from the same version
  info.
- `qtcoreexports.h` — per-module export/visibility macros (`Q_CORE_EXPORT`
  etc., empty here — everything links statically into one flat ring-3
  binary, no `dllimport`/ELF-visibility concerns) plus three ABI-versioning
  macros (`QT_CORE_REMOVED_SINCE`, `QT_CORE_INLINE_SINCE`,
  `QT_CORE_INLINE_IMPL_SINCE`) that only matter for preserving a separate
  shared library's binary compatibility across releases — moot here, so
  each resolves to "use the current, inline behavior."
- `qplatformdefs.h` — this target's `mkspecs/<platform>/qplatformdefs.h`
  equivalent. Includes the real host POSIX headers plus Qt's own real,
  unmodified `mkspecs/common/posix/qplatformdefs.h` (maps `QT_OPEN`/
  `QT_READ`/`QT_WRITE`/... to `::open`/`::read`/`::write`/...).
- `QString`, `QtAlgorithms` — the two CaMeL-case forwarding headers this
  build's closure actually reaches (real Qt builds get ~1170 of these from
  `syncqt`; only these two are needed here). Each is the one real line
  `syncqt` would have generated: `#include "qstring.h"` /
  `#include "qalgorithms.h"`.

See `../README.md` for the two *real* Qt files this port patches directly
(`qsystemdetection.h`, `qconfig-bootstrapped.h`) — those stay in `../src/`
with the changes marked inline, not duplicated here.
