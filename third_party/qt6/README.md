# Qt 6.8.4 (vendored subset) — `apps/hello-qt`'s dependency

This directory holds real, largely-unmodified Qt 6.8.4 source — not a
reimplementation, not a stand-in. It exists to build exactly one thing:
[`apps/hello-qt`](../../apps/hello-qt), the first real-Qt6 userspace app on
this OS.

## What's here and why

Real Qt6 needs CMake to configure and build — dozens of feature-detection
probes, generated per-module headers, a `syncqt` pass to lay out the public
include tree. None of that runs here. Instead:

- **`src/`** — the actual `.cpp`/`.h` files needed to build `QString` and
  its real dependency closure (locale-aware number formatting included),
  in Qt's own `QT_BOOTSTRAPPED` configuration (the same trimmed build Qt's
  own `moc`/`syncqt` tools use internally — no `QObject`, no threading).
  Copied verbatim from the upstream `qtbase` `6.8` branch, in their
  original `src/corelib/...` paths for provenance, **except** two files
  with a small, clearly-marked addition (see below). 24 `.cpp`
  translation units, ~163 headers they need, plus 3 more `.cpp` files
  `#include`d inline by `qstring.cpp` itself (`qchar.cpp`,
  `qunicodetables.cpp`, `qstringmatcher.cpp`) and one bundled third-party
  file (`src/3rdparty/siphash/siphash.cpp`, CC0-1.0, used by `qhash.cpp`).
- **`mkspecs/`** — two real files from Qt's own
  `mkspecs/common/{posix,c89}/qplatformdefs.h` (the `QT_OPEN`/`QT_READ`/
  `QT_WRITE`/... → `::open`/`::read`/`::write`/... macro mappings), reused
  rather than reinvented.
- **`koppios/`** — **not** Qt source. The small adaptation layer a real Qt
  build gets for free from CMake and `syncqt`, hand-written here because
  there is no CMake configure step against this target. See
  [`koppios/README.md`](koppios/README.md) for exactly what each file is
  and why. Includes `koppios/QtCore/` — a directory of relative symlinks
  into `../src/corelib/...`, mirroring what `syncqt` would generate, so
  real Qt source's own `#include <QtCore/qchar.h>`-style includes resolve.
- **`LICENSES/`** — Qt's own SPDX license texts for what's vendored here:
  `LGPL-3.0-only`, `GPL-2.0-only`, `GPL-3.0-only` (every vendored Qt file
  is triple-licensed `LicenseRef-Qt-Commercial OR LGPL-3.0-only OR
  GPL-2.0-only OR GPL-3.0-only` — this project exercises the LGPL-3.0-only
  option), `Unicode-3.0` (the Unicode Consortium's own data-table license,
  `qunicodetables_p.h`), and `CC0-1.0` (`siphash.cpp`).

## The two patched files

Only two real Qt files are modified, both with the change marked inline
(search for `koppios addition, not upstream Qt`):

- **`src/corelib/global/qsystemdetection.h`** — the first header any Qt
  file transitively includes hard `#error`s unless the compiler predefines
  a *recognized* OS macro. Added one `#elif defined(__KOPPIOS__)` branch
  (compiled with `-D__KOPPIOS__`) that falls through to the generic
  `Q_OS_UNIX` path, deliberately *not* `Q_OS_LINUX` (no epoll, no `/proc`,
  no real Linux syscalls beneath this).
- **`src/corelib/global/qconfig-bootstrapped.h`** — real Qt's own
  `QT_BOOTSTRAPPED` feature-flag config defines ~42 `QT_FEATURE_*` macros
  by hand; the rest of what `src/corelib/` references (~64 more, found by
  grepping the source rather than chasing one compile error at a time) are
  appended, all defaulted off (`-1`) except `version_tagging` (some
  headers `QT_REQUIRE_CONFIG` it unconditionally, and it's harmless — just
  embeds the Qt version string via a linker section).

## Why this footprint, not smaller

`QString` is not a leaf class. Number formatting (`QString::number()`,
used internally by more than it looks) needs real `QLocaleData`, which
needs real `QCalendar`/`QDateTime`/`QTimeZone` — the "just vendor
`qstring.cpp`" instinct doesn't survive contact with the actual link step.
What's here is the real, `ld`-verified minimal closure for this specific
app (see `apps/hello-qt/Makefile`'s `--gc-sections`, which drops what a
`QString`-only demo never calls — `QRegularExpression`, most of the
calendar backends — from what's compiled in but not part of the closure).

## Not vendored

Everything else in Qt6 — `QObject`, `QThread`, `QCoreApplication`, any GUI
module. `apps/hello-qt` deliberately doesn't need them. Getting a real
`QObject`/`QThread` app running here is a much larger undertaking
(`moc` code generation, an event loop, real `QCoreApplication`) that was
prototyped but not vendored — see the project's own memory notes for that
exploration's findings, not this directory.

## Upstream

`https://code.qt.io/qt/qtbase.git`, branch `6.8`, as of 2026-09-28. Qt's
own copyright headers and SPDX identifiers are preserved unchanged in
every vendored file.
