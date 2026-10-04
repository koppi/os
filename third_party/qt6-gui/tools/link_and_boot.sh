#!/bin/bash
# Archives the objects, links the t3 test (minimal QGuiApplication construction)
# against the koppios libc, stages it into a private copy of initrd.img and
# boots it under QEMU. usage: link_and_boot.sh <scratchpad-dir> [app-source: t3|t2]
set -u
SCRATCH="${1:?usage: link_and_boot.sh <scratchpad-dir> [t3]}"
APP="${2:-t3}"
TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; Q6G="$(cd "$TOOLS/.." && pwd)"; REPO="$(cd "$Q6G/../.." && pwd)"
BOOT="$TOOLS"; OS="$REPO"
QT=$SCRATCH/qtguitest; LIB=$OS/lib
source "$BOOT/compile_flags.sh" "$SCRATCH"
cd "$QT"
if [ -f "$BOOT/probes/$APP.cpp" ]; then SRC="$BOOT/probes/$APP.cpp"; else SRC="$REPO/apps/hello-qt-gui/$APP.cpp"; fi
g++ $CXXCOMMON -c "$SRC" -o $APP.o || exit 1
g++ $CXXCOMMON -c "$Q6G/koppios/qt_koppios_platform.cpp" -o qt_koppios_platform.o || exit 1
rm -f qtcore_gui.a harfbuzz.a pcre2.a freetype.a
ar rcs qtcore_gui.a obj/*.o; ar rcs harfbuzz.a hbobj/*.o; ar rcs pcre2.a pcre2obj/*.o; ar rcs freetype.a ftobj/*.o
( cd $LIB && make >/dev/null 2>&1 )
g++ -m32 -nostdlib -no-pie -Wl,-T,"$REPO/apps/hello-qt-gui/hello_qt_gui.lds" -Os -Wl,--gc-sections -Wl,--no-keep-memory \
  -o hqtgui_$APP $APP.o qt_koppios_platform.o \
  -Wl,--start-group qtcore_gui.a harfbuzz.a pcre2.a freetype.a \
  $LIB/cxx_start.o $LIB/mutex.o $LIB/system_calls.o $LIB/unistd.o $LIB/stdlib.o $LIB/string.o $LIB/stdio.o \
  $LIB/cxxabi.o $LIB/cxx_string.o $LIB/cxx_rbtree.o $LIB/cxx_hashtable.o $LIB/cxx_chrono.o $LIB/cxx_pmr.o \
  $LIB/cxx_condvar.o $LIB/cxx_list.o $LIB/pthread_glibc.o $LIB/libm.o $LIB/libc_ext.o $LIB/emutls.o \
  -lgcc -Wl,--end-group > /tmp/link_$APP.log 2>&1
echo "link exit: $?"
grep -oE "undefined reference to \`[^']+'" /tmp/link_$APP.log | sed "s/undefined reference to \`//;s/'$//" | sort -u | c++filt | head -30
[ -f hqtgui_$APP ] || exit 1
strip -s -o hqtgui_$APP.stripped hqtgui_$APP 2>/dev/null
cp "$OS/initrd.img" "$SCRATCH/test_initrd.img"
mcopy -i "$SCRATCH/test_initrd.img" -D o hqtgui_$APP.stripped ::hqtgui
# optional test font (scratch-only: font choice/licensing for shipping is the user's call)
FONT="${FONT:-/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf}"
[ -f "$FONT" ] && mcopy -i "$SCRATCH/test_initrd.img" -D o "$FONT" ::font.ttf
cd "$OS"
# MON_SCRIPT="34:screendump a.ppm;37:screendump b.ppm;40:sendkey esc": timed QEMU monitor commands
# (seconds after start; relative filenames land in $SCRATCH). SHOT_AT=N is shorthand for "N:screendump shot.ppm".
MON="$SCRATCH/qemu_mon.sock"; rm -f "$MON" "$SCRATCH"/*.ppm
MON_SCRIPT="${MON_SCRIPT:-${SHOT_AT:+$SHOT_AT:screendump shot.ppm}}"
if [ -n "$MON_SCRIPT" ]; then
  ( python3 - "$MON" "$SCRATCH" "$MON_SCRIPT" <<'PYEOF'
import socket, sys, time, os
sock, scratch, script = sys.argv[1], sys.argv[2], sys.argv[3]
for _ in range(100):
    if os.path.exists(sock): break
    time.sleep(0.2)
s = socket.socket(socket.AF_UNIX); s.connect(sock); time.sleep(0.3); s.recv(4096)
t0 = time.time()
for item in script.split(";"):
    when, cmd = item.split(":", 1)
    time.sleep(max(0, float(when) - (time.time() - t0)))
    parts = cmd.split()
    if parts[0] == "screendump": parts[1] = os.path.join(scratch, parts[1])
    s.send((" ".join(parts) + "\n").encode()); time.sleep(0.6)
    try: s.recv(4096)
    except Exception: pass
PYEOF
  ) &
fi
# ISO=1: boot a private GRUB ISO (kernel + this test ramdisk) so QEMU gets a real VBE
# framebuffer; the default -kernel boot stays in VGA text mode (gfx_open then declines).
BOOTARGS=(-kernel kernel.elf -initrd "$SCRATCH/test_initrd.img")
if [ -n "${ISO:-}" ]; then
  ISODIR="$SCRATCH/iso_test"; rm -rf "$ISODIR"; mkdir -p "$ISODIR/boot/grub"
  cp kernel.elf "$ISODIR/boot/kernel.elf"; cp "$SCRATCH/test_initrd.img" "$ISODIR/boot/initrd.img"
  cp grub.cfg "$ISODIR/boot/grub/grub.cfg"
  grub-mkrescue -o "$SCRATCH/test.iso" "$ISODIR" >/dev/null 2>&1 || { echo "grub-mkrescue failed"; exit 1; }
  BOOTARGS=(-cdrom "$SCRATCH/test.iso" -boot d,menu=off)
fi
(sleep ${TYPE_AT:-8}; printf 'hqtgui\r'; sleep ${BOOT_WAIT:-25}) | timeout $((${BOOT_WAIT:-25}+${TYPE_AT:-8}+12)) qemu-system-i386 "${BOOTARGS[@]}" \
  -m 512M -no-reboot -display none -serial stdio ${QEMU_EXTRA:-} \
  -monitor unix:"$MON",server,nowait > "$SCRATCH/boot_$APP.log" 2>&1
wait
tr -d '\r' < "$SCRATCH/boot_$APP.log" | sed -n '/^hqtgui/,$p' | grep -v "nfs" | head -${LINES_SHOWN:-30} | cut -c1-220
