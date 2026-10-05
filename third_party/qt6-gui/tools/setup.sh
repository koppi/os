#!/bin/bash
# Rebuilds the entire scratch QtGui compile environment from scratch into
# $SCRATCH/{qtbase,qtgen}. Idempotent -- safe to rerun. /tmp gets wiped
# between sessions, so nothing here can assume anything in $SCRATCH already
# exists; only this directory (and the vendored tree it feeds) is
# durable. See qt6-real-source-compile.md memory for the full narrative of
# why each step below is needed.
set -e
SCRATCH="${1:?usage: setup.sh <scratchpad-dir>}"
TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; Q6G="$(cd "$TOOLS/.." && pwd)"; REPO="$(cd "$Q6G/../.." && pwd)"
BOOT="$TOOLS"
QTBASE="$SCRATCH/qtbase"
QTGEN="$SCRATCH/qtgen/include"

echo "=== 1/6: clone qtbase 6.8 (skips if already present) ==="
if [ ! -d "$QTBASE/.git" ]; then
    git clone --branch 6.8 --single-branch --depth 1 https://code.qt.io/qt/qtbase.git "$QTBASE"
fi

echo "=== 2/6: flattened symlink trees (QtCore/, QtGui/, qpa/, */private/) ==="
mkdir -p "$QTGEN/QtCore/private" "$QTGEN/QtGui/private" "$QTGEN/QtWidgets/private" "$QTGEN/qpa/private" "$QTGEN/ft_koppios_override"

relsym() {  # relsym <target-file> <dest-dir>
    local base; base=$(basename "$1")
    local rel; rel=$(python3 -c "import os,sys; print(os.path.relpath(sys.argv[1], sys.argv[2]))" "$1" "$2")
    ln -sf "$rel" "$2/$base"
}

find "$QTBASE/src/corelib" -iname "*.h" | while read -r f; do relsym "$f" "$QTGEN/QtCore"; done
find "$QTBASE/src/gui" -iname "*.h" | grep -v "/3rdparty/" | while read -r f; do relsym "$f" "$QTGEN/QtGui"; done
find "$QTBASE/src/gui" \( -iname "qplatform*.h" -o -iname "qwindowsysteminterface*.h" \) | grep -v "/3rdparty/" | while read -r f; do
    base=$(basename "$f")
    case "$base" in *_p.h) relsym "$f" "$QTGEN/qpa/private";; *) relsym "$f" "$QTGEN/qpa";; esac
done
find "$QTBASE/src/widgets" -iname "*.h" | grep -v "/doc/" | while read -r f; do relsym "$f" "$QTGEN/QtWidgets"; done
find "$QTBASE/src/widgets" -iname "*_p.h" | grep -v "/doc/" | while read -r f; do relsym "$f" "$QTGEN/QtWidgets/private"; done
find "$QTBASE/src/corelib" -iname "*_p.h" | while read -r f; do relsym "$f" "$QTGEN/QtCore/private"; done
find "$QTBASE/src/gui" -iname "*_p.h" | grep -v "/3rdparty/" | while read -r f; do relsym "$f" "$QTGEN/QtGui/private"; done

