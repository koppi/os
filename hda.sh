#!/usr/bin/env bash
# Build the IDE hard-disk image. Uses mtools, so no root / loop device needed.
set -e

IMG=hda.img

qemu-img create -f raw "$IMG" 5M
# -s 1: one sector per cluster, which is all the in-kernel FAT driver supports.
/sbin/mkfs.fat -s 1 -R 1 "$IMG"

mcopy -i "$IMG" -D o apps/hello/hello ::hello
mcopy -i "$IMG" -D o apps/01/01     ::tst

mdir -i "$IMG" ::/
