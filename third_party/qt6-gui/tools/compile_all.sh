#!/bin/bash
# Batch-compiles the full corelib+gui candidate closure, the real koppios
# file-engine replacement for the POSIX QFSFileEngine, and PCRE2 (16-bit,
# real QRegularExpression backend) -- everything obj/*.o + pcre2obj/*.o
# needs to hold before attempting the link. Safe to rerun (skips objects
# already newer than their source).
set -u
SCRATCH="${1:?usage: compile_all.sh <scratchpad-dir>}"
QTBASE=$SCRATCH/qtbase
TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; Q6G="$(cd "$TOOLS/.." && pwd)"; REPO="$(cd "$Q6G/../.." && pwd)"
BOOT="$TOOLS"
OBJDIR=$SCRATCH/qtguitest/obj
PCREDIR=$SCRATCH/qtguitest/pcre2obj
mkdir -p "$OBJDIR" "$PCREDIR"
source "$BOOT/compile_flags.sh" "$SCRATCH"

echo "=== candidate file list ==="
( cd "$QTBASE" && find src/corelib src/gui -iname "*.cpp" \
    | grep -vE "/(win32|windows|unix|wasm|android|darwin|ios|mac|cocoa|integrityfb|vxworks|qnx|3rdparty|doc)/" \
    | grep -vE "_(win|mac|macx|wasm|android|ios|qnx|integrity)\.cpp$" \
    | grep -vE "qpnghandler\.cpp$" \
    | grep -vE "qfsfileengine(_unix|_iterator)?\.cpp$" \
    | grep -vE "qfilesystemwatcher(_polling|_kqueue|_inotify)?\.cpp$" \
    | grep -vE "qtemporaryfile\.cpp$" \
    | grep -vE "qsavefile\.cpp$" \
    | grep -vE "qstandardpaths_unix\.cpp$" \
    | grep -vE "qstorageinfo_linux\.cpp$" \
    | grep -vE "qimagescale_sse4\.cpp$" \
    | grep -vE "qdrawhelper_(sse4|ssse3|avx2)\.cpp$" \
    | grep -vFxf "$BOOT/exclude_files.txt" \
) > "$SCRATCH/qtguitest/all_candidate_files.txt"
wc -l "$SCRATCH/qtguitest/all_candidate_files.txt"

: > "$SCRATCH/qtguitest/compile_fail_log.txt"

compile_one() {
  rel="$1"
  base=$(basename "$rel" .cpp)
  obj="$OBJDIR/$base.o"
  src="$QTBASE/$rel"
  if [ "$obj" -nt "$src" ] 2>/dev/null; then
    return 0
  fi
  err=$(g++ $CXXCOMMON -c "$src" -o "$obj" 2>&1)
  if [ $? -ne 0 ]; then
    rm -f "$obj"
    {
      echo "=== FAIL: $rel ==="
      echo "$err" | head -15
      echo ""
    } >> "$SCRATCH/qtguitest/compile_fail_log.txt"
  fi
}
export -f compile_one
export SCRATCH QTBASE OBJDIR CXXCOMMON

echo "=== corelib+gui candidates ==="
cat "$SCRATCH/qtguitest/all_candidate_files.txt" | xargs -P 11 -I{} bash -c 'compile_one "$@"' _ {}
OK=$(ls "$OBJDIR" | wc -l)
FAIL=$(grep -c "^=== FAIL" "$SCRATCH/qtguitest/compile_fail_log.txt")
echo "OK=$OK FAIL=$FAIL"

echo "=== moc_qnamespace.cpp (Q_NAMESPACE_EXPORT on the Qt:: namespace itself -- no real qnamespace.cpp exists to #include this into, so it needs compiling as its own translation unit; provides Qt::staticMetaObject) ==="
g++ $CXXCOMMON -c "$SCRATCH/qtguitest/mocgen/moc_qnamespace.cpp" -o "$OBJDIR/moc_qnamespace.o" 2>&1 | tail -10