# Real Qt source includes qpa private headers BOTH as a bare relative
# "foo_p.h" (resolved via qpa/private/ above) AND as a qualified <qpa/foo_p.h>
# (needs the same file reachable directly under qpa/ itself, one dir up) --
# mirror every qpa/private/*.h up into qpa/ too.
for f in "$QTGEN"/qpa/private/*.h; do
    [ -e "$f" ] || continue
    ln -sf "private/$(basename "$f")" "$QTGEN/qpa/$(basename "$f")"
done

echo "=== 3/6: config headers (qconfig.h, qtcore-config.h, qtgui-config.h, *_p.h, exports) ==="
cp /usr/include/x86_64-linux-gnu/qt6/QtCore/qconfig.h "$QTGEN/QtCore/qconfig.h"
sed -i 's/"6\.10\.2"/"6.8.4"/; s/QT_VERSION_MINOR 10/QT_VERSION_MINOR 8/; s/QT_VERSION_PATCH 2/QT_VERSION_PATCH 4/' "$QTGEN/QtCore/qconfig.h"

Q="${QT_SDK_INCLUDE:-$HOME/.cache/qgcoder-wasm/Qt/6.10.2/gcc_64/include}"   # any installed Qt 6.10 SDK: source of the config headers
if [ -d "$Q" ]; then
    cp "$Q/QtCore/6.10.2/QtCore/private/qconfig_p.h" "$QTGEN/QtCore/private/" 2>/dev/null || true
    cp "$Q/QtCore/6.10.2/QtCore/private/qtcore-config_p.h" "$QTGEN/QtCore/private/" 2>/dev/null || true
    # that reference SDK is a real desktop Linux build with real backtrace()
    # support; koppios has none (no execinfo.h equivalent) -- without this,
    # qlogging_p.h's `__has_include(<cxxabi.h>) && QT_CONFIG(backtrace)` sees
    # a real host <cxxabi.h> and turns on execinfo-backtrace code this target
    # can't provide (BACKTRACE_HEADER left as a bare, unexpanded macro name).
    sed -i 's/#define QT_FEATURE_backtrace 1/#define QT_FEATURE_backtrace -1/' "$QTGEN/QtCore/private/qtcore-config_p.h" 2>/dev/null || true
    cp "$Q/QtGui/6.10.2/QtGui/private/qtgui-config_p.h" "$QTGEN/QtGui/private/" 2>/dev/null || true
    cp "$Q/QtCore/qtcoreexports.h" "$QTGEN/QtCore/" 2>/dev/null || true
    cp "$Q/QtGui/qtguiexports.h" "$QTGEN/QtGui/" 2>/dev/null || true
    for f in "$Q"/QtCore/Qt*; do
        [ -f "$f" ] || continue
        base=$(basename "$f")
        [ -e "$QTGEN/QtCore/$base" ] || cp "$f" "$QTGEN/QtCore/$base"
    done
    for f in "$Q"/QtGui/Qt*; do
        [ -f "$f" ] || continue
        base=$(basename "$f")
        [ -e "$QTGEN/QtGui/$base" ] || cp "$f" "$QTGEN/QtGui/$base"
    done
else
    echo "WARNING: reference SDK at $Q not found -- qtcore-config_p.h/qtgui-config_p.h/exports/Qt* aggregate headers must be sourced another way"
fi

# ---- QtWidgets config: the SDK's desktop build, trimmed to what this port can back ----
if [ -d "$Q" ]; then
    cp "$Q/QtWidgets/qtwidgets-config.h" "$QTGEN/QtWidgets/qtwidgets-config.h"
    cp "$Q/QtWidgets/6.10.2/QtWidgets/private/qtwidgets-config_p.h" "$QTGEN/QtWidgets/private/qtwidgets-config_p.h"
    cp "$Q/QtWidgets/qtwidgetsexports.h" "$QTGEN/QtWidgets/"
    for f in "$Q"/QtWidgets/Qt*; do
        [ -f "$f" ] || continue
        base=$(basename "$f")
        [ -e "$QTGEN/QtWidgets/$base" ] || cp "$f" "$QTGEN/QtWidgets/$base"
    done
    # style_stylesheet and style_windows stay ON: QWidget (6.8) includes QStyleSheetStyle
    # unconditionally and that derives from QWindowsStyle.
    # menu stays ON: QCommonStyle's menu-item sizing/drawing is #if QT_CONFIG(menu), and a
    # combo box popup is a menu-item list (with it off every popup item is as big as the view).
    # Off: menu *bars*/toolbars/main-window machinery, all dialogs, graphics view, item views beyond
    # list view (combo box), rich-text editors, tooltips, style sheets, and the other native
    # styles. On: every simple control (buttons, sliders, tabs, spin boxes, line edit, label...).
    for f in datetimeedit textbrowser splashscreen fontcombobox toolbar toolbox \
             mainwindow dockwidget mdiarea resizehandler statusbar menubar contextmenu scroller \
             graphicsview graphicseffect textedit syntaxhighlighter rubberband tooltip statustip \
             sizegrip calendarwidget keysequenceedit dialog dialogbuttonbox messagebox colordialog \
             filedialog fontdialog progressdialog inputdialog errormessage wizard tableview \
             tablewidget treeview treewidget columnview datawidgetmapper completer fscompleter \
             undoview commandlinkbutton; do
        sed -i "s/^#define QT_FEATURE_$f 1\$/#define QT_FEATURE_$f -1/" "$QTGEN/QtWidgets/qtwidgets-config.h"
    done
    for f in gtk3 effects; do
        sed -i "s/^#define QT_FEATURE_$f 1\$/#define QT_FEATURE_$f -1/" "$QTGEN/QtWidgets/private/qtwidgets-config_p.h"
    done
fi

