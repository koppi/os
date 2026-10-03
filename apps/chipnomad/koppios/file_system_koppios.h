/*
 * FileSystem for this kernel's VFS.
 */
#ifndef __FILE_SYSTEM_KOPPIOS_H__
#define __FILE_SYSTEM_KOPPIOS_H__

#include "file_system.h"

class FileSystemKoppiOS : public FileSystem {
  public:
    bool getDefaultDirectory(char* buffer, int bufferSize) override;
    const char* getSettingsPath(void) override;
    const char* getAutosavePath(void) override;

    bool directoryExists(const char* path) override;
    bool createDirectory(const char* path) override;
    bool deleteFile(const char* path) override;

    FileEntry* listDirectory(const char* path, const char* extensions,
                             int* entryCount) override;
};

#endif // __FILE_SYSTEM_KOPPIOS_H__
