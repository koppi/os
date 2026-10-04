#!/usr/bin/env python3
"""koppios addition, not upstream Qt: make init_platform() in qguiapplication.cpp
build the platform integration through one koppios glue function instead of
QPlatformIntegrationFactory (dynamic plugin loading: no dlopen on this kernel).
qt_koppios_create_platform_integration() lives in qt_koppios_platform.cpp and
returns Qt's own, unmodified QMinimalIntegration."""
import sys
p = sys.argv[1] + "/src/gui/kernel/qguiapplication.cpp"
s = open(p).read()
already_gui = "qt_koppios_create_platform_integration" in s
old_call = "        QGuiApplicationPrivate::platform_integration = QPlatformIntegrationFactory::create(name, arguments, argc, argv, platformPluginPath);\n"
new_call = """#if defined(__KOPPIOS__)
        // koppios addition, not upstream Qt: no plugin loader here (see patch_gui.py)
        QGuiApplicationPrivate::platform_integration = qt_koppios_create_platform_integration();
#else
        QGuiApplicationPrivate::platform_integration = QPlatformIntegrationFactory::create(name, arguments, argc, argv, platformPluginPath);
#endif
"""
old_fn = "static void init_platform(const QString &pluginNamesWithArguments,"
new_fn = """#if defined(__KOPPIOS__)
QT_END_NAMESPACE
QT_PREPEND_NAMESPACE(QPlatformIntegration) *qt_koppios_create_platform_integration();
QT_BEGIN_NAMESPACE
#endif

static void init_platform(const QString &pluginNamesWithArguments,"""
if not already_gui:
    assert old_call in s and old_fn in s
    s = s.replace(old_call, new_call, 1).replace(old_fn, new_fn, 1)
    open(p, "w").write(s)
    print("patched qguiapplication.cpp")


# --- QThreadPoolPrivate::qtGuiInstance(): never create the GUI thread pool ---
# Qt parallelizes big image fills/conversions through it. A worker thread's
# stack is placed by the kernel at a fixed offset above the image, which
# collides with the main heap once the heap has grown over that area (e.g. a
# 518 KB QImage); thread teardown then unmaps live heap pages. Qt already has
# a runtime kill-switch for this (QT_NO_GUI_THREADPOOL); koppios has no
# environment, so make it unconditional. Callers fall back to the serial path.
p2 = sys.argv[1] + "/src/corelib/thread/qthreadpool.cpp"
t = open(p2).read()
if "koppios" not in t:
    old2 = '    const static bool runtime_disable = qEnvironmentVariableIsSet("QT_NO_GUI_THREADPOOL");'
    new2 = """#if defined(__KOPPIOS__)
    // koppios addition, not upstream Qt: serial image paths only (see patch_gui.py)
    const static bool runtime_disable = true;
#else
    const static bool runtime_disable = qEnvironmentVariableIsSet("QT_NO_GUI_THREADPOOL");
#endif"""
    assert old2 in t
    open(p2, "w").write(t.replace(old2, new2, 1))
    print("patched qthreadpool.cpp")