echo "=== orphan moc_*.cpp (headers whose moc output no compiled .cpp #includes -- e.g. *_p.h classes like QUnixEventDispatcherQPA; compiled best-effort, files for excluded modules simply fail and are dropped; unreferenced archive members never reach the link) ==="
export LC_ALL=C
( cd "$QTBASE"
  { cat "$SCRATCH/qtguitest/all_candidate_files.txt"
    echo src/plugins/platforms/minimal/qminimalintegration.cpp
    echo src/plugins/platforms/minimal/qminimalbackingstore.cpp
    find src/widgets -iname "*.cpp"
  } | while read -r f; do grep -ohE '#[[:space:]]*include[[:space:]]*"moc_[A-Za-z0-9_]+\.cpp"' "$f" 2>/dev/null; done \
    | sed -E 's/.*"(moc_[^"]+)".*/\1/' | sort -u > "$SCRATCH/qtguitest/moc_included.txt"
  ls "$SCRATCH/qtguitest/mocgen" | grep -E '^moc_.*\.cpp$' | sort -u > "$SCRATCH/qtguitest/moc_all.txt"
  comm -23 "$SCRATCH/qtguitest/moc_all.txt" "$SCRATCH/qtguitest/moc_included.txt" > "$SCRATCH/qtguitest/moc_orphans.txt" )
compile_moc() {
  m="$1"; b="${m%.cpp}"
  out="$OBJDIR/$b.o"; src="$SCRATCH/qtguitest/mocgen/$m"
  [ "$out" -nt "$src" ] 2>/dev/null && return 0
  g++ $CXXCOMMON -c "$src" -o "$out" >/dev/null 2>&1 || rm -f "$out"
}
export -f compile_moc
xargs -P 11 -I{} bash -c 'compile_moc "$@"' _ {} < "$SCRATCH/qtguitest/moc_orphans.txt"
echo "orphan mocs: $(wc -l < "$SCRATCH/qtguitest/moc_orphans.txt") listed, $(ls "$OBJDIR"/moc_*.o 2>/dev/null | wc -l) moc objects now present"

echo "=== qgrayraster.c (real Qt's own FreeType-derived rasterizer -- a .c file, the *.cpp candidate scan above never finds it; easy to accidentally recompile ftgrays.c from 3rdparty/freetype instead, which defines DIFFERENT symbol names and silently doesn't satisfy the real qt_ft_grays_raster/q_gray_rendered_spans references) ==="
gcc -m32 -fno-builtin -fno-strict-aliasing -ffunction-sections -fdata-sections -Os \
  -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
  -D__KOPPIOS__ -DQT_BUILD_GUI_LIB -DQT_BUILD_CORE_LIB -DQT_DISABLE_DEPRECATED_UP_TO=QT_VERSION \
  -include "$QTGEN/QtCore/qglobal.h" -include dirent.h \
  -w $INCS \
  -c "$QTBASE/src/gui/painting/qgrayraster.c" -o "$OBJDIR/qgrayraster.o" 2>&1 | tail -10

echo "=== qstandardpaths_koppios.cpp (koppios's own QStandardPaths::standardLocations -- no per-user directory convention to report) ==="
g++ $CXXCOMMON -c "$Q6G/koppios/qstandardpaths_koppios.cpp" -o "$OBJDIR/qstandardpaths_koppios.o" 2>&1 | tail -10

echo "=== CPU-feature-specific painting helpers (the shared -m32 flags above target the generic i386 baseline; each of these real Qt6 files needs its own -m<feature> to compile the intrinsics it uses) ==="
compile_simd() {
  local f="$1" flag="$2"
  local src; src=$(find "$QTBASE/src/gui" -name "$f.cpp")
  [ -z "$src" ] && return
  local out="$OBJDIR/$f.o"
  [ "$out" -nt "$src" ] 2>/dev/null && return
  g++ $CXXCOMMON $flag -c "$src" -o "$out" 2>&1 | tail -10
}
compile_simd qimagescale_sse4 -msse4.1
compile_simd qdrawhelper_sse4 -msse4.1
compile_simd qdrawhelper_ssse3 -mssse3
compile_simd qdrawhelper_avx2 "-mavx2 -mbmi -mbmi2 -mf16c -mfma -mlzcnt -mpopcnt"

