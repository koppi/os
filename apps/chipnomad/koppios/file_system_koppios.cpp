/*
 * ChipNomad's FileSystem on koppi-os.
 *
 * Every method here replaces a POSIX one that this kernel either does not
 * have or answers differently, and the shape of the filesystem is worth
 * stating up front because it is what the file browser ends up showing:
 *
 *   - Paths are device-qualified. "/rd" is the boot RAM disk packed into
 *     os.iso, "/hda" the persistent FAT16 scratch disk, "/fda" a floppy,
 *     "/nfs" a mounted NFS export. A bare "/" names no device and lists the
 *     mount points.
 *   - Directories exist on the read side only. The FAT driver's operation
 *     vector (vfs.h) has touch and delete but no mkdir, and fat_listdir()
 *     walks a volume's root directory and nothing below it. So in practice
 *     each device is one flat directory, which is also why createDirectory()
 *     below reports failure rather than pretending.
 *   - Names are 8.3. The two files ChipNomad keeps for itself,
 *     "settings.txt" and "autosave.cnm", fit exactly; a project saved as
 *     "MYSONG.CNM" is fine, one saved with a longer name comes back
 *     truncated, which is the filesystem's answer and not something this
 *     layer can paper over.
 *
 * Reads and writes themselves are ordinary stdio -- see shim/stdio_koppios.c
 * for how fopen/fseek/fwrite sit on top of whole-file syscalls.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "file_system_koppios.h"
#include "ksys.h"

/* ------------------------------------------------------------------ *
 *  Where ChipNomad keeps its own files                                *
 * ------------------------------------------------------------------ */

/*
 * The persistent disk if there is one, the RAM disk otherwise.
 *
 * /hda survives a reboot and /rd does not, so a settings file and an autosave
 * are worth more on /hda; but os.iso boots perfectly well with no hard disk
 * attached (that is the usual `make qemu-iso` configuration), and then /rd is
 * all there is. Probed once -- the set of mounted devices does not change
 * while a program runs.
 */
static const char* baseDirectory(void) {
    static const char* cached;
    if (cached)
        return cached;

    static const char* const candidates[] = { "/hda", "/rd", "/nfs", NULL };
    char probe[512];
    for (int i = 0; candidates[i]; i++) {
        if (ksys3(SYS_LISTDIR, (unsigned long) candidates[i],
                  (unsigned long) probe, (unsigned long) sizeof probe) > 0) {
            cached = candidates[i];
            return cached;
        }
    }

    /* Nothing listable: still answer with something openable rather than an
     * empty path, so the error the caller reports names a file. */
    cached = "/rd";
    return cached;
}

bool FileSystemKoppiOS::getDefaultDirectory(char* buffer, int bufferSize) {
    if (!buffer || bufferSize <= 0)
        return false;
    snprintf(buffer, bufferSize, "%s", baseDirectory());
    return true;
}

/*
 * Both of these override the versions in platforms/shared/file_system.cpp,
 * which build their answer in a *local* array and return a pointer to it --
 * the `static` there is on the filename, not on the buffer, so the returned
 * pointer dangles the moment the function returns. (See
 * ../README.md, "Upstream bugs this port had to fix".) The buffers here are
 * static, which is what the signature promises.
 */
const char* FileSystemKoppiOS::getSettingsPath(void) {
    static char path[kPathMaxLength];
    snprintf(path, sizeof path, "%s%ssettings.txt",
             baseDirectory(), kPathSeparatorStr);
    return path;
}

const char* FileSystemKoppiOS::getAutosavePath(void) {
    static char path[kPathMaxLength];
    snprintf(path, sizeof path, "%s%sautosave.cnm",
             baseDirectory(), kPathSeparatorStr);
    return path;
}

/* ------------------------------------------------------------------ *
 *  Directories                                                        *
 * ------------------------------------------------------------------ */

bool FileSystemKoppiOS::directoryExists(const char* path) {
    if (!path || !path[0])
        return false;

    /* The root is not a device but the browser can sit in it. */
    if (strcmp(path, "/") == 0)
        return true;

    char probe[512];
    return ksys3(SYS_LISTDIR, (unsigned long) path,
                 (unsigned long) probe, (unsigned long) sizeof probe) > 0;
}

