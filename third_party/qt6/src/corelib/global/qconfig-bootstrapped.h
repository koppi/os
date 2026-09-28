// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//
// Despite its file name, this really is not a public header.
// It is an implementation detail of the private bootstrap library.
//

#if 0
// silence syncqt warnings
#pragma qt_sync_skip_header_check
#pragma qt_sync_stop_processing
#endif

#ifdef QT_BOOTSTRAPPED

#include <stdlib.h> // for __GLIBC_PREREQ

#ifndef QT_NO_EXCEPTIONS
#define QT_NO_EXCEPTIONS
#endif

#define QT_NO_USING_NAMESPACE
#define QT_NO_DEPRECATED

// Keep feature-test macros in alphabetic order by feature name:
#define QT_FEATURE_alloca 1
#define QT_FEATURE_alloca_h -1
#ifdef _WIN32
# define QT_FEATURE_alloca_malloc_h 1
#else
# define QT_FEATURE_alloca_malloc_h -1
#endif
#define QT_FEATURE_cborstreamreader -1
#define QT_FEATURE_cborstreamwriter 1
#define QT_CRYPTOGRAPHICHASH_ONLY_SHA1
#define QT_FEATURE_cxx17_filesystem -1
#define QT_NO_DATASTREAM
#define QT_FEATURE_datestring 1
#define QT_FEATURE_datetimeparser -1
#define QT_FEATURE_dup3 -1
#define QT_FEATURE_easingcurve -1
#define QT_FEATURE_etw -1
#if defined(__linux__) || defined(__GLIBC__)
#define QT_FEATURE_getauxval (__has_include(<sys/auxv.h>) ? 1 : -1)
#else
#define QT_FEATURE_getauxval -1
#endif
#define QT_FEATURE_getentropy -1
#define QT_NO_GEOM_VARIANT
#define QT_FEATURE_hijricalendar -1
#define QT_FEATURE_icu -1
#define QT_FEATURE_islamiccivilcalendar -1
#define QT_FEATURE_jalalicalendar -1
#define QT_FEATURE_journald -1
#define QT_FEATURE_futimens -1
#undef QT_FEATURE_future
#define QT_FEATURE_future -1
#define QT_FEATURE_itemmodel -1
#define QT_FEATURE_library -1
#ifdef __linux__
# define QT_FEATURE_linkat 1
#else
# define QT_FEATURE_linkat -1
#endif
#define QT_FEATURE_lttng -1
#define QT_FEATURE_memmem -1
#define QT_FEATURE_memrchr -1
#define QT_NO_QOBJECT
#define QT_FEATURE_process -1
#define QT_FEATURE_regularexpression 1
#ifdef __GLIBC_PREREQ
# define QT_FEATURE_renameat2 (__GLIBC_PREREQ(2, 28) ? 1 : -1)
#else
# define QT_FEATURE_renameat2 -1
#endif
#define QT_FEATURE_shortcut -1
#define QT_FEATURE_slog2 -1
#define QT_FEATURE_syslog -1
#define QT_NO_SYSTEMLOCALE
#define QT_FEATURE_temporaryfile -1
#define QT_FEATURE_textdate 1
#undef QT_FEATURE_thread
#define QT_FEATURE_thread -1
#define QT_FEATURE_timezone -1
#define QT_FEATURE_topleveldomain -1
#define QT_NO_TRANSLATION
#define QT_FEATURE_translation -1
#define QT_NO_VARIANT -1

#define QT_NO_COMPRESS

// rcc.pro will DEFINES+= this
#ifndef QT_FEATURE_zstd
#define QT_FEATURE_zstd -1
#endif

#define QT_FEATURE_commandlineparser 1
#define QT_FEATURE_settings -1
#define QT_FEATURE_permissions -1

#define QT_NO_TEMPORARYFILE

