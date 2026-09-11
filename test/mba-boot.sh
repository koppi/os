#!/bin/bash
# mba-boot.sh — boot the USB image in QEMU configs that approximate a MacBook
# Air 2013 (Haswell-ULT, xHCI-only, GOP FB, Cirrus or QXL video).
#
#   test/mba-boot.sh [uefi|all]   (default: all)
#   SHOT_DELAY=12  seconds to wait before the screenshot
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/mba-boot}
mkdir -p "$OUT"
SHOT_DELAY=${SHOT_DELAY:-12}
RUN_SECS=${RUN_SECS:-20}
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS_SRC=/usr/share/OVMF/OVMF_VARS_4M.fd
KVM=${KVM:--enable-kvm}

# q35 + xHCI + AHCI + no PS/2, Cirrus GOP-like video.  HDMI is small by
# default; MacBook panels are 1440x900 / 1366x768 — the kernel reads the
# real width/height from the multiboot framebuffer tag.
COMMON=(-machine q35 -m 3G -smp 4 -no-reboot $KVM -vga qxl
        -device qemu-xhci,id=xhci -device usb-kbd -device usb-tablet
        -drive id=sata,file="$OUT/sata.img",format=raw,if=none
        -device ich9-ahci,id=ahci -device ide-hd,drive=sata,bus=ahci.0
        -drive file=os.iso,if=none,id=cd0,media=cdrom,format=raw
        -device ide-cd,drive=cd0,bus=ahci.1 -boot d)

if [ ! -f "$OUT/sata.img" ]; then
    qemu-img create -f raw "$OUT/sata.img" 256M >/dev/null
    command -v mkfs.fat >/dev/null && mkfs.fat -F 16 -n SATADISK "$OUT/sata.img" >/dev/null
fi

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
    echo "--- $name serial (last 12 lines) ---"
    sed 's/\x1b\[[0-9;]*[a-zA-Z]//g; s/\x1b[()][A-Z0-9]//g' "$OUT/$name.serial" | grep -v '^$' | tail -12
    echo
}

do_uefi() {
    cp -f "$OVMF_VARS_SRC" "$OUT/uefi-vars.fd"
    run uefi qemu-system-x86_64 "${COMMON[@]}" \
        -bios /usr/share/OVMF/OVMF_CODE_4M.fd \
        -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
        -drive if=pflash,format=raw,readonly=off,file="$OUT/uefi-vars.fd"
}

for arg in "$@"; do
    case $arg in
        uefi) do_uefi ;;
        all)  do_uefi ;;
        *)    echo "usage: $0 [uefi|all]" >&2; exit 1 ;;
    esac
done

# Default to "all" when no arguments given.
if [ $# -eq 0 ]; then do_uefi; fi