/*
 * Honest failure: there is no mkdir in this kernel, at any layer. The
 * tracker's "create folder" screen surfaces the false return as a message to
 * the user, which is the right outcome -- better than creating nothing and
 * claiming success, and better than hiding the screen, since the same binary
 * would want it the day the FAT driver grows directory writes.
 */
bool FileSystemKoppiOS::createDirectory(const char* path) {
    (void) path;
    return false;
}

bool FileSystemKoppiOS::deleteFile(const char* path) {
    return path && remove(path) == 0;
}

/* ------------------------------------------------------------------ *
 *  Listing                                                            *
 * ------------------------------------------------------------------ */

/** Does @p name end with one of the comma-separated suffixes in @p exts? */
static bool extensionMatches(const char* name, const char* exts) {
    if (!exts || !exts[0])
        return true;

    const char* dot = strrchr(name, '.');
    if (!dot)
        return false;

    size_t dotLen = strlen(dot);
    for (const char* pos = exts; pos; ) {
        if (strncasecmp(pos, dot, dotLen) == 0 &&
            (pos[dotLen] == '\0' || pos[dotLen] == ','))
            return true;
        pos = strchr(pos, ',');
        if (pos) pos++;
    }
    return false;
}

/*
 * Two kinds of listing, because this VFS has two kinds of directory.
 *
 * At "/" the entries are the mounted devices, which the kernel will not
 * enumerate for a caller (vfs_ls prints them to the console); they are probed
 * instead, which is cheap and gives the browser a root to start from.
 *
 * Anywhere else, listdir (#20) returns the volume's names newline-separated.
 * It reports no type bits, and since the FAT driver only ever lists a root
 * directory there are no subdirectories to report: every entry is a file.
 */
FileEntry* FileSystemKoppiOS::listDirectory(const char* path, const char* extensions,
                                            int* entryCount) {
    if (entryCount) *entryCount = 0;
    if (!path || !path[0])
        return nullptr;

    int capacity = 64;
    int count = 0;
    FileEntry* entries = (FileEntry*) malloc((size_t) capacity * sizeof(FileEntry));
    if (!entries)
        return nullptr;

    auto push = [&](const char* name, bool isDir) -> bool {
        if (count >= capacity) {
            int want = capacity * 2;
            FileEntry* grown = (FileEntry*) realloc(entries, (size_t) want * sizeof(FileEntry));
            if (!grown)
                return false;
            entries = grown;
            capacity = want;
        }
        snprintf(entries[count].name, sizeof entries[count].name, "%s", name);
        entries[count].isDirectory = isDir;
        count++;
        return true;
    };

    if (strcmp(path, "/") == 0) {
        static const char* const devices[] = { "rd", "hda", "hdb", "fda", "nfs", NULL };
        char probe[512];
        for (int i = 0; devices[i]; i++) {
            char devPath[16];
            snprintf(devPath, sizeof devPath, "/%s", devices[i]);
            if (ksys3(SYS_LISTDIR, (unsigned long) devPath,
                      (unsigned long) probe, (unsigned long) sizeof probe) > 0) {
                if (!push(devices[i], true)) { free(entries); return nullptr; }
            }
        }
        if (entryCount) *entryCount = count;
        return entries;
    }

    /* ".." first, so the browser can get back to the device list. */
    if (!push("..", true)) { free(entries); return nullptr; }

    /*
     * One allocation sized to the worst case the kernel will produce: a FAT16
     * root directory holds 512 entries, each at most "12345678.123\n".
     */
    const size_t listCap = 512 * 14 + 1;
    char* list = (char*) malloc(listCap);
    if (!list) {
        free(entries);
        return nullptr;
    }

    int n = (int) ksys3(SYS_LISTDIR, (unsigned long) path,
                        (unsigned long) list, (unsigned long) listCap);
    if (n <= 0) {
        free(list);
        if (entryCount) *entryCount = count;   /* just ".." */
        return entries;
    }

    char* p = list;
    while (*p) {
        char* nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        if (*p && extensionMatches(p, extensions)) {
            if (!push(p, false)) {
                free(list);
                free(entries);
                return nullptr;
            }
        }
        if (!nl)
            break;
        p = nl + 1;
    }

    free(list);
    if (entryCount) *entryCount = count;
    return entries;
}
