#!/bin/bash
set -u
SCRATCH="${1:?usage: genmoc.sh <scratchpad-dir>}"
TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; Q6G="$(cd "$TOOLS/.." && pwd)"; REPO="$(cd "$Q6G/../.." && pwd)"
BOOT="$TOOLS"
QTBASE=$SCRATCH/qtbase
QTGEN=$SCRATCH/qtgen/include
MOC="${MOC:-$HOME/.cache/qt6-gui/moc6.8.4}"   # real moc 6.8.4 built from qtbase/src/tools/moc (not checked in)
MOCDIR=$SCRATCH/qtguitest/mocgen
mkdir -p "$MOCDIR"

source "$BOOT/compile_flags.sh" "$SCRATCH"

# moc doesn't understand every g++-only flag; give it just -I/-D via INCS plus
# defines -- MUST match CXXCOMMON's defines exactly (same QT_NO_* set too),
# or moc sees methods the real compile has gated out (or vice versa) and the
# generated meta-call table references symbols the class doesn't actually have.
MOCDEFS="-D__KOPPIOS__ -DQT_USE_QSTRINGBUILDER -DQT_BUILD_GUI_LIB -DQT_BUILD_CORE_LIB \
    -DQT_DISABLE_DEPRECATED_UP_TO=QT_VERSION $QT_NO_DEFINES"

count=0
fail=0
for f in $(grep -lrE "Q_OBJECT|Q_GADGET|Q_NAMESPACE" "$QTBASE/src/corelib" "$QTBASE/src/gui" "$QTBASE/src/widgets" --include="*.h" 2>/dev/null | grep -vE "/(3rdparty|doc)/"); do
  base=$(basename "$f" .h)
  out="$MOCDIR/moc_$base.cpp"
  $MOC $MOCDEFS $INCS "$f" -o "$out" 2>>"$SCRATCH/moc_errors.log"
  if [ -s "$out" ]; then
    count=$((count+1))
  else
    fail=$((fail+1))
  fi
done
echo "moc generated: $count, empty/failed: $fail"

# A handful of .cpp files define a local Q_OBJECT/Q_GADGET class and
# #include "themselves.moc" at the bottom (qthreadpool.cpp, qpixmapcache.cpp,
# ...) instead of using a moc_*.cpp companion. Different output name
# convention (<base>.moc, not moc_<base>.cpp), same moc binary.
count2=0
for f in $(grep -lrE "Q_OBJECT|Q_GADGET|Q_NAMESPACE" "$QTBASE/src/corelib" "$QTBASE/src/gui" "$QTBASE/src/widgets" --include="*.cpp" 2>/dev/null | grep -vE "/(3rdparty|doc)/"); do
  base=$(basename "$f" .cpp)
  out="$MOCDIR/$base.moc"
  $MOC $MOCDEFS $INCS "$f" -o "$out" 2>>"$SCRATCH/moc_errors.log"
  [ -s "$out" ] && count2=$((count2+1))
done
echo "self-moc generated: $count2"
