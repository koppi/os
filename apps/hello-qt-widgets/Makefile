# QtWidgets test app: tabs, buttons, check/radio boxes, sliders, dial, scroll bar, progress
# bar, spin box, line edit, combo box. Real Qt 6.8 (QtCore + QtGui + QtWidgets), vendored in
# third_party/qt6-gui/.
#
#   make -C apps/hello-qt-widgets -j4      ->  ./hqtwid
#
# Opt-in like apps/hello-qt-gui (not part of `make -C apps`); the Qt objects are shared with it
# in third_party/qt6-gui/build, so whichever is built second only compiles its own source and
# links. hda.sh stages hqtwid + font.ttf when the binary exists. Needs a framebuffer boot.

Q := ../../third_party/qt6-gui
include $(Q)/koppios/qt.mk

$(eval $(call QT_APP,hqtwid,hello_qt_widgets.cpp,widgets))

all: hqtwid

clean:
	rm -f hqtwid

.PHONY: all clean
