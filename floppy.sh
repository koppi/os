#!/usr/bin/env bash
# Build the floppy image (drive A). Uses mtools, so no root / loop device needed.
set -e

IMG=floppy.img

# -C 1440: create a standard 1.44M FAT12 floppy (1 sector per cluster), which
# is the geometry the in-kernel FAT driver was written against.
rm -f "$IMG"
/sbin/mkfs.fat -C "$IMG" 1440

mcopy -i "$IMG" -D o apps/hello/hello   ::hello
mcopy -i "$IMG" -D o apps/01/01         ::tst
mcopy -i "$IMG" -D o apps/example/example ::example
mcopy -i "$IMG" -D o apps/mem/mem       ::mem
mcopy -i "$IMG" -D o apps/lua/lua       ::lua
mcopy -i "$IMG" -D o apps/lua/test.lua  ::t.lua
mcopy -i "$IMG" -D o apps/lua/mod.lua   ::mod.lua
mcopy -i "$IMG" -D o mouse.bmp          ::mouse.bmp

mdir -i "$IMG" ::/
