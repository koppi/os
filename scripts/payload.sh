#!/usr/bin/env bash
# Build payload.img: what the installer carries to the disk besides the RAM disk.
#
#   scripts/payload.sh            -> payload.img
#
# It is a small FAT16 image (flat root, 8.3 names, upper case: the installer reads it
# with a hand-written reader, see apps/install/engine.c) holding
#
#   KERNEL.ELF    the kernel, as built
#   BOOT.IMG      GRUB's MBR boot code
#   CORE.IMG      GRUB's core image for BIOS, with arch/x86/boot/grub/embed.cfg baked in
#   BOOTX64.EFI   GRUB for UEFI, ditto -- the fallback path \EFI\BOOT\BOOTX64.EFI
#   GRUB.CFG      arch/x86/boot/grub/grub.cfg without its live-only entries
#   ASCII.PF2     the menu font (the ISO's unicode.pf2 is 2.4 MB; a menu needs ASCII)
#   VERSION       one line for the installer to show
#
# GRUB loads this next to initrd.img as a second Multiboot module, but only in the
# "Install" menu entry, so a normal boot pays nothing for it. The kernel exposes it
# to ring 3 as a read-only block device (block/blk.c). Needs grub-mkimage (with the
# i386-pc and x86_64-efi platforms), mkfs.fat and mtools -- the same set make iso uses.
set -eu

cd "$(dirname "$0")/.."
KERNEL=${KERNEL:-kernel.elf}
CFG=${CFG:-arch/x86/boot/grub/grub.cfg}
EMBED=${EMBED:-arch/x86/boot/grub/embed.cfg}
OUT=${OUT:-payload.img}
GRUB_PC=${GRUB_PC:-/usr/lib/grub/i386-pc}
FONT=${FONT:-/usr/share/grub/ascii.pf2}

for t in grub-mkimage mkfs.fat mcopy; do
    command -v "$t" >/dev/null || { echo "payload.sh: need $t" >&2; exit 1; }
done
[ -f "$KERNEL" ]            || { echo "payload.sh: $KERNEL missing (run make)" >&2; exit 1; }
[ -f "$GRUB_PC/boot.img" ]  || { echo "payload.sh: $GRUB_PC/boot.img missing (grub-pc-bin)" >&2; exit 1; }
[ -f "$FONT" ]              || { echo "payload.sh: $FONT missing" >&2; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# The modules are built in rather than loaded from /boot/grub/<platform>/ at run time,
# so the installed volume needs no module directory. What grub.cfg uses: a menu
# (normal), the framebuffer (all_video, gfxterm, font, video*), multiboot 1 and 2, and
# finding the volume (search, part_gpt, fat) -- plus a few commands for the GRUB prompt.
COMMON="normal configfile gfxterm all_video font videoinfo part_gpt part_msdos fat \
        search search_fs_file boot multiboot multiboot2 minicmd ls cat test sleep echo"

grub-mkimage -O i386-pc -o "$WORK/CORE.IMG" -p /boot/grub -c "$EMBED" biosdisk $COMMON
grub-mkimage -O x86_64-efi -o "$WORK/BOOTX64.EFI" -p /boot/grub -c "$EMBED" efi_gop $COMMON
cp "$GRUB_PC/boot.img" "$WORK/BOOT.IMG"
cp "$KERNEL" "$WORK/KERNEL.ELF"
cp "$FONT" "$WORK/ASCII.PF2"

# The installed menu: the ISO's, minus the installer (an installed system has no
# payload to install from), with the small font and a menu that stays a little longer.
sed -e '/^# @live-only/,/^# @end-live-only/d' \
    -e 's#fonts/unicode\.pf2#fonts/ascii.pf2#' \
    "$CFG" > "$WORK/GRUB.CFG"

{
    printf 'koppi-os %s' "$(git describe --tags --always --dirty 2>/dev/null || echo unknown)"
    printf ' (%s)' "$(date -u +%Y-%m-%d 2>/dev/null)"
    printf '\n'
} > "$WORK/VERSION"

# Size the image to its contents: 1-sector clusters (so the reader needs no
# cluster arithmetic worth the name), a megabyte or two of headroom, never below
# the 2 MiB a FAT16 needs to be one.
bytes=0
for f in "$WORK"/*; do bytes=$((bytes + $(stat -c %s "$f") + 512)); done
mib=$(( (bytes + 2 * 1048576 + 1048575) / 1048576 ))
[ "$mib" -lt 4 ] && mib=4

rm -f "$OUT"
dd if=/dev/zero of="$OUT" bs=1M count="$mib" status=none
mkfs.fat -F 16 -s 1 -R 1 -n KOPPIPAY "$OUT" >/dev/null
for f in KERNEL.ELF BOOT.IMG CORE.IMG BOOTX64.EFI GRUB.CFG ASCII.PF2 VERSION; do
    mcopy -i "$OUT" -D o "$WORK/$f" "::$f"
done

echo "payload.sh: wrote $OUT ($mib MiB; kernel $(stat -c %s "$KERNEL") B," \
     "core.img $(stat -c %s "$WORK/CORE.IMG") B, BOOTX64.EFI $(stat -c %s "$WORK/BOOTX64.EFI") B)"
