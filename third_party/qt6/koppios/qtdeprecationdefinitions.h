#ifndef QTDEPRECATIONDEFINITIONS_H
#define QTDEPRECATIONDEFINITIONS_H
/* Not Qt source -- see koppios/README.md. Normally CMake-generated from
 * QT_REPO_MODULE_VERSION: a QT_DEPRECATED_SINCE(major,minor) true/false
 * macro used to phase deprecated-API warnings in per release.
 * qtdeprecationmarkers.h (real Qt source) redefines this to a flat 0 when
 * QT_NO_DEPRECATED is set (which qconfig-bootstrapped.h's koppios addition
 * does) -- guard against the resulting harmless-but-noisy redefinition
 * warning. */
#ifndef QT_DEPRECATED_SINCE
#define QT_DEPRECATED_SINCE(major, minor) \
    ((QT_VERSION_MAJOR > (major)) || (QT_VERSION_MAJOR == (major) && QT_VERSION_MINOR >= (minor)))
#endif
#endif
