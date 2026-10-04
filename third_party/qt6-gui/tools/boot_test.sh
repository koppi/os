#!/bin/bash
# Boots apps/hello-qt-gui/hqtgui under QEMU on a private GRUB ISO (so there is a
# real VBE framebuffer) and optionally drives the QEMU monitor.
#
#   boot_test.sh <work-dir> [name]
#     MON_SCRIPT="28:screendump a.ppm;31:sendkey esc;36:screendump c.ppm"  timed monitor commands
#                 (seconds after QEMU starts; .ppm files land in <work-dir>)
#     TYPE_AT=14   seconds before `hqtgui` is typed at the shell   BOOT_WAIT=26  seconds to keep running
#     NIC=none     add -nic none
# Uses the repo's kernel.elf, initrd.img and grub.cfg (run `make` first); never modifies them.
set -u
WORK="${1:?usage: boot_test.sh <work-dir> [program-name]}"; NAME="${2:-hqtgui}"
TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; REPO="$(cd "$TOOLS/../../.." && pwd)"
APP="$REPO/apps/hello-qt-gui"
mkdir -p "$WORK"; cd "$REPO"
[ -f "$APP/hqtgui" ] || { echo "build it first: make -C apps/hello-qt-gui -j4"; exit 1; }
cp initrd.img "$WORK/test_initrd.img"
mcopy -i "$WORK/test_initrd.img" -D o "$APP/hqtgui" ::hqtgui
mcopy -i "$WORK/test_initrd.img" -D o "$APP/unifont-subset.ttf" ::font.ttf
rm -rf "$WORK/iso"; mkdir -p "$WORK/iso/boot/grub"
cp kernel.elf "$WORK/iso/boot/"; cp "$WORK/test_initrd.img" "$WORK/iso/boot/initrd.img"; cp grub.cfg "$WORK/iso/boot/grub/"
grub-mkrescue -o "$WORK/test.iso" "$WORK/iso" >/dev/null 2>&1 || { echo "grub-mkrescue failed"; exit 1; }

MON="$WORK/qemu_mon.sock"; rm -f "$MON" "$WORK"/*.ppm
if [ -n "${MON_SCRIPT:-}" ]; then
  ( python3 - "$MON" "$WORK" "$MON_SCRIPT" <<'PYEOF'
import socket, sys, time, os
sock, work, script = sys.argv[1], sys.argv[2], sys.argv[3]
for _ in range(100):
    if os.path.exists(sock): break
    time.sleep(0.2)
s = socket.socket(socket.AF_UNIX); s.connect(sock); time.sleep(0.3); s.recv(4096)
t0 = time.time()
for item in script.split(";"):
    when, cmd = item.split(":", 1)
    time.sleep(max(0, float(when) - (time.time() - t0)))
    parts = cmd.split()
    if parts[0] == "screendump": parts[1] = os.path.join(work, parts[1])
    s.send((" ".join(parts) + "\n").encode()); time.sleep(0.6)
    try: s.recv(4096)
    except Exception: pass
PYEOF
  ) &
fi
EXTRA=(); [ "${NIC:-}" = none ] && EXTRA=(-nic none)
(sleep "${TYPE_AT:-14}"; printf '%s\r' "$NAME"; sleep "${BOOT_WAIT:-26}") | \
  timeout $((${BOOT_WAIT:-26}+${TYPE_AT:-14}+12)) qemu-system-i386 -cdrom "$WORK/test.iso" -boot d,menu=off \
  -m 512M -no-reboot -display none -serial stdio -monitor unix:"$MON",server,nowait "${EXTRA[@]}" \
  > "$WORK/boot.log" 2>&1
wait
tr -d '\r' < "$WORK/boot.log" | sed -n "/^$NAME/,\$p" | grep -v " nfs" | head -${LINES_SHOWN:-25}
