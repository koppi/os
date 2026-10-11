#!/usr/bin/env bash
# Build a FAT16 image staged with the userland (shell, apps, cc toolchain).
# Uses mtools, so no root / loop device needed.
#
#   scripts/hda.sh                    -> hda.img, 16 MiB  (persistent scratch disk)
#   IMG=initrd.img scripts/hda.sh     -> the boot RAM disk packed into os.iso
#
# 16 MiB gives headroom for the self-hosting C compiler (cc), its source and
# runtime, and a couple of generations of compiler output. One sector per
# cluster and -F 16 keep it firmly FAT16, which is all the in-kernel driver does.
#
# The Doom IWAD is staged only if you put one in the tree (see apps/doom/
# PORTING.md); it is several MiB, so the image grows to fit rather than
# carrying that much empty space around on every build.
set -e

# Everything below is named relative to the top of the tree.
cd "$(dirname "$0")/.."

WAD=${WAD:-doom1.wad}

# The RAM disk is FAT16 with one-sector clusters, so it cannot be bigger than 32 MiB. The big optional files (the
# Doom WAD and the three Qt programs, each with a Qt of its own) all fit, with about 3 MiB to spare; if you add
# more, or want the room, leave out what a boot does not need:
#   HDA_SKIP="hqtgui hqtwid" make iso     (names: doom1.wad hqtgui hqtwid qtlua)
skip() {
    case " ${HDA_SKIP:-} " in *" $1 "*) return 0 ;; esac
    return 1
}

IMG=${IMG:-hda.img}
if [ -z "${SIZE:-}" ]; then
    # hda.img is the persistent scratch disk and is always 16 MiB. The RAM disk
    # is 8 MiB for the base userland and grows to carry what is optional and big
    # (a Doom WAD, the Qt demos), plus 1 MiB of FAT slack, never below 16 MiB
    # then. One-sector clusters cap FAT16 at ~32 MiB, which is the ceiling.
    if [ "$IMG" = "hda.img" ]; then
        SIZE=16M
    else
        extra=0
        for f in "$WAD" apps/hello-qt-gui/hqtgui apps/hello-qt-widgets/hqtwid apps/qt-lua/qtlua; do
            name=$(basename "$f"); [ "$f" = "$WAD" ] && name=doom1.wad
            skip "$name" && continue
            [ -f "$f" ] && extra=$((extra + $(stat -c %s "$f")))
        done
        if [ "$extra" -eq 0 ]; then
            SIZE=8M
        else
            extra_mib=$(( (extra + 1048575) / 1048576 ))
            mib=$((8 + extra_mib + 1))
            [ "$mib" -lt 16 ] && mib=16
            # 8 MiB is the roomy guess for the base userland (about 3 MiB of it is files, the rest is space to
            # write to); above the 32 MiB ceiling what has to fit is the files themselves plus a little slack
            if [ "$mib" -gt 32 ]; then
                if [ $((4 + extra_mib + 1)) -gt 32 ]; then
                    echo "hda.sh: the optional files need ${extra_mib} MiB and FAT16 with 512-byte clusters stops at 32 MiB. Leave some out: HDA_SKIP=\"hqtgui hqtwid\" (doom1.wad hqtgui hqtwid qtlua)" >&2
                fi
                mib=32
            fi
            SIZE=${mib}M
        fi
    fi
fi

qemu-img create -f raw "$IMG" "$SIZE"
/sbin/mkfs.fat -F 16 -s 1 -R 1 "$IMG"

mcopy -i "$IMG" -D o apps/zsh/zsh         ::zsh
mcopy -i "$IMG" -D o apps/zsh/zshrc       ::zshrc
mcopy -i "$IMG" -D o apps/hello/hello     ::hello
mcopy -i "$IMG" -D o apps/hello-cpp/hellocpp ::hellocpp
mcopy -i "$IMG" -D o apps/hello-stl/hellostl ::hellostl
mcopy -i "$IMG" -D o apps/hello-str/hellostr ::hellostr
mcopy -i "$IMG" -D o apps/hello-map/hellomap ::hellomap
mcopy -i "$IMG" -D o apps/hello-umap/helloump ::helloump
mcopy -i "$IMG" -D o apps/hello-set/helloset ::helloset
mcopy -i "$IMG" -D o apps/hello-thread/hellothr ::hellothr
mcopy -i "$IMG" -D o apps/hello-pthread/hellopth ::hellopth
mcopy -i "$IMG" -D o apps/hello-theap/thrheap ::thrheap
mcopy -i "$IMG" -D o apps/hello-tls/tlsthr ::tlsthr
mcopy -i "$IMG" -D o apps/hello-qt/helloqt        ::helloqt
mcopy -i "$IMG" -D o apps/01/01           ::tst
mcopy -i "$IMG" -D o apps/example/example ::example
mcopy -i "$IMG" -D o apps/mem/mem         ::mem
mcopy -i "$IMG" -D o apps/fault/fault       ::fault
mcopy -i "$IMG" -D o apps/lua/lua         ::lua
mcopy -i "$IMG" -D o apps/lua/test.lua    ::t.lua
mcopy -i "$IMG" -D o apps/lua/mod.lua     ::mod.lua
mcopy -i "$IMG" -D o drivers/video/mouse.bmp ::mouse.bmp

# The microui apps (apps/microui is the shared ring-3 runtime, not a program).
mcopy -i "$IMG" -D o apps/calc/calc       ::calc
mcopy -i "$IMG" -D o apps/clock/clock     ::clock

