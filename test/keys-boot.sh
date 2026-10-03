#!/bin/bash
# keys-boot.sh — check the raw key stream (the one a full-screen program reads)
# on every input topology this kernel runs on, headless, with no WAD needed.
#
#   test/keys-boot.sh [ps2|uhci|xhci|all]          (default: all)
#
# The arrow cluster is what a game binds to movement and the one thing the
# character ring cannot carry, so it is what this checks: the kernel console's
# `keys` command is started over the serial line, the four arrows are injected
# with the monitor's `sendkey`, and the log has to come back with the 0xE0-
# prefixed set-1 make codes (E0 48/50/4b/4d) and a release for each.
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
    mon() { echo "$1" | socat - "unix-connect:$d/mon.sock" >/dev/null 2>&1; }

    sleep "$BOOT_WAIT"
    printf 'keys\n' >&9            # the kernel console's raw-scancode dump
    sleep 3
    local k
    for k in up down left right; do mon "sendkey $k"; sleep 0.6; done
    mon "sendkey esc"              # `keys` stops on Esc
    sleep 2
    mon quit; sleep 1
    exec 9>&-
    kill $qpid $spid 2>/dev/null; wait $qpid $spid 2>/dev/null
    rm -f "$d"/*.sock "$d"/in.fifo

    # What came out, and whether every arrow made it through press *and*
    # release with the 0xE0 prefix that tells it from the numeric keypad.
    grep -a '^keys: E0' "$d/serial.log" | sed 's/^/    /'
    local sc miss=
    for sc in 48 50 4b 4d; do
        grep -qa "^keys: E0 $sc press"   "$d/serial.log" || miss="$miss E0-$sc-press"
        grep -qa "^keys: E0 $sc release" "$d/serial.log" || miss="$miss E0-$sc-release"
    done
    if [ -n "$miss" ]; then
        echo "    FAIL ($name): missing$miss"
        echo "    -> $d/serial.log"
        fail=1
    else
        echo "    PASS ($name): all four arrows, press and release"
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
