#!/bin/bash
# microui-boot.sh — boot os.iso headless and drive the two ring-3 microui apps,
# apps/calc and apps/clock, so the port can be checked without sitting in front
# of a display.
#
# The shell is reached over the serial console (uart.c feeds serial RX into the
# console input ring). Keystrokes go in with the monitor's `sendkey`, which
# exercises the raw-scancode path these apps read (make *and* break, which the
# ASCII ring does not carry); pointer motion and clicks go in with `mouse_move`
# and `mouse_button`, which is the only way to reach the pointer ring behind
# syscall 36. Every step leaves a PNG behind, and the apps echo what they did
# to the console, so each scenario can be judged from the serial log as well as
# from the pictures.
#
#   test/microui-boot.sh [calc|keys|mouse|clock|quit|all]      (default: all)
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

# The apps grab a 640x400 surface and video_blit8 scales it by the largest
# whole number that fits, centred. Everything the mouse steps below assume
# about where a button is comes from these two numbers.
SURF_W=640
SURF_H=400

# run <name> <command line> <script>
#
# <script> is one "delay:action" per line, applied in order after the command
# has been sent: key:<k> taps a key, shot:<n> screenshots, serial:<text> types
# a line at the shell, home: parks the pointer at the surface's top-left,
# to:<x>,<y> moves it there (from wherever `home` left it), click: taps the
# left button.
run() {
    local name=$1 cmd=$2 script=$3
    local d="$OUT/$name"
    rm -rf "$d"; mkdir -p "$d"
    echo "=== $name: $cmd"

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

    # The pointer is relative, so there is no "move to (x,y)" -- park it against
    # a corner first and count from there. A USB boot-protocol mouse carries one
    # signed byte per axis per report, so a long move is many short ones.
    nudge() {
        local dx=$1 dy=$2 step
        while [ "$dx" -ne 0 ] || [ "$dy" -ne 0 ]; do
            step=$dx; [ "$step" -gt 100 ] && step=100; [ "$step" -lt -100 ] && step=-100
            local sy=$dy; [ "$sy" -gt 100 ] && sy=100; [ "$sy" -lt -100 ] && sy=-100
            mon "mouse_move $step $sy"
            dx=$((dx - step)); dy=$((dy - sy))
            sleep 0.05
        done
    }
    home() { nudge -700 -700; nudge -700 -700; }

    sleep "$BOOT_WAIT"
    printf '%s\n' "$cmd" >&9

    local step delay rest kind arg
    while IFS= read -r step; do
        [ -z "$step" ] && continue
        delay=${step%%:*}; rest=${step#*:}
        kind=${rest%%:*}; arg=${rest#*:}
        sleep "$delay"
        case "$kind" in
            key)    mon "sendkey $arg" ;;
            serial) printf '%s\n' "$arg" >&9 ;;
            home)   home ;;
            to)     nudge "${arg%%,*}" "${arg##*,}" ;;
            click)  mon "mouse_button 1"; sleep 0.2; mon "mouse_button 0" ;;
            shot)   mon "screendump $d/$arg.ppm"; sleep 1
                    [ -f "$d/$arg.ppm" ] && command -v pnmtopng >/dev/null \
                        && pnmtopng "$d/$arg.ppm" > "$d/$arg.png" 2>/dev/null
                    rm -f "$d/$arg.ppm" ;;
        esac
    done <<< "$script"

    mon quit
    sleep 1
    exec 9>&-
    kill $qpid $spid 2>/dev/null; wait $qpid $spid 2>/dev/null
    rm -f "$d"/*.sock "$d"/in.fifo

    # Strip the kernel's own timestamped log lines: what matters here is what
    # the app printed.
    sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$d/serial.log" \
        | grep -vE '^\[ *[0-9]+\.[0-9]+\]' | grep -v '^$' | tail -"${TAIL:-12}"
    echo "    -> $d"
    echo
}

# The calculator comes up and draws: window, display, 4x5 keypad, pointer.
do_calc() {
    run calc calc $'4:shot:window'
}

# Arithmetic from the keyboard. Immediate execution, so "2 + 3 * 4" settles to
# 5 at the '*' and then to 20 -- the log lines say so, which is the point.
# Then a divide by zero, which must be caught and must need C to leave.
do_keys() {
    run keys calc \
        $'4:shot:idle\n1:key:2\n1:key:shift-equal\n1:key:3\n1:key:shift-8\n1:key:4\n1:key:ret\n2:shot:result\n1:key:c\n1:key:8\n1:key:slash\n1:key:0\n1:key:ret\n2:shot:divzero\n1:key:c\n2:shot:cleared'
}

# The pointer: park it, walk it to the "7" key and click, then to "+", "5", "=".
# This is the syscall-36 path -- relative motion and a button mask -- plus
# microui's own hit testing on top of it.
do_mouse() {
    # Keypad geometry: the 320x328 window is centred in the 640x400 surface, so
    # its body starts at (165, 65); the display row is 54 high and the button
    # rows 40 high, 4 apart, in 71-wide columns 4 apart (the last stretches to
    # the body's right edge). That puts "7" at (200, 187), "+" at (432, 275),
    # "5" at (275, 231) and "=" at (432, 319): 7 + 5 = 12.
    run mouse calc \
        $'4:home:\n1:to:200,187\n1:shot:hover\n1:click:\n1:to:232,88\n1:click:\n1:to:-157,-44\n1:click:\n1:to:157,88\n1:click:\n2:shot:clicked'
}

# The clock: two shots a few seconds apart (the hands must have moved), then a
# click on "24h" to check that a control under the custom-drawn face still
# works. -v makes it log a line a second, so the serial tail shows time running.
do_clock() {
    # The 332x384 window is centred in the 640x400 surface, which puts the
    # "24h" checkbox -- the middle one on the options row under the face -- at
    # (277, 348).
    run clock 'clock -v' \
        $'4:shot:face\n6:shot:later\n1:home:\n1:to:277,348\n1:click:\n2:shot:24h'
}

# Esc gives the screen back: the desktop has to return and the shell has to get
# the keyboard back, for both apps in turn.
do_quit() {
    run quit calc \
        $'4:shot:calc\n1:key:esc\n3:shot:desktop\n1:serial:clock\n5:shot:clock\n1:key:esc\n3:shot:back\n1:serial:ls /rd\n3:shot:shell'
}

case "${1:-all}" in
    calc)  do_calc ;;
    keys)  do_keys ;;
    mouse) do_mouse ;;
    clock) do_clock ;;
    quit)  do_quit ;;
    all)   do_calc; do_keys; do_mouse; do_clock; do_quit ;;
    *)     echo "usage: $0 [calc|keys|mouse|clock|quit|all]"; exit 1 ;;
esac
