SCRATCH="${1:?usage: source compile_flags.sh <scratchpad-dir>}"
QTBASE=$SCRATCH/qtbase
QTGEN=$SCRATCH/qtgen/include

INCS="-I $QTGEN -I $QTGEN/QtCore -I $QTGEN/QtGui -I $QTGEN/qpa -I $SCRATCH/qtguitest/mocgen"
INCS="$INCS -I $QTBASE/src/3rdparty/freetype/include"
INCS="$INCS -I $QTBASE/src/3rdparty/harfbuzz-ng/src -I $QTBASE/src/3rdparty/harfbuzz-ng"
INCS="$INCS -I $QTBASE/src/3rdparty/tinycbor/src"
INCS="$INCS -I $QTBASE/src/3rdparty/double-conversion"
for d in $(find "$QTBASE/src/corelib" "$QTBASE/src/gui" -maxdepth 1 -mindepth 1 -type d 2>/dev/null) \
         $(find "$QTBASE/src/gui" -maxdepth 2 -mindepth 2 -type d 2>/dev/null | grep -v 3rdparty); do
  INCS="$INCS -I $d"
done
INCS="$INCS -I $QTBASE/src/corelib -I $QTBASE/src/gui"
INCS="$INCS -I $QTBASE/src/plugins/platforms/minimal"
INCS="$INCS -I $QTBASE/mkspecs/common/posix"
# QtWidgets: appended LAST so header resolution for corelib/gui is unchanged.
INCS="$INCS -I $QTGEN/QtWidgets"
for d in $(find "$QTBASE/src/widgets" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | grep -v "/doc$") \
         $(find "$QTBASE/src/widgets" -maxdepth 2 -mindepth 2 -type d 2>/dev/null | grep -v "/doc"); do
  INCS="$INCS -I $d"
done
INCS="$INCS -I $QTBASE/src/widgets"

# Real Qt source guards a lot of code with the OLDER QT_NO_<FEATURE> style
# macros (predating the QT_CONFIG()/QT_FEATURE_* system), independently of
# and in ADDITION to the qt*-config.h feature flags above -- both need to
# agree, or code gated only by the old style compiles in even though the
# matching QT_FEATURE_* is off. One legacy define per QT_FEATURE_x we set to
# -1 in qt{core,gui}-config.h.
QT_NO_DEFINES="-DQT_NO_OPENGL -DQT_NO_OPENGLES2 -DQT_NO_EGL -DQT_NO_VULKAN \
    -DQT_NO_SESSIONMANAGER -DQT_NO_SYSTEMTRAYICON -DQT_NO_ACCESSIBILITY \
    -DQT_NO_WHATSTHIS -DQT_NO_UNDOCOMMAND -DQT_NO_UNDOSTACK -DQT_NO_UNDOGROUP \
    -DQT_NO_DRAGANDDROP -DQT_NO_CLIPBOARD -DQT_NO_TABLETEVENT \
    -DQT_NO_IM -DQT_NO_FILESYSTEMMODEL \
    -DQT_NO_IMAGEFORMATPLUGIN -DQT_NO_MOVIE -DQT_NO_PICTURE -DQT_NO_PDF \
    -DQT_NO_DESKTOPSERVICES -DQT_NO_SETTINGS -DQT_NO_PROCESS \
    -DQT_NO_SHAREDMEMORY -DQT_NO_SYSTEMSEMAPHORE -DQT_NO_LIBRARY \
    -DQT_NO_FILESYSTEMWATCHER -DQT_NO_TEMPORARYFILE -DQT_NO_TRANSLATION \
    -DQT_NO_ANIMATION -DQT_NO_GESTURES -DQT_NO_COMMANDLINEPARSER \
    -DQT_NO_IMAGEFORMAT_PNG -DQT_NO_IMAGEFORMAT_BMP -DQT_NO_IMAGEFORMAT_PPM \
    -DQT_NO_IMAGEFORMAT_XBM -DQT_NO_IMAGEFORMAT_XPM -DQT_NO_IMAGEFORMAT_JPEG \
    -DQT_NO_GLIB -DQT_NO_CONTEXTMENU -DQT_NO_TEXTHTMLPARSER -DQT_NO_TEXTODFWRITER"

# NOTE: -fkeep-inline-functions used to be global here, to recover a handful
# of inline member functions (QDataStream::status(), ...) that -Os leaves
# unemitted in every TU lacking a local call site. Dropped: it forces GCC to
# emit EVERY inline function's body in EVERY TU that merely includes its
# header, regardless of whether anything calls it there. --gc-sections only
# prunes the final executable -- the bloat already happened in the .o files
# themselves (396 objects hit 1.1GB on disk, individual .o up to 5.5MB vs.
# a few hundred KB without it), and ar/ld have to read and index all of that
# before any pruning occurs, which is what was blowing out this sandbox's
# memory budget. Any inline symbols this turns out to still be missing get
# fixed individually (real link-error-driven), not papered over globally.
CXXCOMMON="-m32 -std=gnu++20 -fno-builtin -fno-exceptions -fno-rtti -fno-strict-aliasing \
    -ffunction-sections -fdata-sections \
    -Os -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
    -D__KOPPIOS__ -DQT_USE_QSTRINGBUILDER -DQT_BUILD_GUI_LIB -DQT_BUILD_CORE_LIB \
    -DQT_DISABLE_DEPRECATED_UP_TO=QT_VERSION \
    -DQT_QPA_DEFAULT_PLATFORM_NAME=\"minimal\" \
    $QT_NO_DEFINES \
    -UQLOGGING_HAVE_BACKTRACE -UQLOGGING_USE_EXECINFO_BACKTRACE \
    -include $QTGEN/QtCore/qglobal.h -include dirent.h \
    -w $INCS"
