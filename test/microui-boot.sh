#!/bin/bash
# microui-boot.sh — boot os.iso headless and drive the ring-3 microui apps,
# apps/calc and apps/clock, so the window manager can be checked without
# sitting in front of a display.
#
# The shell is reached over the serial console (uart.c feeds serial RX into the
# console input ring, a path the window manager deliberately does not take over
# -- see keyboard.c). Keystrokes for the apps go in with the monitor's
# `sendkey`, which is the real keyboard and therefore the routed one; the
# pointer goes in with `mouse_move` and `mouse_button`. Every step leaves a PNG
# behind, and the apps echo what they did to the console, so each scenario can
# be judged from the serial log as well as from the pictures.
#
#   test/microui-boot.sh [window|keys|mouse|two|close|fullscreen|all]
#                                                              (default: all)
#
# Output, one subdirectory per scenario, in $OUT.
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/microui-boot}
mkdir -p "$OUT"
BOOT_WAIT=${BOOT_WAIT:-15}     # seconds from power-on to a usable shell prompt
VGA=${VGA:-virtio}             # virtio | std (std gives a 24-bpp 800x600 mode)
KVM=${KVM:--enable-kvm}

for t in qemu-system-i386 socat; do
    command -v "$t" >/dev/null || { echo "microui-boot: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "microui-boot: no os.iso -- run 'make iso' first"; exit 1; }

# Where the window manager puts things. A program's window is placed at
# (60 + 28*slot, 60 + 28*slot) and is its surface plus one 24-pixel title bar
# (wm.c), so every coordinate below is in *desktop* pixels and independent of
# the screen size -- which is what makes clicking a particular button from a
# script possible at all.
#
#   calc  slot 0: window (60,60) 320x328, body (60,84) 320x304
#   clock slot 1: window (88,88) 332x384, body (88,112) 332x360

# run <name> <script>
#
# <script> is one "kind:arg" step at a time: cmd types a line at the shell,
# key taps a key, shot screenshots, sleep waits, home parks the pointer at the
# desktop's top-left corner, to:<dx>,<dy> walks it from where it is, click taps
# the left button, and down/up bracket a drag.
run() {
    local name=$1; shift
    local d="$OUT/$name"
    rm -rf "$d"; mkdir -p "$d"
    echo "=== $name"

    qemu-system-i386 -vga "$VGA" -m 512M -no-reboot -smp 4 $KVM \
        -rtc base=localtime,clock=vm \
        -drive file=os.iso,if=ide,index=1,media=cdrom \
        -boot d,menu=off -display none \
        -usb -device usb-kbd,port=1 \
        -device usb-hub,port=2 -device usb-mouse,port=2.1 \
        -serial "unix:$d/ser.sock,server,nowait" \
        -monitor "unix:$d/mon.sock,server,nowait" &
    local qpid=$!

    sleep 2
    mkfifo "$d/in.fifo"
    socat "unix-connect:$d/ser.sock" - < "$d/in.fifo" > "$d/serial.log" 2>/dev/null &
    local spid=$!
    exec 9>"$d/in.fifo"

    mon() { echo "$1" | socat - "unix-connect:$d/mon.sock" >/dev/null 2>&1; }

    # The pointer is relative -- there is no "move to (x,y)" -- so park it
    # against the corner first and count from there. A USB boot-protocol mouse
    # carries one signed byte per axis per report, so a long move is many short
    # ones; the kernel clamps the pointer to the screen, which is what makes
    # `home` land exactly on (0,0).
    nudge() {
        local dx=$1 dy=$2 sx sy
        while [ "$dx" -ne 0 ] || [ "$dy" -ne 0 ]; do
            sx=$dx; [ "$sx" -gt 100 ] && sx=100; [ "$sx" -lt -100 ] && sx=-100
            sy=$dy; [ "$sy" -gt 100 ] && sy=100; [ "$sy" -lt -100 ] && sy=-100
            mon "mouse_move $sx $sy"
            dx=$((dx - sx)); dy=$((dy - sy))
            sleep 0.05
        done
    }

    sleep "$BOOT_WAIT"

    local step kind arg
    for step in "$@"; do
        kind=${step%%:*}; arg=${step#*:}
        case "$kind" in
            cmd)   printf '%s\n' "$arg" >&9 ;;
            key)   mon "sendkey $arg" ;;
            sleep) sleep "$arg" ;;
            home)  nudge -900 -900; nudge -900 -900 ;;
            to)    nudge "${arg%%,*}" "${arg##*,}" ;;
            click) mon "mouse_button 1"; sleep 0.2; mon "mouse_button 0" ;;
            down)  mon "mouse_button 1" ;;
            up)    mon "mouse_button 0" ;;
            shot)  mon "screendump $d/$arg.ppm"; sleep 1
                   [ -f "$d/$arg.ppm" ] && command -v pnmtopng >/dev/null \
                       && pnmtopng "$d/$arg.ppm" > "$d/$arg.png" 2>/dev/null
                   rm -f "$d/$arg.ppm" ;;
        esac
    done

    mon quit
    sleep 1
    exec 9>&-
    kill $qpid $spid 2>/dev/null; wait $qpid $spid 2>/dev/null
    rm -f "$d"/*.sock "$d"/in.fifo

    # Strip the kernel's own timestamped log lines, except the window
    # manager's: those say what it decided to do and are the point here.
    sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$d/serial.log" \
        | grep -aoE "wm: window [0-9] '[^']*' [0-9x]+ for pid [0-9]+|^(calc|clock): .*" \
        | tail -"${TAIL:-12}"
    echo "    -> $d"
    echo
}

# The calculator comes up as a window on the running desktop: title bar, close
# box, its surface composited into the body, the rest of the desktop still
# there and still drawing.
do_window() {
    run window "cmd:calc &" sleep:4 shot:desktop
}

# Typing goes to the focused window and nowhere else. Immediate execution, so
# "2 + 3 * 4" settles to 5 at the '*' and then to 20; then a divide by zero,
# which must be caught. If the keystrokes had also reached the shell behind it,
# the serial log would show it trying to run them as commands.
do_keys() {
    run keys "cmd:calc &" sleep:4 \
        key:2 key:shift-equal key:3 key:shift-8 key:4 key:ret sleep:2 shot:result \
        key:c key:8 key:slash key:0 key:ret sleep:2 shot:divzero
}

# The pointer: park it, walk it to the "7" key and click, then "+", "5", "=".
# These are desktop coordinates; the window manager turns them into the
# window's own before the program ever sees them.
do_mouse() {
    run mouse "cmd:calc &" sleep:4 \
        home: to:100,211 shot:hover click: \
        to:232,88 click: to:-157,-44 click: to:157,88 click: \
        sleep:2 shot:clicked
}

# Two programs, two windows, at the same time -- which is the whole point.
# Click the clock's "24h" through its window, then drag that window by its
# title bar and check the program's surface went with it.
do_two() {
    run two "cmd:calc &" sleep:3 "cmd:clock &" sleep:4 shot:both \
        home: to:210,428 click: sleep:2 shot:24h \
        to:40,-328 down: sleep:0.3 to:320,160 up: sleep:2 shot:dragged
}

# Closing. The title bar's box is a *request*: the window manager sends it and
# the program exits, which is what takes the window away. Esc does the same
# from the keyboard. Afterwards the shell must have the keyboard back.
do_close() {
    run close "cmd:calc &" sleep:3 "cmd:clock &" sleep:4 \
        home: to:408,100 click: sleep:3 shot:clock_closed \
        to:-200,0 click: key:esc sleep:3 shot:both_closed \
        "cmd:ls /rd" sleep:2
}

# The other path: `-f` asks for the whole screen instead of a window, which is
# also what happens on a machine with no desktop to put one on. The desktop is
# parked, the program draws its own title bar and its own pointer, and Esc
# gives everything back.
do_fullscreen() {
    run fullscreen "cmd:calc -f" sleep:4 shot:grabbed \
        key:2 key:shift-equal key:3 key:ret sleep:2 shot:result \
        key:esc sleep:3 shot:desktop_back
}

case "${1:-all}" in
    window)     do_window ;;
    keys)       do_keys ;;
    mouse)      do_mouse ;;
    two)        do_two ;;
    close)      do_close ;;
    fullscreen) do_fullscreen ;;
    all)        do_window; do_keys; do_mouse; do_two; do_close; do_fullscreen ;;
    *)          echo "usage: $0 [window|keys|mouse|two|close|fullscreen|all]"
                exit 1 ;;
esac
