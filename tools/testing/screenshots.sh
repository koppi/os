#!/bin/bash
# screenshots.sh -- take the README's screenshots from the real OS, in QEMU, headless.
#
#   tools/testing/screenshots.sh [name ...]            (default: every scenario)
#   tools/testing/screenshots.sh list
#
# Each scenario boots a copy of os.iso, drives it the way the other *-boot.sh scripts do
# (the shell over the serial console, keystrokes through the monitor's `sendkey`), and
# leaves one PNG per `shot` in $DEST (Documentation/screenshots). The PNGs are palette
# images: the desktop is a handful of flat colours, and 256 are plenty for 1280x800 of it.
#
# Needs a built os.iso with the Qt demos and a Doom WAD on its RAM disk for those scenarios
# (they are skipped, with a note, if the program is not on it), qemu-system-i386, socat,
# ImageMagick (convert) and pnmtopng. The installer scenario needs `make iso` for payload.img.
set -u
cd "$(dirname "$0")/../.."
DEST=${DEST:-Documentation/screenshots}
WORK=${WORK:-/tmp/screenshots}
BOOT_WAIT=${BOOT_WAIT:-16}      # seconds from power-on until the shell answers
KVM=${KVM:--enable-kvm}
mkdir -p "$DEST" "$WORK"

for t in qemu-system-i386 socat convert; do
    command -v "$t" >/dev/null || { echo "screenshots: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "screenshots: no os.iso -- run 'make iso' first"; exit 1; }

# The checkout can be rebuilt under a long run (another terminal's make); work from a copy.
ISO="$WORK/os.iso"
cp -f os.iso "$ISO"

# A file on the RAM disk? (asked of the image the ISO was built from)
on_ramdisk() { [ -f initrd.img ] && mdir -i initrd.img "::$1" >/dev/null 2>&1; }

# No network card unless a scenario asks for one (NET=(...)): with one the kernel starts its
# NFS client, whose retry warnings land in the middle of the desktop console. QEMU adds a
# default NIC of its own unless told `-nic none`.
NET_NONE=(-nic none)
NET=("${NET_NONE[@]}")

# run <name> <iso> <extra qemu args...> -- <steps...>
#
#   cmd:<line>     type a line at the shell (serial)      sleep:<s>     wait
#   key:<qemu key> tap a key                              shot:<file>   screenshot -> $DEST/<file>.png
#   crop:<WxH+X+Y> crop the *next* shot                   wait_boot:    sleep until the shell is up
#   move:<dx>,<dy> relative pointer move (`home:` parks it at 0,0 first)   click: down: up:   the left button
run() {
    local name=$1 iso=$2; shift 2
    local qargs=()
    while [ $# -gt 0 ] && [ "$1" != "--" ]; do qargs+=("$1"); shift; done
    shift
    local d="$WORK/$name"
    rm -rf "$d"; mkdir -p "$d"
    echo "=== $name"

    qemu-system-i386 -vga virtio -m 512M -no-reboot -smp 4 $KVM -rtc base=localtime,clock=vm \
        -drive file="$iso",if=ide,index=2,media=cdrom -boot d,menu=off -display none \
        -usb -device usb-kbd,port=1 -device usb-hub,port=2 -device usb-mouse,port=2.1 \
        "${NET[@]}" "${qargs[@]}" \
        -serial "unix:$d/ser.sock,server,nowait" -monitor "unix:$d/mon.sock,server,nowait" &
    local qpid=$!
    sleep 2
    mkfifo "$d/in.fifo"
    socat "unix-connect:$d/ser.sock" - < "$d/in.fifo" > "$d/serial.log" 2>/dev/null &
    local spid=$!
    exec 9>"$d/in.fifo"

    local mon_sock="$d/mon.sock" crop=""
    mon() { echo "$1" | socat - "unix-connect:$mon_sock" >/dev/null 2>&1; }
    nudge() {
        local dx=$1 dy=$2 sx sy
        while [ "$dx" -ne 0 ] || [ "$dy" -ne 0 ]; do
            sx=$dx; [ "$sx" -gt 100 ] && sx=100; [ "$sx" -lt -100 ] && sx=-100
            sy=$dy; [ "$sy" -gt 100 ] && sy=100; [ "$sy" -lt -100 ] && sy=-100
            mon "mouse_move $sx $sy"; dx=$((dx - sx)); dy=$((dy - sy)); sleep 0.05
        done
    }

    local step kind arg
    for step in "$@"; do
        kind=${step%%:*}; arg=${step#*:}
        case "$kind" in
            wait_boot) sleep "$BOOT_WAIT" ;;
            cmd)   printf '%s\n' "$arg" >&9 ;;
            key)   mon "sendkey $arg" ;;
            sleep) sleep "$arg" ;;
            crop)  crop=$arg ;;
            home)  nudge -1400 -900; nudge -1400 -900 ;;
            move)  nudge "${arg%%,*}" "${arg##*,}" ;;
            click) mon "mouse_button 1"; sleep 0.2; mon "mouse_button 0" ;;
            down)  mon "mouse_button 1" ;;
            up)    mon "mouse_button 0" ;;
            shot)
                mon "screendump $d/s.ppm"; sleep 1.5
                if [ -s "$d/s.ppm" ]; then
                    local c=()
                    [ -n "$crop" ] && c=(-crop "$crop" +repage)
                    convert "$d/s.ppm" "${c[@]}" -dither None -colors 256 PNG8:"$DEST/$arg.png" 2>/dev/null \
                        && echo "    $DEST/$arg.png ($(stat -c %s "$DEST/$arg.png") bytes)"
                else
                    echo "    (no screendump for $arg)"
                fi
                rm -f "$d/s.ppm"; crop="" ;;
        esac
    done

    mon quit; sleep 1
    exec 9>&-
    kill $qpid $spid 2>/dev/null; wait $qpid $spid 2>/dev/null
}

