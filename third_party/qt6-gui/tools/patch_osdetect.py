#!/usr/bin/env python3
"""koppios addition, not upstream Qt: make qsystemdetection.h recognise
__KOPPIOS__ FIRST. The host g++ predefines __linux__ regardless of target, so
without this Q_OS_LINUX wins and Qt takes epoll//proc/glibc-TLS (%gs) paths
this kernel does not have. Same hunk as third_party/qt6's vendored copy."""
import sys
p = sys.argv[1] + "/src/corelib/global/qsystemdetection.h"
s = open(p).read()
if "Q_OS_KOPPIOS" in s:
    sys.exit(0)
old = "#if defined(__APPLE__) && (defined(__GNUC__) || defined(__xlC__) || defined(__xlc__))"
new = """#if defined(__KOPPIOS__)
   /* --- koppios addition, not upstream Qt: koppi's hobby OS, a freestanding
    * i386 kernel with its own syscall ABI. Falls through to the generic
    * Q_OS_UNIX branch (deliberately no Q_OS_LINUX). Checked FIRST because the
    * host g++ predefines __linux__ whatever the target is. --- */
#  define Q_OS_KOPPIOS
#elif defined(__APPLE__) && (defined(__GNUC__) || defined(__xlC__) || defined(__xlc__))"""
assert old in s
open(p, "w").write(s.replace(old, new, 1))
print("patched qsystemdetection.h")
