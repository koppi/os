#ifndef QTCOREEXPORTS_H
#define QTCOREEXPORTS_H
/* Not Qt source -- see koppios/README.md. Normally CMake-generated
 * per-module export/visibility macros (dllimport/dllexport or ELF
 * visibility for a shared libQtCore.so). Everything here links statically
 * into one flat ring-3 binary, so these are just empty. */
#define Q_CORE_EXPORT
#define Q_CORE_EXPORT_INLINE inline
#define Q_AUTOTEST_EXPORT

/* QT_<MODULE>_REMOVED_SINCE gates old-ABI compat shims (real Qt's
 * src/corelib/compat/removed_api.cpp and friends) that keep a
 * previously-removed function around for binary compatibility with code
 * built against an older Qt. Nothing here links against a separate
 * libQtCore, so there is no ABI to stay compatible with -- always false,
 * never compile in the old shim. */
#define QT_CORE_REMOVED_SINCE(major, minor) 0

/* QT_CORE_INLINE_SINCE(x,y) marks a declaration `inline` as of version x.y
 * (vs. out-of-line, kept for pre-x.y binary compatibility);
 * QT_CORE_INLINE_IMPL_SINCE(x,y) gates whether that inline body is compiled
 * into this translation unit at all. We are always "at" 6.8.4 with no
 * separate-library ABI to preserve, so both are unconditionally the
 * current-version answer: mark inline, and always compile the body. */
#define QT_CORE_INLINE_SINCE(major, minor) inline
#define QT_CORE_INLINE_IMPL_SINCE(major, minor) 1
#endif