echo "=== generic-unix event dispatcher (lives under src/gui/platform/unix/, which the directory-name exclusion above sweeps out along with real per-OS variant files -- but this is the REAL generic glue qminimalintegration.cpp's createUnixEventDispatcher() needs, not an excludable OS variant; pulled in explicitly rather than loosening that filter, since most of its siblings in the same directory genuinely are X11/DBus-specific and should stay excluded) ==="
for f in qgenericunixeventdispatcher qunixeventdispatcher; do
  src="$QTBASE/src/gui/platform/unix/$f.cpp"
  [ -f "$src" ] || continue
  out="$OBJDIR/$f.o"
  [ "$out" -nt "$src" ] 2>/dev/null && continue
  g++ $CXXCOMMON -c "$src" -o "$out" 2>&1 | tail -10
done

echo "=== double-conversion (real Grisu/Bignum dtoa/strtod Qt's own QLocale/QString number parsing calls into -- headers alone aren't enough, these real .cc files provide the actual algorithm) ==="
DCDIR="$QTBASE/src/3rdparty/double-conversion/double-conversion"
mkdir -p "$SCRATCH/qtguitest/dc_patched"
# koppios addition, not upstream: std::locale::classic()'s ctype<char>::tolower
# IS plain ASCII tolower; libstdc++'s locale machinery (facets, __cxa_guard,
# exceptions) is not available here, so swap that one lookup for ASCII.
python3 - "$DCDIR/string-to-double.cc" "$SCRATCH/qtguitest/dc_patched/string-to-double.cc" <<'PYEOF'
import sys
s = open(sys.argv[1]).read()
old = """  static const std::ctype<char>& cType =
      std::use_facet<std::ctype<char> >(std::locale::classic());
  return cType.tolower(ch);"""
assert old in s
open(sys.argv[2], "w").write(s.replace(old, "  return (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;"))
PYEOF
for f in bignum bignum-dtoa cached-powers double-to-string fast-dtoa fixed-dtoa strtod string-to-double; do
  src="$DCDIR/$f.cc"
  [ "$f" = string-to-double ] && src="$SCRATCH/qtguitest/dc_patched/string-to-double.cc"
  [ -f "$src" ] || continue
  out="$OBJDIR/dc_$f.o"
  [ "$out" -nt "$src" ] 2>/dev/null && continue
  g++ -m32 -std=gnu++17 -w -fno-builtin -fno-exceptions -fno-rtti \
    -ffunction-sections -fdata-sections -Os -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
    -I "$QTBASE/src/3rdparty/double-conversion" -I "$DCDIR" \
    -c "$src" -o "$out" 2>&1 | tail -10
done

echo "=== HarfBuzz (real Qt6 bundles it as a static lib, compiled with its OWN defines/SOURCES list from src/3rdparty/harfbuzz-ng/CMakeLists.txt -- not part of the corelib/gui candidate scan above, and the wrong include-only setup silently links against nothing) ==="
HBDIR="$QTBASE/src/3rdparty/harfbuzz-ng/src"
HBOBJDIR="$SCRATCH/qtguitest/hbobj"
mkdir -p "$HBOBJDIR"
HBDEFS="-DHAVE_ATEXIT -DHAVE_CONFIG_H -DHB_EXTERN= -DHB_NDEBUG -DHB_NO_UNICODE_FUNCS \
  -DQT_NO_VERSION_TAGGING -DHAVE_PTHREAD -DHAVE_SCHED_H -DHAVE_SCHED_YIELD -DHAVE_OT"
HBFILES="hb-dummy hb-aat-layout hb-aat-map hb-blob hb-buffer hb-buffer-serialize \
  hb-buffer-verify hb-draw hb-face hb-face-builder hb-fallback-shape hb-font \
  hb-map hb-number hb-outline hb-paint hb-paint-bounded hb-paint-extents \
  hb-set hb-shape hb-shape-plan hb-shaper hb-style hb-subset hb-subset-cff-common \
  hb-subset-cff1 hb-subset-cff2 hb-subset-input hb-subset-instancer-iup \
  hb-subset-instancer-solver hb-subset-plan hb-subset-plan-layout \
  hb-subset-serialize hb-subset-plan-var hb-unicode \
  hb-ot-cff1-table hb-ot-cff2-table hb-ot-color hb-ot-face hb-ot-font \
  hb-ot-layout hb-ot-map hb-ot-math hb-ot-meta hb-ot-metrics hb-ot-name \
  hb-ot-shape hb-ot-shape-fallback hb-ot-shape-normalize \
  hb-ot-shaper-arabic hb-ot-shaper-default hb-ot-shaper-hangul hb-ot-shaper-hebrew \
  hb-ot-shaper-indic hb-ot-shaper-indic-table hb-ot-shaper-khmer hb-ot-shaper-myanmar \
  hb-ot-shaper-syllabic hb-ot-shaper-thai hb-ot-shaper-use \
  hb-ot-shaper-vowel-constraints hb-ot-tag hb-ot-var"
