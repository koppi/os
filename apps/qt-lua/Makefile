# qtlua: Lua 5.4 with the Qt6 Lua bindings of tsujikiri-qt (github.com/koppi/tsujikiri-qt), a ring-3 program on real Qt 6.8
# (QtCore + QtGui + QtWidgets, vendored in third_party/qt6-gui/).
#
#   make -C apps/qt-lua -j2      ->  ./qtlua
#
# Opt-in like the other Qt apps (not part of `make -C apps`). The bindings are generated from the
# headers of the vendored Qt, so they need a checkout of tsujikiri-qt next to this one (as
# tsujikiri-qt or tsujikiri-qt6, or TSUJIKIRI_QT6=/path), tsujikiri in a Python virtual environment (made on demand, see
# koppios/requirements.txt there) and a git clone of LuaBridge3. All the rules live in
# $(TSUJIKIRI_QT6)/koppios/koppios.mk; the Qt objects are shared with the other Qt apps.
# hda.sh stages qtlua and the scripts of $(TSUJIKIRI_QT6)/koppios/scripts when the binary exists.
#
# One translation unit of bindings needs about a gigabyte: -j2 is plenty on a 16 GB machine.

Q := ../../third_party/qt6-gui
include $(Q)/koppios/qt.mk

TSUJIKIRI_QT6 ?= $(abspath $(firstword $(wildcard ../../../tsujikiri-qt6 ../../../tsujikiri-qt) ../../../tsujikiri-qt))
ifeq ($(wildcard $(TSUJIKIRI_QT6)/koppios/koppios.mk),)
$(error tsujikiri-qt not found at $(TSUJIKIRI_QT6): clone it next to this repository or set TSUJIKIRI_QT6)
endif
include $(TSUJIKIRI_QT6)/koppios/koppios.mk

all: qtlua

clean: qtlua-clean

.PHONY: all clean
