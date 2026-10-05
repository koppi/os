#!/bin/bash
# qt-gui-boot.sh — boot os.iso headless and drive apps/hello-qt-gui (the real
# Qt 6.8 "Hello, Qt6!" window) through QEMU, and judge the result from pixels.
#
# Same arrangement as doom-boot.sh: the shell is reached over the serial
# console, the program is started by typing its name, and keystrokes go through
# the monitor's `sendkey` (raw scancodes, which is what the app's own input
# poll reads). Unlike the other scripts this one decides pass/fail itself:
# every screenshot is analysed (pure Python, no imaging library) and the run
# exits non-zero if a check fails.
#
#   test/qt-gui-boot.sh [start|animate|soak|quit|all]          (default: all)
#
# Needs apps/hello-qt-gui built and staged:
#   make -C apps/hello-qt-gui -j4 && make iso
# Output, one subdirectory per scenario, in $OUT.
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/qt-gui-boot}
mkdir -p "$OUT"
BOOT_WAIT=${BOOT_WAIT:-15}     # seconds from power-on to a usable shell prompt
VGA=${VGA:-virtio}             # virtio | std
KVM=${KVM:--enable-kvm}        # set KVM= to run under TCG
SMP=${SMP:-4}

for t in qemu-system-i386 socat mdir python3; do
    command -v "$t" >/dev/null || { echo "qt-gui-boot: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "qt-gui-boot: no os.iso -- run 'make iso' first"; exit 1; }
mdir -i initrd.img ::hqtgui >/dev/null 2>&1 \
    || { echo "qt-gui-boot: hqtgui is not on the RAM disk -- make -C apps/hello-qt-gui -j4 && make iso"; exit 1; }

# analyse <check> <a.ppm> [<b.ppm>]  -- prints one line per finding, exit 0 = pass
analyse() {
    python3 - "$@" <<'PY'
import sys

def ppm(path):
    d = open(path, "rb").read()
    toks, i = [], 0
    while len(toks) < 4:                      # magic, w, h, maxval (comments allowed)
        while d[i:i+1].isspace(): i += 1
        if d[i:i+1] == b"#":
            while d[i:i+1] != b"\n": i += 1
            continue
        j = i
        while not d[j:j+1].isspace(): j += 1
        toks.append(d[i:j]); i = j
    assert toks[0] == b"P6", toks[0]
    w, h = int(toks[1]), int(toks[2])
    return w, h, d[i+1:i+1+w*h*3]

def px(img, x, y):
    w, h, d = img; o = (y*w + x)*3
    return d[o], d[o+1], d[o+2]

def bbox_nonblack(img, thr=24):
    w, h, d = img
    x0, y0, x1, y1 = w, h, -1, -1
    for y in range(0, h, 2):
        row = d[y*w*3:(y+1)*w*3]
        for x in range(0, w, 2):
            r, g, b = row[x*3], row[x*3+1], row[x*3+2]
            if r + g + b > thr:
                if x < x0: x0 = x
                if x > x1: x1 = x
                if y < y0: y0 = y
                if y > y1: y1 = y
    return x0, y0, x1, y1

def window_checks(img):
    ok = True
    x0, y0, x1, y1 = bbox_nonblack(img)
    w, h = x1 - x0 + 1, y1 - y0 + 1
    ratio = w / h if h else 0
    good = 1.55 < ratio < 1.65            # the window fills the 640x400 screen: 8:5
    print(f"    frame bbox {w}x{h} at ({x0},{y0}), aspect {ratio:.2f} (want ~1.60): {'ok' if good else 'FAIL'}")
    ok &= good
    # vertical gradient: bottom rows bluer/brighter than top rows
    def band(yy):
        n = tot = 0
        for y in range(yy, yy + max(2, h // 20)):
            for x in range(x0 + w // 10, x1 - w // 10, 3):
                r, g, b = px(img, x, y); tot += r + g + b; n += 1
        return tot / max(n, 1)
    top, bot = band(y0 + 2), band(y1 - max(2, h // 20) - 2)
    good = bot > top * 1.4
    print(f"    gradient top {top:.0f} -> bottom {bot:.0f} (want bottom clearly brighter): {'ok' if good else 'FAIL'}")
    ok &= good
    # text/outline: near-white pixels inside the frame
    white = sum(1 for y in range(y0, y1, 2) for x in range(x0, x1, 2) if min(px(img, x, y)) > 200)
    frac = white * 4 / (w * h)
    good = 0.003 < frac < 0.30
    print(f"    near-white (text, panel outline) {frac*100:.1f}% of frame (want 0.3-30%): {'ok' if good else 'FAIL'}")
    ok &= good
    return ok, (x0, y0, x1, y1)

def diff_pixels(a, b, box):
    x0, y0, x1, y1 = box; n = 0
    for y in range(y0, y1, 2):
        for x in range(x0, x1, 2):
            pa, pb = px(a, x, y), px(b, x, y)
            if abs(pa[0]-pb[0]) + abs(pa[1]-pb[1]) + abs(pa[2]-pb[2]) > 90: n += 1
    return n * 4

def desktop_checks(img):
    w, h, d = img
    x0, y0, x1, y1 = bbox_nonblack(img)
    bw, bh = x1 - x0 + 1, y1 - y0 + 1
    good = bw > w * 0.9 and bh > h * 0.9           # the desktop fills the screen again
    print(f"    non-black area {bw}x{bh} of {w}x{h} (desktop should fill the screen): {'ok' if good else 'FAIL'}")
    return good

kind = sys.argv[1]
a = ppm(sys.argv[2])
ok = True
if kind == "window":
    ok, _ = window_checks(a)
elif kind == "moved":
    b = ppm(sys.argv[3])
    ok1, box = window_checks(a); ok2, _ = window_checks(b)
    n = diff_pixels(a, b, box)
    good = n > 300
    print(f"    {n} pixels changed between frames (the ball moved; want > 300): {'ok' if good else 'FAIL'}")
    ok = ok1 and ok2 and good
elif kind == "desktop":
    ok = desktop_checks(a)
sys.exit(0 if ok else 1)
PY
}

FAILED=0
# run <name> <script>  -- starts hqtgui; <script> is one "delay:action" per line
# (key:<k>, shot:<n>, serial:<text>), applied in order after the command is sent.
run() {
    local name=$1 script=$2
    local d="$OUT/$name"
    rm -rf "$d"; mkdir -p "$d"
    echo "=== $name"

    qemu-system-i386 -vga "$VGA" -m 512M -no-reboot -smp "$SMP" $KVM \
        -rtc base=localtime,clock=vm \
        -drive file=os.iso,if=ide,index=1,media=cdrom \
        -boot d,menu=off -display none \
        -serial "unix:$d/ser.sock,server,nowait" \
        -monitor "unix:$d/mon.sock,server,nowait" &
    local qpid=$!

    sleep 2
    mkfifo "$d/in.fifo"
    socat "unix-connect:$d/ser.sock" - < "$d/in.fifo" > "$d/serial.log" 2>/dev/null &
    local spid=$!
    exec 9>"$d/in.fifo"

    mon() { echo "$1" | socat - "unix-connect:$d/mon.sock" >/dev/null 2>&1; }

    sleep "$BOOT_WAIT"
    printf '%s\n' "hqtgui" >&9

    local step delay rest kind arg
    while IFS= read -r step; do
        [ -z "$step" ] && continue
        delay=${step%%:*}; rest=${step#*:}
        kind=${rest%%:*}; arg=${rest#*:}
        sleep "$delay"
        case "$kind" in
            key)    mon "sendkey $arg" ;;
            serial) printf '%s\n' "$arg" >&9 ;;
            shot)   mon "screendump $d/$arg.ppm"; sleep 1
                    command -v pnmtopng >/dev/null && [ -f "$d/$arg.ppm" ] \
                        && pnmtopng "$d/$arg.ppm" > "$d/$arg.png" 2>/dev/null ;;
        esac
    done <<< "$script"

    mon quit
    sleep 1
    exec 9>&-
    kill $qpid $spid 2>/dev/null; wait $qpid $spid 2>/dev/null
    rm -f "$d"/*.sock "$d"/in.fifo
    sed -i 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$d/serial.log"
}

verdict() {   # verdict <label> <rc>
    if [ "$2" -eq 0 ]; then echo "  PASS  $1"; else echo "  FAIL  $1"; FAILED=1; fi
    echo
}

# The window comes up: right shape, gradient background, text drawn.
do_start() {
    run start $'5:shot:up'
    analyse window "$OUT/start/up.ppm"; verdict "start" $?
}

# The event loop is live: the QTimer keeps repainting, the ball keeps moving.
do_animate() {
    run animate $'5:shot:a\n1:shot:b'
    analyse moved "$OUT/animate/a.ppm" "$OUT/animate/b.ppm"; verdict "animate" $?
}

# A minute of animation: every frame hands big fills to Qt's GUI thread pool, and the kernel hands
# out a limited number of thread slots per process and never recycles them, so a pool that
# churned through threads (or hung waiting for one) would freeze the ball partway through.
do_soak() {
    run soak $'5:shot:early\n30:shot:mid\n30:shot:late\n1:key:esc\n3:shot:after'
    local rc=0 d="$OUT/soak"
    analyse moved "$d/early.ppm" "$d/mid.ppm" || rc=1
    analyse moved "$d/mid.ppm" "$d/late.ppm" || rc=1
    analyse desktop "$d/after.ppm" || rc=1
    if grep -q "Process returned with exit code 0" "$d/serial.log"; then
        echo "    program exited with status 0 after a minute: ok"
    else
        echo "    program exited with status 0 after a minute: FAIL"; rc=1
    fi
    verdict "soak" $rc
}

# Esc through the raw-scancode path ends the program with status 0, the screen
# is handed back to the desktop, and the shell works again.
do_quit() {
    run quit $'5:shot:before\n1:key:esc\n3:shot:after\n1:serial:ls /rd\n3:shot:shell'
    local rc=0 d="$OUT/quit"
    analyse window "$d/before.ppm" || rc=1
    analyse desktop "$d/after.ppm" || rc=1
    if grep -q "Process returned with exit code 0" "$d/serial.log"; then
        echo "    program exited with status 0: ok"
    else
        echo "    program exited with status 0: FAIL"; rc=1
    fi
    if grep -q "font" "$d/serial.log" && grep -q "hqtgui" "$d/serial.log"; then
        echo "    shell answers 'ls /rd' after the app: ok"
    else
        echo "    shell answers 'ls /rd' after the app: FAIL"; rc=1
    fi
    verdict "quit" $rc
}

case "${1:-all}" in
    start)   do_start ;;
    animate) do_animate ;;
    soak)    do_soak ;;
    quit)    do_quit ;;
    all)     do_start; do_animate; do_soak; do_quit ;;
    *)       echo "usage: $0 [start|animate|soak|quit|all]"; exit 1 ;;
esac
echo "output in $OUT"
exit $FAILED
