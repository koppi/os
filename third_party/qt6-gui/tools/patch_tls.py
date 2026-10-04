#!/usr/bin/env python3
"""koppios addition, not upstream Qt: turn the 7 real `thread_local`
declarations in the corelib closure into plain `static`s. This kernel sets up
no %gs TLS segment (GCC 15 has no -femulated-tls either), so a native
thread_local access page-faults at address ~0. Single-threaded closure only;
revisit if a second thread ever touches property bindings / logging."""
import sys, re
qt = sys.argv[1]
NOTE = "/* koppios addition, not upstream Qt: single-threaded closure, no %gs TLS (see .qt6-gui-bootstrap/patch_tls.py) */ "
patches = [
 ("src/corelib/time/qtimezoneprivate_tz.cpp", "Q_CONSTINIT thread_local static ZoneNameReader reader;", "static ZoneNameReader reader;"),
 ("src/corelib/thread/qthread_unix.cpp", "Q_CONSTINIT static thread_local QThreadData *currentThreadData = nullptr;", "Q_CONSTINIT static QThreadData *currentThreadData = nullptr;"),
 ("src/corelib/text/qregularexpression.cpp", "Q_CONSTINIT static thread_local std::unique_ptr<pcre2_jit_stack_16, PcreJitStackFree> jitStacks;", "static std::unique_ptr<pcre2_jit_stack_16, PcreJitStackFree> jitStacks;"),
 ("src/corelib/kernel/qproperty.cpp", "Q_CONSTINIT static thread_local QBindingStatus bindingStatus;", "Q_CONSTINIT static QBindingStatus bindingStatus;"),
 ("src/corelib/kernel/qobject.cpp", "Q_CONSTINIT static thread_local FlaggedDebugSignatures flaggedSignatures = {};", "Q_CONSTINIT static FlaggedDebugSignatures flaggedSignatures = {};"),
 ("src/corelib/io/qloggingregistry.cpp", "Q_CONSTINIT thread_local bool recursionGuard = false;", "static bool recursionGuard = false;"),
 ("src/corelib/global/qlogging.cpp", "Q_CONSTINIT static thread_local bool msgHandlerGrabbed = false;", "Q_CONSTINIT static bool msgHandlerGrabbed = false;"),
]
for rel, old, new in patches:
    p = f"{qt}/{rel}"
    s = open(p).read()
    if NOTE in s and new in s:
        continue
    if old not in s:
        print(f"patch_tls: pattern not found in {rel}: {old}"); sys.exit(1)
    open(p, "w").write(s.replace(old, NOTE + new, 1))
    print("patched", rel)
