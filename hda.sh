#!/usr/bin/env bash
# Build the IDE hard-disk image. Uses mtools, so no root / loop device needed.
set -e

IMG=hda.img

# 16 MiB: enough headroom for the self-hosting C compiler (cc), its source and
# runtime, and a couple of generations of compiler output. Still one sector per
# cluster (~32k clusters => FAT16), which is all the in-kernel FAT driver does.
qemu-img create -f raw "$IMG" 16M
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

# The C compiler: the binary, its own source, its runtime library, and tests.
mcopy -i "$IMG" -D o apps/cc/cc           ::cc
mcopy -i "$IMG" -D o apps/cc/cc.c         ::cc.c
mcopy -i "$IMG" -D o apps/cc/prelude.c    ::prelude.c
for t in apps/cc/tests/*.c; do
    mcopy -i "$IMG" -D o "$t" "::$(basename "$t")"
done

mdir -i "$IMG" ::/