# ChipNomad. The tracker plus the bundled content it is useless without: a
# colour theme, the AY instrument presets, the pitch tables and two of
# upstream's demo songs. Names are 8.3 because the FAT driver is (see
# apps/chipnomad/koppios/file_system_koppios.cpp); the originals' longer
# names are in apps/chipnomad/README.md.
CNDATA=third_party/chipnomad/tracker/packaging/common
mcopy -i "$IMG" -D o apps/chipnomad/chipnomad ::cnomad
mcopy -i "$IMG" -D o "$CNDATA/themes/Default.cth"            ::default.cth
mcopy -i "$IMG" -D o "$CNDATA/projects/MICROEGGZ.cnm"         ::microegg.cnm
mcopy -i "$IMG" -D o "$CNDATA/projects/WB7.cnm"               ::wb7.cnm
mcopy -i "$IMG" -D o "$CNDATA/projects/ModAndTimerDemos.cnm"  ::modtimer.cnm
mcopy -i "$IMG" -D o "$CNDATA/pitch-tables/PT3-0.csv"         ::pt3-0.csv
mcopy -i "$IMG" -D o "$CNDATA/pitch-tables/PT3-1.csv"         ::pt3-1.csv
mcopy -i "$IMG" -D o "$CNDATA/pitch-tables/PT3-2.csv"         ::pt3-2.csv
mcopy -i "$IMG" -D o "$CNDATA/pitch-tables/PT3-3.csv"         ::pt3-3.csv
mcopy -i "$IMG" -D o "$CNDATA/wavetables/FIFTH.aywave"        ::fifth.ayw
mcopy -i "$IMG" -D o "$CNDATA/wavetables/NESTRI.aywave"       ::nestri.ayw
mcopy -i "$IMG" -D o "$CNDATA/wavetables/VRC6SAW.aywave"      ::vrc6saw.ayw
mcopy -i "$IMG" -D o "$CNDATA/instruments/Bass 1.cni"         ::bass1.cni
mcopy -i "$IMG" -D o "$CNDATA/instruments/Lead 1.cni"         ::lead1.cni
mcopy -i "$IMG" -D o "$CNDATA/instruments/BD 1.cni"           ::bd1.cni
mcopy -i "$IMG" -D o "$CNDATA/instruments/Snare 1.cni"        ::snare1.cni
mcopy -i "$IMG" -D o "$CNDATA/instruments/Hat 1.cni"          ::hat1.cni
mcopy -i "$IMG" -D o "$CNDATA/instruments/Clap.cni"           ::clap.cni
mcopy -i "$IMG" -D o "$CNDATA/instruments/Pluck 1.cni"        ::pluck1.cni
mcopy -i "$IMG" -D o "$CNDATA/instruments/Waves.cni"          ::waves.cni

# Doom. The engine is always staged; the IWAD only if one is present, since
# it is not ours to redistribute (apps/doom/PORTING.md says where to get one).
mcopy -i "$IMG" -D o apps/doom/doom       ::doom
if [ -f "$WAD" ] && ! skip doom1.wad; then
    mcopy -i "$IMG" -D o "$WAD"           ::doom1.wad
fi

# Graphical Qt6 demos. apps/hello-qt-gui and apps/hello-qt-widgets are not part of
# `make -C apps` (about 560 translation units, shared); build them with
# `make -C apps/hello-qt-gui -j4` / `make -C apps/hello-qt-widgets -j4` and they are staged
# here, together with the Unifont subset they load as /rd/font.ttf.
# Needs a framebuffer boot (the ISO / real hardware), not `-kernel`.
qt_staged=
for q in hqtgui:apps/hello-qt-gui/hqtgui hqtwid:apps/hello-qt-widgets/hqtwid qtlua:apps/qt-lua/qtlua; do
    [ -f "${q#*:}" ] && ! skip "${q%%:*}" && qt_staged=1
done
if [ -n "$qt_staged" ]; then
    mcopy -i "$IMG" -D o apps/hello-qt-gui/unifont-subset.ttf  ::font.ttf
fi
if [ -f apps/hello-qt-gui/hqtgui ] && ! skip hqtgui; then
    mcopy -i "$IMG" -D o apps/hello-qt-gui/hqtgui              ::hqtgui
fi
if [ -f apps/hello-qt-widgets/hqtwid ] && ! skip hqtwid; then
    mcopy -i "$IMG" -D o apps/hello-qt-widgets/hqtwid          ::hqtwid
fi

# qtlua: Lua with the Qt6 Lua bindings of tsujikiri-qt (apps/qt-lua; needs a checkout of that repo next
# to this one, or TSUJIKIRI_QT6=/path). The scripts come from its koppios/scripts.
# Run `qtlua` for the widgets demo or `qtlua /rd/selftest.lua` for the check of the bindings.
TSUJIKIRI_QT6=${TSUJIKIRI_QT6:-$(for d in ../tsujikiri-qt6 ../tsujikiri-qt; do [ -d "$d/koppios/scripts" ] && { echo "$d"; break; }; done)}
if [ -f apps/qt-lua/qtlua ] && ! skip qtlua; then
    mcopy -i "$IMG" -D o apps/qt-lua/qtlua                     ::qtlua
    for s in qtlua selftest; do
        [ -f "$TSUJIKIRI_QT6/koppios/scripts/$s.lua" ] && mcopy -i "$IMG" -D o "$TSUJIKIRI_QT6/koppios/scripts/$s.lua" ::$s.lua
    done
fi
