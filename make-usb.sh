#!/bin/bash
# make-usb.sh — build a bootable UEFI USB image (os-usb.img) for a MacBook
# Air 2013 / any laptop that is UEFI-only (no CSM): GPT + FAT32 EFI System
# Partition holding a compiled grub2 x86_64-efi BOOTX64.EFI, the kernel, the
# initrd RAM disk and grub.cfg.
#
#   ./make-usb.sh            build os-usb.img in this directory
#   ./make-usb.sh /dev/sdX   (after umount) write it to a real USB stick
#   OUT=... SIZE=... ./make-usb.sh
#
# The MacBook's Apple EFI will not run a hybrid-MBR/USB-HDD stick reliably, so
# we use a pure GPT image. dd'ing the .img (or letting the script dd it) works
# with any NAND stick; no partitioning tool is needed on the target -- but the
# long GPT dummy (sector 1) means sdX must be the whole stick, not a partition.
set -eu

cd "$(dirname "$0")"
KERNEL=${KERNEL:-kernel.elf}
INITRD=${INITRD:-initrd.img}
CFG=${CFG:-grub.cfg}
OUT=${OUT:-os-usb.img}
SIZE=${SIZE:-128M}
GRUB_PREFIX=/boot/grub

[ -x "$KERNEL" ]      || { echo "make-usb: $KERNEL missing (run make)"   >&2; exit 1; }
[ -f "$INITRD" ]      || { echo "make-usb: $INITRD missing (run make)"   >&2; exit 1; }
[ -f "$CFG" ]         || { echo "make-usb: $CFG missing"                 >&2; exit 1; }
command -v grub-mkimage >/dev/null || { echo "make-usb: grub-mkimage missing" >&2; exit 1; }
command -v sfdisk >/dev/null       || { echo "make-usb: sfdisk missing"       >&2; exit 1; }
command -v mformat >/dev/null      || { echo "make-usb: mformat missing"      >&2; exit 1; }
command -v mcopy >/dev/null        || { echo "make-usb: mcopy missing"        >&2; exit 1; }
FONT=${FONT:-/usr/share/grub/unicode.pf2}
[ -f "$FONT" ]        || { echo "make-usb: font $FONT missing"            >&2; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# --- GRUB image (BOOTX64.EFI), configured the same way as the ISO boot dir ---
# emb.cfg sets prefix=/boot/grub so modules and grub.cfg load from the ESP.
cat > "$WORK/emb.cfg" <<EOF
set prefix=$GRUB_PREFIX
normal
EOF
grub-mkimage -O x86_64-efi -o "$WORK/BOOTX64.EFI" -p "$GRUB_PREFIX" \
    -c "$WORK/emb.cfg" \
    normal loadenv configfile gfxterm gfxmenu gfxterm_background \
    all_video efi_gop font videoinfo \
    part_gpt part_msdos fat \
    search search_fs_file boot \
    multiboot multiboot2 minicmd ls cat test sleep

# --- ESP payload ---
rm -f "$WORK/kernel.elf"; cp "$KERNEL" "$WORK/kernel.elf"
rm -f "$WORK/initrd.img"; cp "$INITRD" "$WORK/initrd.img"
rm -f "$WORK/grub.cfg";   cp "$CFG"    "$WORK/grub.cfg"
mkdir -p "$WORK/fonts";   cp "$FONT"   "$WORK/fonts/unicode.pf2"

# --- GPT disk image with a single FAT32 ESP, flags boot+esp ---
rm -f "$OUT"
qemu-img create -f raw "$OUT" "$SIZE" >/dev/null 2>&1 \
    || dd if=/dev/zero of="$OUT" bs=1M count=1 seek=$(( $(echo "$SIZE" | tr -dc 0-9) )) status=none

printf 'label: gpt\nfirst-lba: 2048\n%s1 : start=2048, size=+, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B, bootable\n' "$OUT" \
    | sfdisk "$OUT" >/dev/null

# FAT32 ESP at 1 MiB (sector 2048), populated via mtools' image offset syntax.
OFF=1048576          # 2048 * 512
mformat -F -i "$OUT"@@$OFF :: >/dev/null
mmd -i "$OUT"@@$OFF ::/EFI
mmd -i "$OUT"@@$OFF ::/EFI/BOOT
mmd -i "$OUT"@@$OFF ::/boot
mmd -i "$OUT"@@$OFF ::/boot/grub
mcopy -i "$OUT"@@$OFF "$WORK/BOOTX64.EFI" ::/EFI/BOOT/
mcopy -i "$OUT"@@$OFF "$WORK/kernel.elf"  ::/boot/
mcopy -i "$OUT"@@$OFF "$WORK/initrd.img"  ::/boot/
mcopy -i "$OUT"@@$OFF "$WORK/grub.cfg"    ::/boot/grub/
mcopy -o -i "$OUT"@@$OFF "$WORK/fonts"    ::/boot/grub/

echo "make-usb: wrote $OUT ($(du -h "$OUT" | cut -f1))"
if [ "${1:-}" ]; then
    [ -b "$1" ] || { echo "make-usb: $1 is not a block device" >&2; exit 1; }
    echo "make-usb: dd'ing to $1 (this overwrites the whole stick!)"
    dd if="$OUT" of="$1" bs=1M status=progress
    sync
    echo "make-usb: done — boot the MacBook from USB:"
    echo "           power on and hold Option to pick the EFI boot icon."
fi