# The console window is 500x320 and keeps the tail of whatever a command printed. For the
# text-heavy scenarios, drag its bottom-right corner (a resize handle) out to most of the screen.
BIG_CONSOLE="home: move:546,446 down: sleep:0.3 move:690,330 up: sleep:1 cmd:clear sleep:0.5"

# ---- scenarios ---------------------------------------------------------------------------

sc_grub() {   # the boot menu, with the installer's entry
    run grub "$ISO" -- sleep:1.8 shot:grub-menu
}

sc_boot() {   # the kernel coming up: the boot log and its progress bar
    run boot "$ISO" -- sleep:5.3 shot:boot-log
}

sc_desktop() {   # the desktop, and the shell in its console window
    run desktop "$ISO" -- wait_boot: cmd:clear cmd:lsblk cmd:cpus sleep:2 shot:desktop
}

sc_help() {   # the shell and the kernel console's commands
    run help "$ISO" -- wait_boot: $BIG_CONSOLE cmd:help sleep:3 shot:shell-help
}

sc_hardware() {   # what the kernel found: PCI, network
    NET=(-netdev user,id=n0 -device e1000,netdev=n0)
    run hardware "$ISO" -- wait_boot: $BIG_CONSOLE cmd:net sleep:2 cmd:pci sleep:3 shot:console-pci
    NET=("${NET_NONE[@]}")
}

sc_windows() {   # two ring-3 programs as windows on the desktop at the same time
    # The windows open on top of one another; drag the clock (focused, so its keys would be the
    # clock's) by its title bar out to the right, then type a sum into the calculator behind it.
    run windows "$ISO" -- wait_boot: cmd:clear "cmd:calc &" sleep:3 "cmd:clock &" sleep:4 \
        home: move:250,100 down: sleep:0.3 move:420,60 up: sleep:1 \
        home: move:200,72 click: sleep:0.5 \
        key:2 key:shift-equal key:3 key:shift-8 key:4 key:ret sleep:2 shot:windows
}

sc_fault() {   # fault isolation: a ring-3 crash kills one process, not the machine
    run fault "$ISO" -- wait_boot: $BIG_CONSOLE cmd:fault sleep:3 "cmd:echo the machine is still up" cmd:cpus sleep:2 shot:fault-isolation
}

