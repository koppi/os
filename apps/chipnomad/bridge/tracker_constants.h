/*
 * tracker_constants.h -- referenced by the vendored tracker's app_settings.h
 * but absent from upstream's cpp-migration branch (see ../README.md).
 *
 * The two lengths are upstream's own, carried over verbatim from the
 * FILENAME_LENGTH / THEME_NAME_LENGTH macros that main's src/common.h
 * defined before the migration split that header up; the on-disk settings
 * format depends on them, so they are not free to change.
 */
#ifndef __TRACKER_CONSTANTS_H__
#define __TRACKER_CONSTANTS_H__

constexpr int kFileNameLength = 24;
constexpr int kThemeNameLength = 16;

#endif // __TRACKER_CONSTANTS_H__
