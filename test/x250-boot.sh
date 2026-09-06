#!/bin/bash
# x250-boot.sh — boot os.iso in QEMU configs that approximate a ThinkPad X250
# (q35 chipset, AHCI SATA, xHCI USB, e1000e NIC), under both SeaBIOS and OVMF
# (UEFI). Captures the serial log and a framebuffer screenshot for each.
#
#   test/x250-boot.sh [uefi|q35|i440|all]   (default: all)
#   SHOT_DELAY=12  seconds to wait before the screenshot
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/x250-boot}
mkdir -p "$OUT"
SHOT_DELAY=${SHOT_DELAY:-12}
RUN_SECS=${RUN_SECS:-20}
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS_SRC=/usr/share/OVMF/OVMF_VARS_4M.fd
KVM=${KVM:--enable-kvm}

COMMON=(-m 3G -smp 4 -no-reboot $KVM
        -device qemu-xhci,id=xhci -device usb-kbd -device usb-tablet
        -netdev user,id=n0 -device e1000e,netdev=n0
        -drive id=sata,file="$OUT/sata.img",format=raw,if=none
        -device ich9-ahci,id=ahci -device ide-hd,drive=sata,bus=ahci.0)

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
    run uefi qemu-system-x86_64 -machine q35 "${COMMON[@]}" \
        -drive if=pflash,format=raw,unit=0,readonly=on,file="$OVMF_CODE" \
        -drive if=pflash,format=raw,unit=1,file="$OUT/uefi-vars.fd" \
        -drive file=os.iso,if=none,id=cd0,media=cdrom,format=raw \
        -device ide-cd,drive=cd0,bus=ahci.1 -boot d
}
do_q35() {
    run q35 qemu-system-x86_64 -machine q35 "${COMMON[@]}" \
        -drive file=os.iso,if=none,id=cd0,media=cdrom,format=raw \
        -device ide-cd,drive=cd0,bus=ahci.1 -boot d
}
do_i440() {
    run i440 qemu-system-i386 -machine pc -m 256M -smp 4 -no-reboot $KVM \
        -cdrom os.iso -boot d
}

case "${1:-all}" in
    uefi) do_uefi ;;
    q35)  do_q35 ;;
    i440) do_i440 ;;
    all)  do_uefi; do_q35; do_i440 ;;
esac
echo "screenshots + logs in $OUT/"
