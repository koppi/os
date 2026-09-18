#!/bin/bash
# t470s-boot.sh — boot os.iso in QEMU configs that approximate a ThinkPad T470s
# (q35 chipset, an NVMe M.2 SSD, xHCI USB, e1000e NIC, Intel HD Audio), under
# both SeaBIOS and OVMF (UEFI). Captures the serial log and a framebuffer
# screenshot for each.
#
#   test/t470s-boot.sh [uefi|q35|all]     (default: all)
#   SHOT_DELAY=12   seconds to wait before the screenshot
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/t470s-boot}
mkdir -p "$OUT"
SHOT_DELAY=${SHOT_DELAY:-14}
RUN_SECS=${RUN_SECS:-22}
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS_SRC=/usr/share/OVMF/OVMF_VARS_4M.fd
KVM=${KVM:--enable-kvm}

# VGA=std : legacy VBE (GRUB picks an 800x600x24-ish mode); exercises the
#           24-bpp shadow-surface path.
# VGA=virtio / qxl : closer to a real 32-bpp GOP framebuffer.
VGA=${VGA:-std}

# A 16 MiB FAT16 image on the NVMe namespace so the mount + read/write path is
# exercised (the in-kernel FAT driver is one-sector-per-cluster, so keep it
# small enough to stay FAT16 with -s 1).
if [ ! -f "$OUT/nvme.img" ]; then
    qemu-img create -f raw "$OUT/nvme.img" 16M >/dev/null
    command -v mkfs.fat >/dev/null && mkfs.fat -F 16 -s 1 -n NVMEDISK "$OUT/nvme.img" >/dev/null
fi

COMMON=(-m 3G -smp 4 -no-reboot $KVM -vga "$VGA"
        -device qemu-xhci,id=xhci -device usb-kbd -device usb-tablet
        -netdev user,id=n0 -device e1000e,netdev=n0
        -audiodev none,id=snd0
        -device intel-hda -device hda-output,audiodev=snd0
        -drive id=nvm,file="$OUT/nvme.img",format=raw,if=none
        -device nvme,drive=nvm,serial=T470SNVME)

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
    echo "--- $name serial (nvme / storage lines) ---"
    sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$OUT/$name.serial" \
        | grep -iE 'nvme|ahci|xhci|e1000|hda:|hd[a-z]:|not mounted|panic' | tail -20
    echo
}

do_uefi() {
    cp -f "$OVMF_VARS_SRC" "$OUT/uefi-vars.fd"
    run uefi qemu-system-x86_64 -machine q35 "${COMMON[@]}" \
        -drive if=pflash,format=raw,unit=0,readonly=on,file="$OVMF_CODE" \
        -drive if=pflash,format=raw,unit=1,file="$OUT/uefi-vars.fd" \
        -drive file=os.iso,if=none,id=cd0,media=cdrom,format=raw \
        -device ide-cd,drive=cd0 -boot d
}
do_q35() {
    run q35 qemu-system-x86_64 -machine q35 "${COMMON[@]}" \
        -cdrom os.iso -boot d
}

case "${1:-all}" in
    uefi) do_uefi ;;
    q35)  do_q35 ;;
    all)  do_uefi; do_q35 ;;
esac
echo "screenshots + logs in $OUT/"
