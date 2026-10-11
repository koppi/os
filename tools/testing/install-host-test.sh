#!/bin/bash
# install-host-test.sh -- the installer's engine (apps/install/engine.c) on image files.
#
#   tools/testing/install-host-test.sh [bios|uefi|all]            (default: all)
#
# Builds the engine with the host compiler (apps/install/host.c), installs into scratch disk
# images of several sizes with payload.img and initrd.img from the tree, and judges the
# result with the tools that did not write it: sfdisk for the GPT, fsck.fat for the volume,
# cmp for the files, and QEMU -- SeaBIOS for the BIOS path, OVMF for UEFI -- to boot the disk.
# No kernel support needed, so it is the quick check after touching the engine.
#
# Needs payload.img and initrd.img (make iso), gcc, sfdisk, fsck.fat, mtools, qemu, socat.
set -u
cd "$(dirname "$0")/../.."
OUT=${OUT:-/tmp/install-host-test}
mkdir -p "$OUT"
KVM=${KVM:--enable-kvm}
BOOT_WAIT=${BOOT_WAIT:-18}
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd

for f in payload.img initrd.img kernel.elf; do
    [ -f "$f" ] || { echo "install-host-test: no $f -- run 'make iso' first"; exit 1; }
done
for t in gcc sfdisk fsck.fat mcopy qemu-system-i386 qemu-system-x86_64 socat; do
    command -v "$t" >/dev/null || { echo "install-host-test: need $t"; exit 1; }
done

pass=0; fail=0
ok()  { echo "  ok    $*"; pass=$((pass + 1)); }
bad() { echo "  FAIL  $*"; fail=$((fail + 1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1"; fi; }

HOST="$OUT/inst-host"
gcc -O2 -Wall -Wextra -Werror -std=gnu11 -o "$HOST" apps/install/host.c apps/install/engine.c -Iapps/install \
    || { echo "install-host-test: the engine does not build"; exit 1; }

# install_into <name> <disk MiB> [volume MiB]
install_into() {
    local name=$1 mib=$2 vol=${3:-}
    local img="$OUT/$name.img"
    rm -f "$img"; truncate -s "${mib}M" "$img"
    "$HOST" "$img" payload.img initrd.img $vol >"$OUT/$name.log" 2>&1
    check "$name: the engine finishes" "tail -1 '$OUT/$name.log' | grep -q 'done'"

    local table; table=$(sfdisk -d "$img" 2>&1)
    check "$name: GPT with the BIOS boot partition and the system volume" \
        "echo \"\$table\" | grep -q 'type=21686148-6449-6E6F-744E-656564454649' && echo \"\$table\" | grep -q 'type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B'"
    sfdisk --verify "$img" >"$OUT/$name.verify" 2>&1
    check "$name: sfdisk --verify" "grep -q 'No errors' '$OUT/$name.verify'"

    local start size
    start=$(echo "$table" | sed -n 's/^.*2 : start= *\([0-9]*\),.*$/\1/p' | head -1)
    size=$(echo "$table" | sed -n 's/^.*2 : start= *[0-9]*, size= *\([0-9]*\),.*$/\1/p' | head -1)
    dd if="$img" of="$OUT/$name.p2" bs=512 skip="$start" count="$size" status=none
    fsck.fat -n "$OUT/$name.p2" >"$OUT/$name.fsck" 2>&1
    check "$name: fsck.fat" "! grep -qi 'error\|differ\|invalid\|dirty\|lost' '$OUT/$name.fsck' && grep -q 'files, ' '$OUT/$name.fsck'"
    local off=$((start * 512))
    mcopy -i "$img@@$off" ::/boot/kernel.elf "$OUT/$name.kernel" 2>/dev/null
    mcopy -i "$img@@$off" ::/boot/initrd.img "$OUT/$name.initrd" 2>/dev/null
    check "$name: kernel.elf is byte for byte the kernel" "cmp -s '$OUT/$name.kernel' kernel.elf"
    check "$name: initrd.img is byte for byte the RAM disk" "cmp -s '$OUT/$name.initrd' initrd.img"
    rm -f "$OUT/$name.p2" "$OUT/$name.kernel" "$OUT/$name.initrd"
}

# boot_disk <name> <bios|uefi>
boot_disk() {
    local name=$1 fw=$2 img="$OUT/$1.img"
    local m=(qemu-system-i386) extra=()
    if [ "$fw" = uefi ]; then
        cp -f "$OVMF_VARS" "$OUT/vars.fd"
        m=(qemu-system-x86_64 -machine q35)
        extra=(-drive if=pflash,format=raw,unit=0,readonly=on,file="$OVMF_CODE" -drive if=pflash,format=raw,unit=1,file="$OUT/vars.fd")
    fi
    rm -f "$OUT/$name.$fw.ser" "$OUT/$name.$fw.mon"
    "${m[@]}" "${extra[@]}" -m 1G -smp 2 -no-reboot $KVM -vga std \
        -drive file="$img",format=raw,if=ide,index=0 \
        -serial "file:$OUT/$name.$fw.ser" -display none -monitor "unix:$OUT/$name.$fw.mon,server,nowait" &
    local q=$!
    sleep "$BOOT_WAIT"
    echo quit | socat - "unix-connect:$OUT/$name.$fw.mon" >/dev/null 2>&1
    kill $q 2>/dev/null; wait $q 2>/dev/null
    check "$name: $fw boots the kernel from it" "grep -q 'initrd: .*RAM disk mounted as rd' '$OUT/$name.$fw.ser'"
}

what=${1:-all}
echo "--- 512 MiB disk, whole-disk volume"
install_into d512 512
echo "--- 4 GiB disk (the volume stops at FAT16's 2 GiB)"
install_into d4g 4096
echo "--- 256 MiB disk, 64 MiB volume"
install_into d64 256 64
echo "--- too small a disk must be refused, not half-written"
rm -f "$OUT/tiny.img"; truncate -s 40M "$OUT/tiny.img"
"$HOST" "$OUT/tiny.img" payload.img initrd.img >"$OUT/tiny.log" 2>&1
check "40 MiB disk: refused" "grep -q 'too small' '$OUT/tiny.log'"
check "40 MiB disk: nothing written" "[ -z \"\$(tr -d '\\\\0' < '$OUT/tiny.img' | head -c 16)\" ]"

if [ "$what" = bios ] || [ "$what" = all ]; then
    echo "--- boot under SeaBIOS"
    boot_disk d512 bios
    boot_disk d4g bios
fi
if [ "$what" = uefi ] || [ "$what" = all ]; then
    echo "--- boot under OVMF (UEFI)"
    boot_disk d512 uefi
fi

echo
echo "install-host-test: $pass passed, $fail failed  (images and logs in $OUT)"
[ "$fail" -eq 0 ]