for f in $HBFILES; do
  src="$HBDIR/$f.cc"
  [ -f "$src" ] || continue
  out="$HBOBJDIR/$f.o"
  [ "$out" -nt "$src" ] 2>/dev/null && continue
  g++ $CXXCOMMON $HBDEFS -I "$HBDIR" \
    -c "$src" -o "$out" 2>"$HBOBJDIR/$f.err"
done
g++ $CXXCOMMON $HBDEFS -I "$HBDIR" -I "$QTBASE/src/3rdparty/harfbuzz-ng/src/OT/Var/VARC" \
  -c "$QTBASE/src/3rdparty/harfbuzz-ng/src/OT/Var/VARC/VARC.cc" -o "$HBOBJDIR/VARC.o" 2>"$HBOBJDIR/VARC.err"
g++ $CXXCOMMON $HBDEFS -I "$HBDIR" -I "$QTBASE/src/3rdparty/harfbuzz-ng" \
  -c "$QTBASE/src/3rdparty/harfbuzz-ng/hb-dummy.cc" -o "$HBOBJDIR/hb-dummy.o" 2>"$HBOBJDIR/hb-dummy.err"
HBOK=$(ls "$HBOBJDIR"/*.o 2>/dev/null | wc -l)
echo "harfbuzz: $HBOK objects"

echo "=== QtWidgets (kernel/ styles/ widgets/ util/ itemviews/ minus exclude_widgets.txt) -> wobj/ ==="
WOBJDIR="$SCRATCH/qtguitest/wobj"; mkdir -p "$WOBJDIR"
( cd "$QTBASE" && for d in kernel styles widgets util itemviews; do find src/widgets/$d -iname "*.cpp"; done \
    | grep -vFxf "$BOOT/exclude_widgets.txt" ) > "$SCRATCH/qtguitest/widgets_candidates.txt"
: > "$SCRATCH/qtguitest/compile_fail_log_w.txt"
compile_one_w() {
  rel="$1"; base=$(basename "$rel" .cpp); obj="$WOBJDIR/$base.o"; src="$QTBASE/$rel"
  [ "$obj" -nt "$src" ] 2>/dev/null && return 0
  err=$(g++ $CXXCOMMON -c "$src" -o "$obj" 2>&1)
  if [ $? -ne 0 ]; then
    rm -f "$obj"
    { echo "=== FAIL: $rel ==="; echo "$err" | head -15; echo ""; } >> "$SCRATCH/qtguitest/compile_fail_log_w.txt"
  fi
}
export -f compile_one_w
export WOBJDIR
xargs -P 11 -I{} bash -c 'compile_one_w "$@"' _ {} < "$SCRATCH/qtguitest/widgets_candidates.txt"
echo "widgets: $(ls "$WOBJDIR"/*.o 2>/dev/null | wc -l) objects, $(grep -c '^=== FAIL' "$SCRATCH/qtguitest/compile_fail_log_w.txt") failed of $(wc -l < "$SCRATCH/qtguitest/widgets_candidates.txt")"

echo "=== Qt's real 'minimal' QPA platform integration (src/plugins/platforms/minimal; qt_koppios_platform.cpp constructs it directly, no plugin loader) ==="
for f in qminimalintegration qminimalbackingstore; do
  src="$QTBASE/src/plugins/platforms/minimal/$f.cpp"
  out="$OBJDIR/$f.o"
  [ "$out" -nt "$src" ] 2>/dev/null && continue
  g++ $CXXCOMMON -c "$src" -o "$out" 2>&1 | grep -E "error" | head -10
done

echo "=== FreeType (trimmed to TrueType + smooth/mono rasterizers + psnames + autofit; koppios ftsystem/ftgzip, see freetype_koppios/) ==="
FTDIR="$QTBASE/src/3rdparty/freetype"
FTOBJ="$SCRATCH/qtguitest/ftobj"; mkdir -p "$FTOBJ"
FTFLAGS="-m32 -Os -w -fno-builtin -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
  -ffunction-sections -fdata-sections -DFT2_BUILD_LIBRARY -DTT_CONFIG_OPTION_SUBPIXEL_HINTING \
  -I $Q6G/koppios/freetype -I $FTDIR/include"
for rel in src/autofit/autofit.c src/base/ftbase.c src/base/ftbbox.c src/base/ftbdf.c src/base/ftbitmap.c \
           src/base/ftcid.c src/base/ftfstype.c src/base/ftgasp.c src/base/ftglyph.c src/base/ftgxval.c \
           src/base/ftinit.c src/base/ftmm.c src/base/ftotval.c src/base/ftpatent.c src/base/ftpfr.c \
           src/base/ftstroke.c src/base/ftsynth.c src/base/fttype1.c src/base/ftwinfnt.c src/base/ftdebug.c \
           src/psnames/psnames.c src/raster/raster.c src/sfnt/sfnt.c src/smooth/smooth.c src/truetype/truetype.c; do
  out="$FTOBJ/ft_$(basename "$rel" .c).o"
  [ "$out" -nt "$FTDIR/$rel" ] 2>/dev/null && continue
  gcc $FTFLAGS -c "$FTDIR/$rel" -o "$out" 2>&1 | grep -E "error" | head -5
done
gcc $FTFLAGS -c "$Q6G/koppios/freetype/ftsystem_koppios.c" -o "$FTOBJ/ftsystem_koppios.o" 2>&1 | grep -E "error" | head -5
gcc $FTFLAGS -c "$Q6G/koppios/freetype/ftgzip_koppios.c" -o "$FTOBJ/ftgzip_koppios.o" 2>&1 | grep -E "error" | head -5
echo "freetype: $(ls "$FTOBJ"/*.o 2>/dev/null | wc -l) objects"

echo "=== koppios file-engine (replaces POSIX QFSFileEngine -- see its own file comment) ==="
g++ $CXXCOMMON -c "$REPO/lib/qfileengine_koppios.cpp" -o "$OBJDIR/qfileengine_koppios.o" 2>&1 | tail -10
g++ $CXXCOMMON -c "$REPO/lib/qfsfileengine_koppios_stub.cpp" -o "$OBJDIR/qfsfileengine_koppios_stub.o" 2>&1 | tail -10
g++ $CXXCOMMON -c "$Q6G/koppios/qshader_koppios.cpp" -o "$OBJDIR/qshader_koppios.o" 2>&1 | tail -10
g++ $CXXCOMMON -c "$Q6G/koppios/qkoppiosfontdatabase.cpp" -o "$OBJDIR/qkoppiosfontdatabase.o" 2>&1 | tail -10
g++ $CXXCOMMON -c "$Q6G/koppios/qresources_koppios.cpp" -o "$OBJDIR/qresources_koppios.o" 2>&1 | tail -10

echo "=== pcre2 (16-bit, real QRegularExpression backend -- no 32-bit system lib available) ==="
( cd "$QTBASE/src/3rdparty/pcre2/src"
  PFAIL=0; POK=0
  for f in *.c; do
    case "$f" in pcre2_dftables.c|pcre2posix.c|pcre2_jit_match.c|pcre2_jit_misc.c) continue;; esac
    out="$PCREDIR/${f%.c}.o"
    [ "$out" -nt "$f" ] 2>/dev/null && continue
    gcc -m32 -c -Os -w -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -DHAVE_CONFIG_H -DPCRE2_CODE_UNIT_WIDTH=16 -DPCRE2_STATIC \
      -I. "$f" -o "$out" 2>"$PCREDIR/${f%.c}.err"
    if [ $? -eq 0 ]; then POK=$((POK+1)); else PFAIL=$((PFAIL+1)); fi
  done
  echo "pcre2: OK=$POK FAIL=$PFAIL"
)
