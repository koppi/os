/*
 * corelib_file.h -- free-function facade over upstream's new FileSystem
 * class (src/corelib/file_system.h). Same arrangement as corelib_gfx.h.
 */
#ifndef __CORELIB_FILE_H__
#define __CORELIB_FILE_H__

#include "file_system.h"

/*
 * extern "C++": several un-migrated headers (src/waveform_display.h,
 * src/app.h as upstream left it) include their corelib dependencies from
 * inside an `extern "C"` block. Without this, the same function would be
 * declared with C linkage in those translation units and C++ linkage in
 * bridge.cpp, and the link would fail on half of them.
 */
extern "C++" {


/*
 * Path-shape macros the un-migrated code still uses; the migration replaced
 * them with the constexprs in src/corelib/file_system.h, which these name.
 */
#define PATH_SEPARATOR     kPathSeparator
#define PATH_SEPARATOR_STR kPathSeparatorStr
#define PATH_LENGTH        kPathMaxLength

/** Install the platform's FileSystem. */
void fileBind(FileSystem* fs);
/** The bound FileSystem. */
FileSystem& fileGet(void);

bool fileGetDefaultDirectory(char* buffer, int bufferSize);
bool fileDirectoryExists(const char* path);
bool fileCreateDirectory(const char* path);
bool fileDeleteFile(const char* path);
FileEntry* fileListDirectory(const char* path, const char* extensions, int* entryCount);

} // extern "C++"

#endif // __CORELIB_FILE_H__