cat > "$QTGEN/QtCore/qtcore-config.h" << 'COREEOF'
#ifndef QT_FEATURES_Core_H
#define QT_FEATURES_Core_H
#define QT_FEATURE_clock_monotonic 1
#define QT_FEATURE_cxx11_future -1
#define QT_FEATURE_cxx17_filesystem -1
#define QT_FEATURE_glib -1
#define QT_FEATURE_inotify -1
#define QT_FEATURE_jemalloc -1
#define QT_FEATURE_std_atomic64 1
#define QT_FEATURE_mimetype -1
#define QT_FEATURE_regularexpression 1
#define QT_FEATURE_sharedmemory -1
#define QT_FEATURE_shortcut 1
#define QT_FEATURE_systemsemaphore -1
#define QT_FEATURE_xmlstream -1
#define QT_FEATURE_cpp_winrt -1
#define QT_FEATURE_xmlstreamreader -1
#define QT_FEATURE_xmlstreamwriter -1
#define QT_FEATURE_textdate 1
#define QT_FEATURE_datestring 1
#define QT_FEATURE_process -1
#define QT_FEATURE_processenvironment -1
#define QT_FEATURE_temporaryfile -1
#define QT_FEATURE_library -1
#define QT_FEATURE_settings -1
#define QT_FEATURE_filesystemwatcher -1
#define QT_FEATURE_filesystemiterator -1
#define QT_FEATURE_itemmodel 1
#define QT_FEATURE_proxymodel -1
#define QT_FEATURE_sortfilterproxymodel -1
#define QT_FEATURE_identityproxymodel -1
#define QT_FEATURE_transposeproxymodel -1
#define QT_FEATURE_concatenatetablesproxymodel -1
#define QT_FEATURE_stringlistmodel -1
#define QT_FEATURE_translation 1
#define QT_FEATURE_easingcurve -1
#define QT_FEATURE_animation -1
#define QT_FEATURE_gestures -1
#define QT_FEATURE_jalalicalendar -1
#define QT_FEATURE_islamiccivilcalendar -1
#define QT_FEATURE_timezone 1
#define QT_FEATURE_timezone_tzdb -1
#define QT_FEATURE_commandlineparser -1
#define QT_FEATURE_cborstreamreader 1
#define QT_FEATURE_cborstreamwriter 1
#define QT_FEATURE_permissions -1
#define QT_THREADSAFE_CLOEXEC -1
#endif
COREEOF

cat > "$QTGEN/QtGui/qtgui-config.h" << 'GUIEOF'
#ifndef QT_FEATURES_Gui_H
#define QT_FEATURES_Gui_H
#define QT_FEATURE_accessibility_atspi_bridge -1
#define QT_FEATURE_emojisegmenter -1
#define QT_FEATURE_freetype 1
#define QT_FEATURE_fontconfig -1
#define QT_FEATURE_harfbuzz 1
#define QT_FEATURE_opengles2 -1
#define QT_FEATURE_opengles3 -1
#define QT_FEATURE_opengles31 -1
#define QT_FEATURE_opengles32 -1
#define QT_FEATURE_dynamicgl -1
#define QT_FEATURE_opengl -1
#define QT_FEATURE_vulkan -1
#define QT_FEATURE_metal -1
#define QT_FEATURE_openvg -1
#define QT_FEATURE_egl -1
#define QT_FEATURE_ico -1
#define QT_FEATURE_sessionmanager -1
#define QT_FEATURE_xcb -1
#define QT_FEATURE_xcb_glx_plugin -1
#define QT_FEATURE_texthtmlparser -1
#define QT_FEATURE_textmarkdownreader -1
#define QT_FEATURE_system_textmarkdownreader -1
#define QT_FEATURE_textmarkdownwriter -1
#define QT_FEATURE_textodfwriter -1
#define QT_FEATURE_cssparser 1
#define QT_FEATURE_draganddrop -1
#define QT_FEATURE_action 1
#define QT_FEATURE_cursor 1
#define QT_FEATURE_clipboard -1
#define QT_FEATURE_wheelevent 1
#define QT_FEATURE_tabletevent -1
#define QT_FEATURE_im -1
#define QT_FEATURE_highdpiscaling 1
#define QT_FEATURE_validator 1
#define QT_FEATURE_standarditemmodel 1
#define QT_FEATURE_filesystemmodel -1
#define QT_FEATURE_imageformatplugin -1
#define QT_FEATURE_movie -1
#define QT_FEATURE_imageformat_bmp -1
#define QT_FEATURE_imageformat_ppm -1
#define QT_FEATURE_imageformat_xbm -1
#define QT_FEATURE_imageformat_xpm -1
#define QT_FEATURE_imageformat_png -1
#define QT_FEATURE_imageformat_jpeg -1
#define QT_FEATURE_image_heuristic_mask -1
#define QT_FEATURE_image_text -1
#define QT_FEATURE_picture -1
#define QT_FEATURE_colornames 1
#define QT_FEATURE_pdf -1
#define QT_FEATURE_desktopservices -1
#define QT_FEATURE_systemtrayicon -1
#define QT_FEATURE_accessibility -1
#define QT_FEATURE_whatsthis -1
#define QT_FEATURE_undocommand -1
#define QT_FEATURE_undostack -1
#define QT_FEATURE_undogroup -1
#define QT_FEATURE_wayland -1
#define QT_FEATURE_waylandscanner -1
#endif
GUIEOF

