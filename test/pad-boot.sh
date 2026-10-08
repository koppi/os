#!/bin/bash
# pad-boot.sh — check the USB game-controller driver on every host controller,
# headless, with no hardware and no WAD.
#
#   test/pad-boot.sh [uhci|xhci|ehci|hub|doom|all]      (default: all, except doom)
#   DOOM_BUS=xhci DOOM_PAD=ds4 test/pad-boot.sh doom    (a topology / pad for doom)
#
# QEMU has no emulated gamepad, so test/usbpad.py plays one: it speaks the
# usbredir protocol to a `usb-redir` device, answers the guest's enumeration
# like a real pad and sends interrupt reports on command. Everything from the
# host controller up (descriptor fetch, the HID report-descriptor parser, the
# state a program reads through `getpad`) runs unmodified.
#
# One scenario per host controller, each with a different pad:
#
#   uhci   a generic DirectInput pad (Game Pad, 4 axes + hat + 12 buttons) on the
#          USB 1.1 controller the Makefile's qemu-iso uses
#   xhci   a DualShock-4-shaped pad (report IDs, 64-byte reports, a 450-byte
#          descriptor with vendor collections), then unplugged and plugged back
#          in -- xhci.c used to ignore a disconnect, so a replugged pad was
#          never seen again
#   ehci   an Xbox 360 wired controller (vendor class, no HID descriptor) as a
#          high-speed device, with `ehci` on the kernel command line
#   hub    the generic pad behind a USB hub on UHCI, pulled out and plugged back
#          in: enumeration through a hub, and usb_release_device() -> the pad
#          being let go (the one unplug path UHCI has)
#
# Each is driven through the same sequence (a button, a stick pushed to each
# corner, hat + several buttons, triggers) and read back through the console's
# `pad` command, which prints the state a program gets. The expected numbers are
# the same whatever the pad: that every profile ends up as the same buttons,
# sticks and D-pad is what the normalisation is for.
#
# `doom` (needs doom1.wad on the RAM disk, see apps/doom/PORTING.md) plays the
# game with the pad: walks, turns, fires, opens the menu, one PNG per step.
set -u
trap '' PIPE
cd "$(dirname "${BASH_SOURCE[0]}")/.."
OUT=${OUT:-/tmp/pad-boot}
mkdir -p "$OUT"
BOOT_WAIT=${BOOT_WAIT:-25}
KVM=${KVM:--enable-kvm}