sc_userland() {   # C++ (STL), threads and TLS, Lua, the C compiler -- all running in ring 3
    run userland "$ISO" -- wait_boot: $BIG_CONSOLE cmd:hellocpp sleep:2 cmd:hellostl sleep:3 "cmd:lua /rd/t.lua" sleep:3 shot:userland
}

sc_doom() {
    on_ramdisk doom1.wad || { echo "=== doom: no doom1.wad on the RAM disk -- skipped"; return; }
    run doom "$ISO" -- wait_boot: cmd:doom sleep:9 shot:doom key:esc sleep:1.5 shot:doom-menu
}

# The tracker, with one of its bundled demo songs as the autosave so it opens on real material.
sc_chipnomad() {
    on_ramdisk cnomad || { echo "=== chipnomad: not on the RAM disk -- skipped"; return; }
    local t="$WORK/song-iso" song=third_party/chipnomad/tracker/packaging/common/projects/MICROEGGZ.cnm
    [ -f "$song" ] || { echo "=== chipnomad: no demo song -- skipped"; return; }
    rm -rf "$t"; mkdir -p "$t"
    cp -r iso "$t/iso"; cp initrd.img "$t/initrd.img"
    mcopy -i "$t/initrd.img" -D o "$song" ::autosave.cnm
    cp "$t/initrd.img" "$t/iso/boot/initrd.img"
    grub-mkrescue -o "$WORK/with-song.iso" "$t/iso" >/dev/null 2>&1
    run chipnomad "$WORK/with-song.iso" -- wait_boot: "cmd:start /rd/cnomad" sleep:6 crop:960x640+160+80 shot:chipnomad \
        key:shift-right sleep:1.5 key:shift-right sleep:2 crop:960x640+160+80 shot:chipnomad-phrase
}

sc_qt() {
    on_ramdisk hqtgui || { echo "=== qt: hqtgui not on the RAM disk -- skipped"; return; }
    run qt-gui "$ISO" -- wait_boot: cmd:hqtgui sleep:9 shot:qt-gui
    run qt-widgets "$ISO" -- wait_boot: cmd:hqtwid sleep:9 shot:qt-widgets
    on_ramdisk qtlua && run qt-lua "$ISO" -- wait_boot: cmd:qtlua sleep:9 shot:qt-lua
}

# The installer: the wizard's pages, and the progress bar part way through. Boots the
# installer entry of a copy of the ISO whose menu starts on it, onto a blank AHCI disk (DMA:
# the install takes seconds). Enter = Next on the first three pages, Space ticks the box.
sc_installer() {
    [ -f payload.img ] || { echo "=== installer: no payload.img -- run make iso; skipped"; return; }
    local t="$WORK/inst-iso"
    rm -rf "$t"; mkdir -p "$t/boot/grub"
    cp iso/boot/kernel.elf iso/boot/initrd.img iso/boot/payload.img "$t/boot/"
    sed -e 's/^set default=0/set default=1/' -e 's/^set timeout=.*/set timeout=1/' \
        arch/x86/boot/grub/grub.cfg > "$t/boot/grub/grub.cfg"
    grub-mkrescue -o "$WORK/install.iso" "$t" >/dev/null 2>&1
    qemu-img create -f raw "$WORK/target.img" 1G >/dev/null
    local W=640x480+320+160       # the wizard's surface, centred on the 1280x800 screen
    run installer "$WORK/install.iso" \
        -drive id=tgt,file="$WORK/target.img",format=raw,if=none -device ich9-ahci,id=ahci \
        -device ide-hd,drive=tgt,bus=ahci.0 -- \
        sleep:14 crop:$W shot:installer-welcome key:ret sleep:1 crop:$W shot:installer-disk key:ret sleep:1 key:ret sleep:1 \
        key:spc sleep:1 crop:$W shot:installer-review key:ret sleep:7 crop:$W shot:installer-progress sleep:20 crop:$W shot:installer-finish
}

ALL="grub boot desktop help hardware windows fault userland doom chipnomad qt installer"
if [ "${1:-}" = list ]; then echo "$ALL"; exit 0; fi
for s in ${@:-$ALL}; do
    if declare -f "sc_$s" >/dev/null; then "sc_$s"; else echo "screenshots: no scenario '$s' (try: list)"; fi
done
echo "screenshots: done -> $DEST"