cp "$REPO/third_party/qt6/koppios/qtdeprecationdefinitions.h" "$QTGEN/QtCore/"
# real Qt6's CMake generates this from qconfig.cpp.in (real install paths);
# koppios has no install tree, every path is just "." -- see the file's own
# header comment for the full reasoning.
cp "$BOOT/qconfig.cpp" "$QTGEN/QtCore/qconfig.cpp"
echo '#include "qtrace_p.h"' > "$QTGEN/QtGui/qtgui_tracepoints_p.h"
echo '#include "qtrace_p.h"' > "$QTGEN/QtCore/qtcore_tracepoints_p.h"
echo '#include "qtrace_p.h"' > "$QTGEN/QtWidgets/qtwidgets_tracepoints_p.h"
# real sources also say <QtGui/qpa/foo.h>: the same files as qpa/foo.h
ln -sfn ../qpa "$QTGEN/QtGui/qpa"

echo "=== 4/6: CamelCase forwarding headers (gen_camel.py) ==="
python3 "$BOOT/gen_camel.py" "$SCRATCH"

# gen_camel.py only finds class/struct definitions; these two names are a
# typedef-only header (QRgb) and a non-class header (QHashFunctions), both
# #include'd CamelCase-style by real sources. Found by scanning every
# #include <Qt{Core,Gui}/QXxx> in src/corelib+src/gui for a missing forward.
echo '#include "qhashfunctions.h"' > "$QTGEN/QtCore/QHashFunctions"
echo '#include "qrgb.h"' > "$QTGEN/QtGui/QRgb"

# The reference SDK's qtgui-config_p.h reflects a desktop Linux build; turn
# off every feature that needs a platform backend / external lib koppios
# does not have, so QT_CONFIG() code paths that would reference them
# (e.g. qkeymapper.cpp's QEvdevKeyMapper dynamic_cast) are not compiled in.
for f in evdev kms drm_atomic linuxfb vnc egl_x11 eglfs eglfs_egldevice eglfs_gbm eglfs_x11 \
         xcb_glx xcb_egl_plugin xcb_xlib xkbcommon xkbcommon_x11 xlib tuiotouch multiprocess \
         accessibility_atspi_bridge vkgen vkkhrdisplay fontconfig run_opengl_tests \
         wayland_client wayland_server wayland_egl wayland_drm_egl_server_buffer \
         wayland_dmabuf_server_buffer wayland_shm_emulation_server_buffer \
         wayland_vulkan_server_buffer wayland_datadevice wayland_client_primary_selection \
         wayland_client_fullscreen_shell_v1 wayland_client_wl_shell wayland_client_xdg_shell \
         egl_extension_platform_wayland; do
    sed -i "s/^#define QT_FEATURE_$f 1\$/#define QT_FEATURE_$f -1/" "$QTGEN/QtGui/private/qtgui-config_p.h"
done

# Same idea for QtCore: the SDK-derived qconfig.h / qtcore-config_p.h reflect
# a desktop Linux build (zstd, zlib, ICU, glib, inotify, renameat2, pidfd).
# None of those libraries/syscalls exist on koppios.
sed -i 's/^#define QT_FEATURE_zstd 1$/#define QT_FEATURE_zstd -1/' "$QTGEN/QtCore/qconfig.h"
grep -q '^#define QT_NO_COMPRESS' "$QTGEN/QtCore/qconfig.h" || echo '#define QT_NO_COMPRESS' >> "$QTGEN/QtCore/qconfig.h"
# Real static Qt builds define QT_STATIC in qconfig.h (see qtconfigmacros.h); the SDK copy is a shared build.
grep -q '^#define QT_STATIC' "$QTGEN/QtCore/qconfig.h" || echo '#define QT_STATIC' >> "$QTGEN/QtCore/qconfig.h"
for f in icu glib inotify renameat2 forkfd_pidfd; do
    sed -i "s/^#define QT_FEATURE_$f 1\$/#define QT_FEATURE_$f -1/" "$QTGEN/QtCore/private/qtcore-config_p.h"
done

bash "$BOOT/redirect_forwards.sh" "$SCRATCH"

echo "=== source patches ==="
python3 "$BOOT/patch_osdetect.py" "$QTBASE"
python3 "$BOOT/patch_gui.py" "$QTBASE"
python3 "$BOOT/patch_minimal.py" "$QTBASE"
python3 "$BOOT/patch_widgets.py" "$QTBASE"

echo "=== 6/6: moc output for every Q_OBJECT/Q_GADGET header, using the real moc6.8.4 ==="
bash "$BOOT/genmoc.sh" "$SCRATCH"

echo "=== done. source $BOOT/compile_flags.sh $SCRATCH to compile. ==="