/* --- koppios addition, not upstream Qt, starts here ---
 *
 * Mechanically generated (grep for QT_FEATURE_* across src/corelib/, diffed
 * against what qconfig-bootstrapped.h already defines) rather than chased
 * one compile error at a time -- every corelib feature that config doesn't
 * already cover, defaulted off (-1: no OS/library integration exists here
 * for any of dlopen/poll/select/openssl/pcre2/zlib/glib/inotify/posix or
 * sysv sem+shm/backtrace/filesystem watching/mimetype db -- and the
 * model/proxy-model and animation/xmlstream features are simply out of
 * scope for this port). */
#define QT_FEATURE_animation -1
#define QT_FEATURE_appstore_compliant -1
#define QT_FEATURE_backtrace -1
#define QT_FEATURE_clock_gettime -1
#define QT_FEATURE_concatenatetablesproxymodel -1
#define QT_FEATURE_cpp_winrt -1
#define QT_FEATURE_ctf -1
#define QT_FEATURE_cxx2b -1
#define QT_FEATURE_dlopen -1
#define QT_FEATURE_doubleconversion -1
#define QT_FEATURE_filesystemiterator -1
#define QT_FEATURE_filesystemwatcher -1
#define QT_FEATURE_framework -1
#define QT_FEATURE_fslibs -1
#define QT_FEATURE_fsnotify -1
#define QT_FEATURE_glib -1
#define QT_FEATURE_identityproxymodel -1
#define QT_FEATURE_inotify -1
#define QT_FEATURE_mimetype -1
#define QT_FEATURE_mimetype_database -1
#define QT_FEATURE_openssl_hash -1
#define QT_FEATURE_openssl_linked -1
#define QT_FEATURE_opensslv30 -1
#define QT_FEATURE_pcre2 -1
#define QT_FEATURE_poll_poll -1
#define QT_FEATURE_poll_pollts -1
#define QT_FEATURE_poll_ppoll -1
#define QT_FEATURE_poll_select -1
#define QT_FEATURE_posix_sem -1
#define QT_FEATURE_posix_shm -1
#define QT_FEATURE_processenvironment -1
#define QT_FEATURE_proxymodel -1
#define QT_FEATURE_reduce_relocations -1
#define QT_FEATURE_separate_debug_info -1
#define QT_FEATURE_shared -1
#define QT_FEATURE_sortfilterproxymodel -1
#define QT_FEATURE_std_atomic64 -1
#define QT_FEATURE_stringlistmodel -1
#define QT_FEATURE_system_doubleconversion -1
#define QT_FEATURE_system_libb2 -1
#define QT_FEATURE_system_pcre2 -1
#define QT_FEATURE_system_zlib -1
#define QT_FEATURE_systemsemaphore -1
#define QT_FEATURE_sysv_sem -1
#define QT_FEATURE_sysv_shm -1
#define QT_FEATURE_timezone_locale -1
#define QT_FEATURE_transposeproxymodel -1
/* version_tagging just embeds the Qt version string via a linker section;
 * harmless and some headers QT_REQUIRE_CONFIG it unconditionally, so on
 * rather than off. */
#define QT_FEATURE_version_tagging 1
#define QT_FEATURE_xmlstream -1

#define QT_FEATURE_broken_threadlocal_dtors -1
#define QT_FEATURE_cxx23_stacktrace -1
#define QT_FEATURE_debug -1
#define QT_FEATURE_debug_and_release -1
#define QT_FEATURE_dladdr -1
#define QT_FEATURE_feature -1
#define QT_FEATURE_forkfd_pidfd -1
#define QT_FEATURE_permission -1
#define QT_FEATURE_poll_exit_on_error -1
#define QT_FEATURE_relocatable -1
#define QT_FEATURE_sharedmemory -1
#define QT_FEATURE_signaling_nan -1
#define QT_FEATURE_vxpipedrv -1
#define QT_FEATURE_xmlstreamreader -1
#define QT_FEATURE_xmlstreamwriter -1

#endif // QT_BOOTSTRAPPED
