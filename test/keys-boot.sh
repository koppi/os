#!/bin/bash
# keys-boot.sh — check the keyboard on every input topology this kernel runs
# on, headless, with no WAD needed.
#
#   test/keys-boot.sh [ps2|uhci|xhci|all]          (default: all)
#
# Two phases per topology, because a keystroke takes two different routes into
# the system and the arrow cluster used to be lost on both:
#
#   raw    the scancode ring a full-screen program reads (keyboard.h, behind
#          the `getscan` syscall) -- make/break with the 0xE0 prefix, which is
#          what apps/doom binds movement to. The kernel console's `keys`
#          command prints it; the four arrows have to arrive as E0 48/50/4b/4d,
#          press and release.
#   shell  the character ring, which the shell's line editor reads through
#          `getkey`. A cursor key reaches it as one control character
#          (keyboard.h), so the checks here are what the editor *did* with it:
#          text inserted mid-line, Home, and history.
#
# One scenario per keyboard this kernel can be driven by, because they are
# three separate decode paths and only the first was ever covered:
#
#   ps2    i8042, decoded in keyboard.c's IRQ handler        (QEMU default, X250)
#   uhci   USB HID boot keyboard via uhci.c + usb_hid.c      (the Makefile's
#                                                             qemu-iso config)
#   xhci   USB HID boot keyboard via xhci.c, with the i8042 switched off, so
#          the only keyboard on the machine is the USB one   (MacBook Air 2013,
#          X250/T470s with an external keyboard)
#
# The shell phase also types a terminal's own `ESC [ D` down the serial line,
# which is the fourth path: uart.c reassembling an escape sequence into the
# same cursor key (and not quitting on the ESC, which it used to do).
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/keys-boot}
mkdir -p "$OUT"
BOOT_WAIT=${BOOT_WAIT:-14}     # seconds from power-on to a usable prompt
KVM=${KVM:--enable-kvm}

for t in qemu-system-i386 socat; do
    command -v "$t" >/dev/null || { echo "keys-boot: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "keys-boot: no os.iso -- run 'make iso' first"; exit 1; }

fail=0

# run <name> <qemu args...>
run() {
    local name=$1; shift
    local d="$OUT/$name"
    rm -rf "$d"; mkdir -p "$d"; mkfifo "$d/in.fifo"
    echo "=== $name"

    qemu-system-i386 -m 512M -smp 2 -no-reboot -vga std $KVM \
        -drive file=os.iso,if=ide,index=1,media=cdrom -boot d,menu=off \
        -display none \
        -serial "unix:$d/ser.sock,server,nowait" \
        -monitor "unix:$d/mon.sock,server,nowait" "$@" &
    local qpid=$!
    local i
    for i in $(seq 40); do [ -S "$d/ser.sock" ] && break; sleep 0.2; done
    socat "unix-connect:$d/ser.sock" - < "$d/in.fifo" > "$d/serial.log" 2>/dev/null &
    local spid=$!
    exec 9>"$d/in.fifo"
    mon()   { echo "$1" | socat - "unix-connect:$d/mon.sock" >/dev/null 2>&1; }
    key()   { mon "sendkey $1"; sleep 0.5; }
    type_() { printf '%s' "$1" >&9; sleep 0.6; }

    sleep "$BOOT_WAIT"

    # -- phase 1: the raw scancode stream ----------------------------------
    printf 'keys\n' >&9
    sleep 3
    local k
    for k in up down left right; do key "$k"; done
    mon "sendkey esc"              # `keys` stops on Esc
    sleep 2

    # -- phase 2: the shell's line editor ----------------------------------
    # `echo abcd`, Left Left, X          -> the line becomes `echo abXcd`
    type_ 'echo abcd'; key left; key left; type_ 'X'; type_ $'\n'; sleep 1
    # `cho hi`, Home, e                  -> `echo hi`
    type_ 'cho hi'; key home; type_ 'e'; type_ $'\n'; sleep 1
    # Up (recalls `echo hi`), End, !     -> `echo hi!`
    key up; key end; type_ '!'; type_ $'\n'; sleep 1
    # the same Left, but as a terminal's escape sequence over the serial line
    type_ 'echo 12'; type_ $'\033[D'; type_ 'Z'; type_ $'\n'; sleep 1

    mon quit; sleep 1
    exec 9>&-
    kill $qpid $spid 2>/dev/null; wait $qpid $spid 2>/dev/null
    rm -f "$d"/*.sock "$d"/in.fifo

    # -- what came out -----------------------------------------------------
    local miss= sc want
    grep -a '^keys: E0' "$d/serial.log" | sed 's/^/    /'
    for sc in 48 50 4b 4d; do
        grep -qa "^keys: E0 $sc press"   "$d/serial.log" || miss="$miss raw:E0-$sc-press"
        grep -qa "^keys: E0 $sc release" "$d/serial.log" || miss="$miss raw:E0-$sc-release"
    done
    # Each is the output of a line that could only be built with the cursor
    # keys: mid-line insert, Home, history + End, and the serial escape form.
    for want in abXcd 'hi!' 1Z2; do
        grep -qa "^$want" "$d/serial.log" || miss="$miss shell:$want"
    done
    echo "    shell: $(for want in abXcd 'hi!' 1Z2; do
                          grep -qa "^$want" "$d/serial.log" && printf '%s ' "$want=ok" \
                                                            || printf '%s ' "$want=MISSING"
                       done)"
    if [ -n "$miss" ]; then
        echo "    FAIL ($name): missing$miss"
        echo "    -> $d/serial.log"
        fail=1
    else
        echo "    PASS ($name): arrows in the raw stream and in the line editor"
    fi
    echo
}

do_ps2()  { run ps2 -machine pc; }
do_uhci() { run uhci -machine pc,i8042=off -usb -device usb-kbd; }
do_xhci() { run xhci -machine q35,i8042=off -device qemu-xhci,id=xhci -device usb-kbd; }

for arg in "${@:-all}"; do
    case $arg in
        ps2)  do_ps2 ;;
        uhci) do_uhci ;;
        xhci) do_xhci ;;
        all)  do_ps2; do_uhci; do_xhci ;;
        *)    echo "usage: $0 [ps2|uhci|xhci|all]" >&2; exit 1 ;;
    esac
done

exit $fail