for t in qemu-system-i386 socat python3; do
    command -v "$t" >/dev/null || { echo "pad-boot: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "pad-boot: no os.iso -- run 'make iso' first"; exit 1; }

fail=0

# start_vm <name> <profile> <speed> <boot: iso|kernel> <qemu args...>
#
# On return: $D is the scenario directory, fd 7 feeds usbpad.py, fd 9 types at
# the kernel shell, and mon / padcmd / shell work.
start_vm() {
    local name=$1 profile=$2 speed=$3 boot=$4; shift 4
    D="$OUT/$name"
    rm -rf "$D"; mkdir -p "$D"
    mkfifo "$D/in.fifo" "$D/pad.fifo"
    echo "=== $name ($profile pad, $speed speed)"

    local bootargs=(-drive file=os.iso,if=ide,index=1,media=cdrom -boot d,menu=off)
    [ "$boot" = kernel ] && bootargs=(-kernel kernel.elf -initrd initrd.img -append ehci)

    qemu-system-i386 -m 512M -smp 4 -no-reboot -vga std $KVM "${bootargs[@]}" \
        -display none \
        -serial "unix:$D/ser.sock,server,nowait" \
        -monitor "unix:$D/mon.sock,server,nowait" \
        -chardev "socket,id=pad,path=$D/pad.sock,server=on,wait=off" \
        "$@" &
    QPID=$!
    local i
    for i in $(seq 50); do [ -S "$D/pad.sock" ] && [ -S "$D/ser.sock" ] && break; sleep 0.2; done

    python3 -I test/usbpad.py --sock "$D/pad.sock" --profile "$profile" --speed "$speed" -v \
        < "$D/pad.fifo" > "$D/pad.out" 2> "$D/pad.err" &
    PPID_=$!
    socat "unix-connect:$D/ser.sock" - < "$D/in.fifo" > "$D/serial.log" 2>/dev/null &
    SPID=$!
    exec 7>"$D/pad.fifo"
    exec 9>"$D/in.fifo"
}

stop_vm() {
    mon quit; sleep 1
    exec 7>&- 9>&-          # usbpad.py leaves by itself when QEMU hangs up
    kill $QPID $SPID $PPID_ 2>/dev/null; wait $QPID $SPID $PPID_ 2>/dev/null
    rm -f "$D"/*.sock "$D"/*.fifo
}

mon()    { echo "$1" | socat - "unix-connect:$D/mon.sock" >/dev/null 2>&1; }
padcmd() { echo "$*" >&7; sleep "${PAD_GAP:-0.8}"; }
shell()  { printf '%s\n' "$1" >&9; }
clean()  { sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$D/serial.log"; }

# Run the standard sequence against the `pad` console command.
drive_pad() {
    shell pad; sleep 2
    padcmd tap 1 500                                         # a button
    padcmd set lx=1 ly=-1                                    # stick: right, up
    padcmd set lx=-1 ly=1 rx=0 ry=0                          # stick: left, down
    padcmd set lx=0 ly=0 rx=-0.7 ry=0.5 hat=ne buttons=3,5,10  # right stick, hat, buttons
    padcmd set rx=0 ry=0 hat=s buttons=7,8                   # D-pad down, buttons 7+8
    padcmd set hat=none buttons= lt=0.8 rt=0.3               # triggers
    padcmd set lt=0 rt=0
    mon "sendkey esc"; sleep 1.5                             # `pad` stops on Esc
}

# Read what `pad` printed and check it. $1 = scenario name.
check_pad() {
    local name=$1
    clean > "$D/clean.log"
    python3 -I - "$D/clean.log" "$name" <<'EOF'
import re, sys
log, name = sys.argv[1], sys.argv[2]
text = open(log, errors="replace").read()

rows = []
pat = re.compile(r"^pad 0: L\(\s*(-?\d+),\s*(-?\d+)\) R\(\s*(-?\d+),\s*(-?\d+)\) "
                 r"T\(\s*(\d+),\s*(\d+)\) dpad (\S{4})\s+buttons: (.*)$")
for line in text.splitlines():
    m = pat.match(line.strip())
    if m:
        lx, ly, rx, ry, lt, rt = map(int, m.groups()[:6])
        btn = [] if m.group(8).strip() == "-" else [int(x) for x in m.group(8).split()]
        rows.append(dict(lx=lx, ly=ly, rx=rx, ry=ry, lt=lt, rt=rt, dpad=m.group(7), b=btn))

# Each step must show up, in order, somewhere after the previous one.
steps = [
    ("button 1 pressed",            lambda r: r["b"] == [1]),
    ("button 1 released",           lambda r: r["b"] == [] and r["lx"] < 1000),
    ("left stick right+up",         lambda r: r["lx"] > 30000 and r["ly"] < -30000),
    ("left stick left+down",        lambda r: r["lx"] < -30000 and r["ly"] > 30000),
    ("right stick, hat NE, 3 5 10", lambda r: r["rx"] < -15000 and r["ry"] > 5000
                                              and r["dpad"] == "U--R" and r["b"] == [3, 5, 10]),
    ("D-pad down, buttons 7 8",     lambda r: r["dpad"] == "-D--" and r["b"] == [7, 8]),
    ("triggers 0.8 / 0.3",          lambda r: 190 <= r["lt"] <= 215 and 65 <= r["rt"] <= 90),
]
if name == "uhci":                  # the generic pad has no analog triggers
    steps = steps[:-1]
fails = []
i = 0
for label, pred in steps:
    while i < len(rows) and not pred(rows[i]):
        i += 1
    if i == len(rows):
        fails.append(label)
        i = 0       # keep checking the rest from the top rather than cascading
    else:
        i += 1
    print("    %-30s %s" % (label, "ok" if label not in fails else "MISSING"))
if not rows:
    print("    no `pad 0:` state lines at all")
sys.exit(1 if fails or not rows else 0)
EOF
}

# What the kernel logged while bringing the pad up, and that nothing else broke.
# (The host controller's own line is shown but not required: threads share the
# console, and a line can come out with another's text in the middle of it.)
check_boot() {
    local name=$1 miss=
    grep -aq "gamepad: .*is pad 0" "$D/clean.log" || miss="$miss pad-not-attached"
    grep -aq "smp: 4/4 CPUs online" "$D/clean.log" || miss="$miss 4-cpus"
    grep -a "gamepad:\|USB HID: game\|ehci: HID game\|xhci: slot.*proto" "$D/clean.log" | sed 's/^/    /' | head -4
    if [ -n "$miss" ]; then
        echo "    FAIL ($name boot): missing$miss"; return 1
    fi
    return 0
}

finish() {
    local name=$1 ok=$2
    if [ "$ok" = 0 ]; then
        echo "    PASS ($name)"
    else
        echo "    FAIL ($name) -> $D/serial.log, $D/pad.err"
        fail=1
    fi
    echo
}

do_uhci() {
    start_vm uhci generic full iso -machine pc -usb -device usb-redir,chardev=pad
    sleep "$BOOT_WAIT"
    drive_pad
    stop_vm
    clean > "$D/clean.log"
    local ok=0
    check_boot uhci || ok=1
    check_pad uhci || ok=1
    finish uhci $ok
}

do_xhci() {
    start_vm xhci ds4 full iso -machine q35 -device qemu-xhci,id=xhci \
        -device usb-redir,chardev=pad,bus=xhci.0
    sleep "$BOOT_WAIT"
    drive_pad

    # Unplug and plug back in. Nothing used to release an xHCI slot, so the
    # replugged pad found its port still "enumerated" and was never looked at.
    padcmd unplug; sleep 2
    padcmd plug;   sleep 5
    shell pad; sleep 2
    padcmd tap 2 500
    mon "sendkey esc"; sleep 1.5
    stop_vm

    clean > "$D/clean.log"
    local ok=0
    check_boot xhci || ok=1
    check_pad xhci || ok=1
    grep -aq "gamepad: pad 0 (054c:09cc) gone" "$D/clean.log"  || { echo "    unplug: pad never released"; ok=1; }
    [ "$(grep -ac 'gamepad: 054c:09cc is pad 0' "$D/clean.log")" -ge 2 ] \
        || { echo "    replug: pad never re-attached"; ok=1; }
    # after the replug, the second `pad` session must see button 2
    awk '/gamepad: .* is pad 0/ {n=1; next} n && /^pad 0: L.*buttons: 2$/ {found=1} END {exit !found}' \
        "$D/clean.log" || { echo "    replug: button 2 not seen after re-attach"; ok=1; }
    finish xhci $ok
}

do_hub() {
    start_vm hub generic full iso -machine pc -usb -device usb-hub,bus=usb-bus.0,port=1 \
        -device usb-redir,chardev=pad,bus=usb-bus.0,port=1.1
    sleep "$BOOT_WAIT"
    drive_pad

    # The hub driver notices a port change on a half-second poll.
    padcmd unplug; sleep 3
    padcmd plug;   sleep 6
    shell pad; sleep 2
    padcmd tap 2 500
    mon "sendkey esc"; sleep 1.5
    stop_vm

    clean > "$D/clean.log"
    local ok=0
    check_boot hub || ok=1
    check_pad uhci || ok=1                 # same pad, same expectations as uhci
    grep -aq "gamepad: pad 0 (0079:0011) gone" "$D/clean.log"  || { echo "    unplug: pad never released"; ok=1; }
    [ "$(grep -ac 'gamepad: 0079:0011 is pad 0' "$D/clean.log")" -ge 2 ] \
        || { echo "    replug: pad never re-attached"; ok=1; }
    awk '/gamepad: .* is pad 0/ {n=1; next} n && /^pad 0: L.*buttons: 2$/ {found=1} END {exit !found}' \
        "$D/clean.log" || { echo "    replug: button 2 not seen after re-attach"; ok=1; }
    finish hub $ok
}

do_ehci() {
    start_vm ehci xinput high kernel -machine pc -device usb-ehci,id=ehci \
        -device usb-redir,chardev=pad,bus=ehci.0
    sleep "$BOOT_WAIT"
    drive_pad
    stop_vm
    clean > "$D/clean.log"
    local ok=0
    check_boot ehci || ok=1
    check_pad ehci || ok=1
    finish ehci $ok
}

# Play Doom with the pad: walk, turn, fire, change weapon, open the menu and move
# around in it, all from the pad, and check each against the game's own frames.
# Needs doom1.wad on the RAM disk (apps/doom/PORTING.md); one PNG per step.
#
# The checks are pixel comparisons of the screenshots -- crude, but what they
# compare is the game's output, not anything the driver says about itself.
do_doom() {
    if [ ! -f doom1.wad ]; then
        echo "=== doom: SKIP (no doom1.wad in the repo root, see apps/doom/PORTING.md)"; echo
        return
    fi
    # DOOM_BUS=uhci|xhci and DOOM_PAD=generic|ds4|xinput pick the topology and the
    # pad (default: the generic pad on UHCI); the game and the checks are the same.
    local bus=${DOOM_BUS:-uhci} padp=${DOOM_PAD:-generic}
    if [ "$bus" = xhci ]; then
        start_vm doom "$padp" full iso -machine q35 -vga virtio -device qemu-xhci,id=xhci \
            -device usb-redir,chardev=pad,bus=xhci.0
    else
        start_vm doom "$padp" full iso -machine pc -usb -vga virtio \
            -device usb-redir,chardev=pad
    fi
    sleep "$BOOT_WAIT"
    shot() { mon "screendump $D/$1.ppm"; sleep 1
             pnmtopng "$D/$1.ppm" > "$D/$1.png" 2>/dev/null; }
    shell 'doom -warp 1 1 -skill 2'; sleep 8
    shot start
    padcmd set ly=-1; sleep 1.2; padcmd set ly=0       # left stick up: walk forward
    shot forward
    padcmd set rx=1; sleep 0.6; padcmd set rx=0        # right stick right: turn
    shot turned
    padcmd tap 8 300; sleep 0.4                        # button 8: fire
    shot fired
    padcmd tap 6 150; sleep 1.5                        # right shoulder: next weapon
    shot weapon
    padcmd tap 10 200; sleep 1                         # Start: the menu
    shot menu
    padcmd set hat=s; padcmd set hat=none              # D-pad down: next item
    shot menu_dpad
    padcmd set ly=1; padcmd set ly=0                   # left stick down: and the next
    shot menu_stick
    padcmd set hat=n; padcmd set hat=none              # D-pad up: back one
    shot menu_up
    padcmd tap 10 200; sleep 1                         # Start again: closes it
    shot closed
    stop_vm

    python3 -I - "$D" <<'EOF'
import sys
from PIL import Image
d = sys.argv[1]
def img(n): return Image.open("%s/%s.png" % (d, n)).convert("RGB")

def diff(a, b, box):
    """mean absolute difference per channel inside box (x0,y0,x1,y1)"""
    pa, pb = img(a).crop(box), img(b).crop(box)
    da, db = pa.tobytes(), pb.tobytes()
    return sum(abs(x - y) for x, y in zip(da, db)) / len(da)

def skull(n):
    """which main-menu row the skull cursor is on (0 = New Game), or None"""
    im = img(n)
    ys = [y for y in range(240, 660) if sum(
        1 for x in range(255, 330)
        if (lambda p: p[0] > 150 and p[1] > 110 and p[2] > 70 and p[0] - p[2] > 40)(
            im.getpixel((x, y)))) > 8]
    return None if not ys else (min(ys) - 240 + 32) // 64

VIEW, AMMO, GUN = (0, 0, 1280, 660), (60, 690, 190, 760), (480, 480, 800, 665)
LOGO = (380, 0, 870, 230)       # the DOOM logo above the menu
checks = [
    ("left stick up walked forward",   diff("start", "forward", VIEW) > 8),
    ("right stick turned the view",    diff("forward", "turned", VIEW) > 8),
    ("button 8 fired (ammo changed)",  diff("turned", "fired", AMMO) > 3),
    ("right shoulder changed weapon",  diff("fired", "weapon", GUN) > 3),
    ("Start opened the menu",          skull("menu") == 0),
    ("D-pad down moved the cursor",    skull("menu_dpad") == 1),
    ("left stick down moved it again", skull("menu_stick") == 2),
    ("D-pad up moved it back",         skull("menu_up") == 1),
    ("Start closed the menu",          diff("menu_up", "closed", LOGO) > 20),
]
bad = 0
for label, ok in checks:
    print("    %-32s %s" % (label, "ok" if ok else "FAILED"))
    bad += not ok
sys.exit(1 if bad else 0)
EOF
    finish doom $?
}

# (Sourced rather than run, the functions above are there to build a one-off
# scenario from.)
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    for arg in "${@:-all}"; do
        case $arg in
            uhci) do_uhci ;;
            xhci) do_xhci ;;
            ehci) do_ehci ;;
            hub)  do_hub ;;
            doom) do_doom ;;
            all)  do_uhci; do_xhci; do_ehci; do_hub ;;
            *)    echo "usage: $0 [uhci|xhci|ehci|hub|doom|all]" >&2; exit 1 ;;
        esac
    done
    exit $fail
fi
