# Real, vendored Qt 6.8.4 source (third_party/qt6/, see its README.md) --
# QT_BOOTSTRAPPED config: no QObject, no threading, just QString and its
# real dependency closure (locale-aware number formatting included).
#
# Compiled with real host system headers (same as apps/hello-str and every
# other real-STL app: no -ffreestanding), but linked against this kernel's
# own runtime/syscalls, not host glibc.

QT6=../../third_party/qt6

CXX=g++
CC=gcc
LD=ld

# Real Qt's own Bootstrap/moc build additionally sets QT_NO_CAST_FROM_ASCII/
# QT_NO_CAST_TO_ASCII -- a code-style discipline for Qt's OWN source, not
# something a QString consumer needs (it would make QString("literal")
# illegal, which this app relies on).
QTDEFS = -D__KOPPIOS__ -DQT_BOOTSTRAPPED -DQT_TYPESAFE_FLAGS -DQT_USE_QSTRINGBUILDER \
  -DQT_NO_FOREACH \
  -DQT_VERSION_MAJOR=6 -DQT_VERSION_MINOR=8 -DQT_VERSION_PATCH=4 -DQT_VERSION_STR='"6.8.4"'

QTINC = -I $(QT6)/koppios -I $(QT6)/koppios/QtCore \
  -I $(QT6)/src/corelib -I $(QT6)/src/corelib/global -I $(QT6)/src/corelib/text \
  -I $(QT6)/src/corelib/time -I $(QT6)/src/corelib/tools -I $(QT6)/src/corelib/kernel \
  -I $(QT6)/mkspecs/common/posix

# -fno-exceptions -fno-rtti: no unwinder/typeinfo runtime exists for this
# target (lib/cxxabi.cpp, shared with every C++ app). -ffunction-sections +
# --gc-sections (LDFLAGS below) drop the parts of vendored Qt's own real
# dependency closure this app never calls (QRegularExpression, most of
# QLocale's calendar backends, ...) rather than needing all of it to link.
CXXFLAGS += -Wall -Wextra -Werror -std=gnu++20 -m32 -w -fno-builtin -Os
CXXFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib
CXXFLAGS += -ffunction-sections -fdata-sections
CXXFLAGS += $(QTDEFS) $(QTINC)

CFLAGS += -m32 -w -fno-builtin -fno-pic -fno-pie -fno-stack-protector

LDFLAGS += -melf_i386 -T hello_qt.lds -Map hello_qt.map --gc-sections -s

QTOBJS = qstring.o qbytearray.o qarraydata.o qstringconverter.o qmalloc.o qlogging.o \
  qassert.o qhash.o qbytearraymatcher.o qlatin1stringmatcher.o qtenvironmentvariables.o \
  qstringlist.o qbytearraylist.o qringbuffer.o qstringbuilder.o qlocale.o qlocale_tools.o \
  qcalendar.o qdatetime.o qgregoriancalendar.o qromancalendar.o qlocaltime.o qtimezone.o

LIBGCC := $(shell $(CXX) -m32 -print-libgcc-file-name)

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp -o main.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qstring.cpp -o qstring.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qbytearray.cpp -o qbytearray.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/tools/qarraydata.cpp -o qarraydata.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qstringconverter.cpp -o qstringconverter.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/global/qmalloc.cpp -o qmalloc.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/global/qlogging.cpp -o qlogging.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/global/qassert.cpp -o qassert.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/tools/qhash.cpp -o qhash.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qbytearraymatcher.cpp -o qbytearraymatcher.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qlatin1stringmatcher.cpp -o qlatin1stringmatcher.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/global/qtenvironmentvariables.cpp -o qtenvironmentvariables.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qstringlist.cpp -o qstringlist.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qbytearraylist.cpp -o qbytearraylist.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/tools/qringbuffer.cpp -o qringbuffer.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qstringbuilder.cpp -o qstringbuilder.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qlocale.cpp -o qlocale.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/text/qlocale_tools.cpp -o qlocale_tools.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/time/qcalendar.cpp -o qcalendar.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/time/qdatetime.cpp -o qdatetime.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/time/qgregoriancalendar.cpp -o qgregoriancalendar.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/time/qromancalendar.cpp -o qromancalendar.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/time/qlocaltime.cpp -o qlocaltime.o
	@$(CXX) $(CXXFLAGS) -c $(QT6)/src/corelib/time/qtimezone.cpp -o qtimezone.o
	@$(CC) $(CFLAGS) -c $(QT6)/koppios/qt_libc_compat.c -o qt_libc_compat.o
	@$(LD) $(LDFLAGS) -o helloqt ../../lib/cxx_start.o main.o $(QTOBJS) qt_libc_compat.o \
		../../lib/cxxabi.o ../../lib/stdio.o ../../lib/string.o ../../lib/system_calls.o ../../lib/stdlib.o \
		$(LIBGCC)

clean:
	@rm -f helloqt hello_qt.map *.o
