#ifndef QPLATFORMDEFS_H
#define QPLATFORMDEFS_H
/* Not Qt source -- see koppios/README.md. This target's equivalent of one
 * of Qt's own mkspecs/<platform>/qplatformdefs.h files. Real host POSIX
 * headers are used here for type-checking, same as every other real-STL
 * app in apps/ (apps/hello-str and friends): compiled with g++ and real
 * system headers, not this kernel's own -ffreestanding path, but linked
 * against this kernel's own runtime/syscalls (see apps/hello-qt's
 * Makefile) -- most of what these headers declare is never actually
 * called by the demo (--gc-sections drops it), they just need to satisfy
 * real Qt source's own compile-time signatures. The QT_OPEN/QT_READ/...
 * macro mappings come from Qt's own real, unmodified
 * mkspecs/common/posix/qplatformdefs.h, reused rather than reinvented. */
#include "qglobal.h"
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include "../mkspecs/common/posix/qplatformdefs.h"
#endif
