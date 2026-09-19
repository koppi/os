#!/usr/bin/env bash
# Build a FAT16 image staged with the userland (shell, apps, cc toolchain).
# Uses mtools, so no root / loop device needed.
#
#   ./hda.sh                    -> hda.img, 16 MiB  (persistent scratch disk)
#   IMG=initrd.img ./hda.sh     -> the boot RAM disk packed into os.iso
#
# 16 MiB gives headroom for the self-hosting C compiler (cc), its source and
# runtime, and a couple of generations of compiler output. One sector per
# cluster and -F 16 keep it firmly FAT16, which is all the in-kernel driver does.
#
# The Doom IWAD is staged only if you put one in the tree (see apps/doom/
# PORTING.md); it is several MiB, so the image grows to fit rather than
# carrying that much empty space around on every build.
set -e

WAD=${WAD:-doom1.wad}

IMG=${IMG:-hda.img}
if [ -z "${SIZE:-}" ]; then
    # hda.img is the persistent scratch disk and is always 16 MiB; the RAM
    # disk only needs that much when it is carrying a WAD.
    if [ "$IMG" = "hda.img" ] || [ -f "$WAD" ]; then
        SIZE=16M
    else
        SIZE=8M
    fi
fi

qemu-img create -f raw "$IMG" "$SIZE"
/sbin/mkfs.fat -F 16 -s 1 -R 1 "$IMG"

mcopy -i "$IMG" -D o apps/zsh/zsh         ::zsh
mcopy -i "$IMG" -D o apps/zsh/zshrc       ::zshrc
mcopy -i "$IMG" -D o apps/hello/hello     ::hello
mcopy -i "$IMG" -D o apps/01/01           ::tst
mcopy -i "$IMG" -D o apps/example/example ::example
mcopy -i "$IMG" -D o apps/mem/mem         ::mem
mcopy -i "$IMG" -D o apps/fault/fault       ::fault
mcopy -i "$IMG" -D o apps/lua/lua         ::lua
mcopy -i "$IMG" -D o apps/lua/test.lua    ::t.lua
mcopy -i "$IMG" -D o apps/lua/mod.lua     ::mod.lua
mcopy -i "$IMG" -D o mouse.bmp            ::mouse.bmp

# Doom. The engine is always staged; the IWAD only if one is present, since
# it is not ours to redistribute (apps/doom/PORTING.md says where to get one).
mcopy -i "$IMG" -D o apps/doom/doom       ::doom
if [ -f "$WAD" ]; then
    mcopy -i "$IMG" -D o "$WAD"           ::doom1.wad
fi

# The C compiler: the binary, its own source, its runtime library, and tests.
mcopy -i "$IMG" -D o apps/cc/cc           ::cc
mcopy -i "$IMG" -D o apps/cc/cc.c         ::cc.c
mcopy -i "$IMG" -D o apps/cc/prelude.c    ::prelude.c
for t in apps/cc/tests/*.c; do
    mcopy -i "$IMG" -D o "$t" "::$(basename "$t")"
done

mdir -i "$IMG" ::/
