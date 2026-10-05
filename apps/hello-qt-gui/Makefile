# Graphical "Hello, Qt6!" -- real Qt 6.8 (QtCore + QtGui + FreeType + HarfBuzz), vendored in
# third_party/qt6-gui/ and built from source for this kernel.
#
#   make -C apps/hello-qt-gui -j4      ->  ./hqtgui   (stripped; 4.7 MB loaded, 7 MB on disk)
#
# Deliberately NOT part of `make -C apps`: Qt is ~560 translation units (about 20 minutes at
# -j4 on first build, incremental afterwards; objects are shared with apps/hello-qt-widgets
# in third_party/qt6-gui/build). hda.sh stages hqtgui + font.ttf on the RAM disk only when the
# binary exists, the way it treats the Doom IWAD. Run `hqtgui` from the shell on a
# framebuffer boot. All flags live in third_party/qt6-gui/koppios/qt.mk.

Q := ../../third_party/qt6-gui
include $(Q)/koppios/qt.mk

$(eval $(call QT_APP,hqtgui,hello_qt_gui.cpp,gui))

all: hqtgui

clean:
	rm -f hqtgui

.PHONY: all clean
