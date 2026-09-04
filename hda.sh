#!/usr/bin/env bash
# Build the IDE hard-disk image. Uses mtools, so no root / loop device needed.
set -e

IMG=hda.img

qemu-img create -f raw "$IMG" 5M
# -s 1: one sector per cluster, which is all the in-kernel FAT driver supports.
/sbin/mkfs.fat -s 1 -R 1 "$IMG"

mcopy -i "$IMG" -D o apps/hello/hello     ::hello
mcopy -i "$IMG" -D o apps/01/01           ::tst
mcopy -i "$IMG" -D o apps/example/example ::example
mcopy -i "$IMG" -D o apps/mem/mem         ::mem
mcopy -i "$IMG" -D o apps/fault/fault       ::fault
mcopy -i "$IMG" -D o apps/lua/lua         ::lua
mcopy -i "$IMG" -D o apps/lua/test.lua    ::t.lua
mcopy -i "$IMG" -D o apps/lua/mod.lua     ::mod.lua
mcopy -i "$IMG" -D o mouse.bmp            ::mouse.bmp

mdir -i "$IMG" ::/
