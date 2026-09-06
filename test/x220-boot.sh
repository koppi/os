#!/bin/bash
# x220-boot.sh — boot the kernel in QEMU configs that approximate a ThinkPad
# X220 (Sandy Bridge / Cougar Point: a BIOS/CSM machine, AHCI SATA, EHCI USB —
# no xHCI — and an Intel 82579LM NIC). Boots via `-kernel` with `ehci` on the
# command line so the opt-in USB 2.0 driver is exercised. Captures a serial log
# and a framebuffer screenshot for each.
#
#   test/x220-boot.sh [bios|q35|all]      (default: all)
#   SHOT_DELAY=12   seconds to wait before the screenshot
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/x220-boot}
mkdir -p "$OUT"
SHOT_DELAY=${SHOT_DELAY:-14}
RUN_SECS=${RUN_SECS:-22}
KVM=${KVM:--enable-kvm}

# VGA=std : legacy VBE, GRUB picks an 800x600x24-ish mode — the X220 boots CSM
#           and its VBIOS hands over a 24-bpp linear framebuffer.
VGA=${VGA:-std}

# A 256 MiB FAT16 image on the AHCI port so the mount + FAT path is exercised.
if [ ! -f "$OUT/sata.img" ]; then
    qemu-img create -f raw "$OUT/sata.img" 256M >/dev/null
    command -v mkfs.fat >/dev/null && mkfs.fat -F 16 -n X220SATA "$OUT/sata.img" >/dev/null
fi

# EHCI + a keyboard and a tablet on the root ports. QEMU's usb-kbd / usb-tablet
# enumerate at high speed, so this exercises root-port reset, control transfers
# on the async schedule and interrupt-IN on the periodic schedule. The hub +
# split-transaction path (a real X220 reaches its full-speed HID devices through
# the PCH Rate-Matching Hub) has no QEMU equivalent and is real-hardware only.
USB=(-device usb-ehci,id=ehci
     -device usb-kbd,bus=ehci.0,port=1
     -device usb-tablet,bus=ehci.0,port=2)

STORAGE=(-drive id=sata,file="$OUT/sata.img",format=raw,if=none
         -device ich9-ahci,id=ahci -device ide-hd,drive=sata,bus=ahci.0)

run() {
    local name=$1; shift
    local mon="$OUT/$name.mon"
    rm -f "$mon"
    echo "=== $name ==="
    ( "$@" -serial "file:$OUT/$name.serial" \
           -display none -monitor "unix:$mon,server,nowait" ) &
    local qpid=$!
    sleep "$SHOT_DELAY"
    if [ -S "$mon" ]; then
        echo "screendump $OUT/$name.ppm" | socat - "unix-connect:$mon" >/dev/null 2>&1
        [ -f "$OUT/$name.ppm" ] && command -v pnmtopng >/dev/null 2>&1 \
            && pnmtopng "$OUT/$name.ppm" > "$OUT/$name.png" 2>/dev/null
    fi
    sleep $(( RUN_SECS - SHOT_DELAY ))
    echo "quit" | socat - "unix-connect:$mon" >/dev/null 2>&1
    kill $qpid 2>/dev/null; wait $qpid 2>/dev/null
    echo "--- $name serial (usb / storage / net lines) ---"
    sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$OUT/$name.serial" \
        | grep -iE 'ehci|uhci|xhci|usb|hid|ahci|hd[a-z]:|e1000|82579|panic|not mounted' | tail -24
    echo
}

KERN=(-kernel kernel.elf -initrd initrd.img -append ehci)

do_bios() {
    run bios qemu-system-i386 -machine pc -m 3G -smp 4 -no-reboot $KVM -vga "$VGA" \
        "${KERN[@]}" "${USB[@]}" "${STORAGE[@]}" \
        -netdev user,id=n0 -device e1000,netdev=n0 \
        -audiodev none,id=snd0 -device intel-hda -device hda-output,audiodev=snd0
}
do_q35() {
    run q35 qemu-system-x86_64 -machine q35 -m 3G -smp 4 -no-reboot $KVM -vga "$VGA" \
        "${KERN[@]}" "${USB[@]}" "${STORAGE[@]}" \
        -netdev user,id=n0 -device e1000e,netdev=n0 \
        -audiodev none,id=snd0 -device intel-hda -device hda-output,audiodev=snd0
}

case "${1:-all}" in
    bios) do_bios ;;
    q35)  do_q35 ;;
    all)  do_bios; do_q35 ;;
esac
echo "screenshots + logs in $OUT/"
