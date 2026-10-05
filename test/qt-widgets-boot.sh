#!/bin/bash
# qt-widgets-boot.sh — boot os.iso headless and drive apps/hello-qt-widgets (the
# real Qt 6.8 QtWidgets test app: tabs, buttons, sliders, combo boxes, ...)
# through QEMU's monitor, and judge what it did.
#
# Same arrangement as qt-gui-boot.sh: the shell is reached over the serial
# console, the program is started by typing its name, keystrokes go through the
# monitor's `sendkey` (raw scancodes, which is what the Qt platform glue reads)
# and the mouse through `mouse_move` / `mouse_button` (relative PS/2 packets,
# which the glue turns into an absolute pointer inside its 640x400 screen).
#
# Unlike a pixel test, behaviour is judged from the app's own log: every state
# change is written to the serial console as "widgets: <what happened>", so a
# pass means the *right widget reacted to the right input*. Pixels are checked
# only for what a log cannot say: that the window is on screen, that an open
# combo popup really draws, and that the desktop is back afterwards.
#
#   test/qt-widgets-boot.sh [start|keys|sliders|input|combos|mouse|esc|all]
#
# Needs apps/hello-qt-widgets built and staged:
#   make -C apps/hello-qt-widgets -j4 && make iso
# Output, one subdirectory per scenario, in $OUT.
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/qt-widgets-boot}
mkdir -p "$OUT"
BOOT_WAIT=${BOOT_WAIT:-15}     # seconds from power-on to a usable shell prompt
VGA=${VGA:-virtio}             # virtio | std
KVM=${KVM:--enable-kvm}        # set KVM= to run under TCG
SMP=${SMP:-4}
APP=hqtwid

for t in qemu-system-i386 socat mdir python3; do
    command -v "$t" >/dev/null || { echo "qt-widgets-boot: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "qt-widgets-boot: no os.iso -- run 'make iso' first"; exit 1; }
mdir -i initrd.img ::$APP >/dev/null 2>&1 \
    || { echo "qt-widgets-boot: $APP is not on the RAM disk -- make -C apps/hello-qt-widgets -j4 && make iso"; exit 1; }

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
    """A QtWidgets window: 640x400 frame, light background, dark text on it."""
    ok = True
    x0, y0, x1, y1 = bbox_nonblack(img)
    w, h = x1 - x0 + 1, y1 - y0 + 1
    good = 600 <= w <= 660 and 380 <= h <= 420
    print(f"    frame bbox {w}x{h} at ({x0},{y0}) (want ~640x400): {'ok' if good else 'FAIL'}")
    ok &= good
    n = light = dark = 0
    for y in range(y0, y1, 2):
        for x in range(x0, x1, 2):
            r, g, b = px(img, x, y); s = r + g + b; n += 1
            light += s > 600; dark += s < 240
    lf, df = light / max(n, 1), dark / max(n, 1)
    good = lf > 0.55
    print(f"    light pixels {lf*100:.0f}% of frame (a Fusion window is mostly light grey): {'ok' if good else 'FAIL'}")
    ok &= good
    good = 0.004 < df < 0.25
    print(f"    dark pixels {df*100:.1f}% of frame (text, outlines; want 0.4-25%): {'ok' if good else 'FAIL'}")
    ok &= good
    return ok, (x0, y0, x1, y1)

def changed(a, b, box, thr=90):
    x0, y0, x1, y1 = box; n = 0
    for y in range(y0, y1, 2):
        for x in range(x0, x1, 2):
            pa, pb = px(a, x, y), px(b, x, y)
            if abs(pa[0]-pb[0]) + abs(pa[1]-pb[1]) + abs(pa[2]-pb[2]) > thr: n += 1
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
elif kind == "differs":                  # window_checks on both, and >= N pixels changed
    b = ppm(sys.argv[3]); want = int(sys.argv[4])
    ok1, box = window_checks(a); ok2, _ = window_checks(b)
    n = changed(a, b, box)
    good = n >= want
    print(f"    {n} pixels differ between the two frames (want >= {want}): {'ok' if good else 'FAIL'}")
    ok = ok1 and ok2 and good
elif kind == "same":                     # two frames, at most N pixels apart
    b = ppm(sys.argv[3]); want = int(sys.argv[4])
    box = bbox_nonblack(a)
    n = changed(a, b, box)
    good = n <= want
    print(f"    {n} pixels differ between the two frames (want <= {want}): {'ok' if good else 'FAIL'}")
    ok = good
elif kind == "desktop":
    ok = desktop_checks(a)
sys.exit(0 if ok else 1)
PY
}

FAILED=0
# The frame the app draws sits centred in the QEMU display; the pointer the glue
# tracks starts in the middle of it. PX/PY mirror that pointer so a scenario can
# say "move to x,y" in frame coordinates.
PX=320; PY=200

# run <name> <script>  -- starts $APP; <script> is one "delay:action" per line,
# applied in order after the command is sent:
#   key:<qemu key name>      one key press       (key:tab, key:spc, key:shift-tab)
#   type:<text>              a run of characters (letters, digits, space)
#   move:<x>,<y>             move the pointer to frame coordinates x,y
#   down / up / click        left button
#   drag:<x>,<y>             press here, move to x,y, release
#   shot:<n>                 screendump -> $OUT/<name>/<n>.ppm
#   serial:<text>            a line to the shell
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
    # Relative moves in chunks the PS/2 packet format (+-255) and the guest can
    # take; each chunk gets a moment to be consumed so none are coalesced away.
    rel() {
        local dx=$1 dy=$2 sx sy
        while [ "$dx" -ne 0 ] || [ "$dy" -ne 0 ]; do
            sx=$dx; [ "$sx" -gt 60 ] && sx=60; [ "$sx" -lt -60 ] && sx=-60
            sy=$dy; [ "$sy" -gt 60 ] && sy=60; [ "$sy" -lt -60 ] && sy=-60
            mon "mouse_move $sx $sy"; sleep 0.08
            dx=$((dx - sx)); dy=$((dy - sy))
        done
    }
    goto() { rel $(( $1 - PX )) $(( $2 - PY )); PX=$1; PY=$2; sleep 0.15; }
    press() { mon "mouse_button 1"; sleep 0.15; }
    release() { mon "mouse_button 0"; sleep 0.25; }
    typestr() {
        local s=$1 i c
        for ((i = 0; i < ${#s}; i++)); do
            c=${s:i:1}
            case "$c" in
                " ") mon "sendkey spc" ;;
                [a-z0-9]) mon "sendkey $c" ;;
                [A-Z]) mon "sendkey shift-${c,,}" ;;
            esac
            sleep 0.15
        done
    }

    sleep "$BOOT_WAIT"
    printf '%s\n' "$APP" >&9
    PX=320; PY=200

    local step delay rest kind arg
    while IFS= read -r step; do
        [ -z "$step" ] && continue
        delay=${step%%:*}; rest=${step#*:}
        kind=${rest%%:*}; arg=${rest#*:}
        [ "$kind" = "$rest" ] && arg=
        sleep "$delay"
        case "$kind" in
            key)    mon "sendkey $arg"; sleep 0.2 ;;
            type)   typestr "$arg" ;;
            move)   goto "${arg%,*}" "${arg#*,}" ;;
            down)   press ;;
            up)     release ;;
            click)  press; release ;;
            drag)   press; goto "${arg%,*}" "${arg#*,}"; release ;;
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
    sed -i 's/\x1b\[[0-9;]*[a-zA-Z]//g; s/\r//g' "$d/serial.log"
}

verdict() {   # verdict <label> <rc>
    if [ "$2" -eq 0 ]; then echo "  PASS  $1"; else echo "  FAIL  $1"; FAILED=1; fi
    echo
}

# The app's own words: serial.log lines ending in "widgets: <text>", in order.
# expect <scenario> <description> <line>...   every line must appear, in this order
#                                             (other lines may come between them)
# has    <scenario> <description> <line>...   every line must appear somewhere
# Both tolerate kernel log text before the marker, never after it.
expect() { _wlog ordered "$@"; }
has()    { _wlog any "$@"; }
_wlog() {
    local mode=$1 d="$OUT/$2" what=$3; shift 3
    python3 - "$mode" "$d/serial.log" "$what" "$@" <<'PY'
import sys
mode, path, what, *want = sys.argv[1:]
lines = [l.rstrip() for l in open(path, errors="replace")]
at, ok = 0, True
for w in want:
    pos = [i for i in range(at if mode == "ordered" else 0, len(lines)) if lines[i].endswith("widgets: " + w)]
    if not pos:
        print(f"    {what}: no 'widgets: {w}'" + (f" after log line {at+1}" if mode == "ordered" else "") + ": FAIL")
        ok = False
        break
    if mode == "ordered":
        at = pos[0] + 1
if ok:
    print(f"    {what}: ok")
sys.exit(0 if ok else 1)
PY
}

# follows <scenario> <description> <name> <min> <max>   the LAST "<name> N" the app
# logged has N in [min, max]
lastval() {
    python3 - "$OUT/$1/serial.log" "$2" "$3" "$4" "$5" <<'PY'
import sys, re
path, what, name, lo, hi = sys.argv[1:]
vals = [int(m.group(1)) for l in open(path, errors="replace")
        for m in [re.search(r"widgets: " + name + r" (-?\d+)$", l.rstrip())] if m]
good = bool(vals) and int(lo) <= vals[-1] <= int(hi)
print(f"    {what}: last {name} = {vals[-1] if vals else 'none'} (want {lo}..{hi}): {'ok' if good else 'FAIL'}")
sys.exit(0 if good else 1)
PY
}

# Every control shows one value, so the last number of each kind must agree.
agree() {
    python3 - "$OUT/$1/serial.log" "$2" <<'PY'
import sys, re
path, what = sys.argv[1:]
last = {}
for l in open(path, errors="replace"):
    m = re.search(r"widgets: (slider|spinbox|dial|scrollbar) (-?\d+)$", l.rstrip())
    if m: last[m.group(1)] = int(m.group(2))
good = len(last) == 4 and len(set(last.values())) == 1
print(f"    {what}: last values {last} (want all four equal): {'ok' if good else 'FAIL'}")
sys.exit(0 if good else 1)
PY
}

# The frame positions of the controls (window-frame pixels, 640x400, pointer starts at 320,200).
TAB_BUTTONS="51,26";  TAB_SLIDERS="131,26"; TAB_INPUT="203,26"; TAB_ABOUT="266,26"
BTN_CLICK="140,69";   BTN_TOGGLE="140,112"; BTN_TOOL="320,112";  BTN_CHECK="30,151"
RADIO_CAREFUL="400,144"; BTN_QUIT="589,377"
SLIDER_HANDLE_28="219,96"; SLIDER_TO_50="300,96"; SPIN_UP="156,221"
EDIT="300,60"; STYLE="300,112"; FRUIT="300,142"
FRUIT_CHERRY="300,191"; STYLE_WINDOWS="300,89"; STYLE_FUSION_BELOW="300,136"

# The window comes up: right shape, light background, text drawn, the app says it is ready.
do_start() {
    run start $'5:shot:up'
    local rc=0 d="$OUT/start"
    analyse window "$d/up.ppm" || rc=1
    expect start "app reports ready with the Fusion style and 4 tabs" "ready style=fusion tabs=4" || rc=1
    verdict "start" $rc
}

# Keyboard only: Tab walks the focus chain, Space presses, arrows move within a radio group.
do_keys() {
    run keys $'5:key:tab\n1:key:spc\n1:key:spc\n1:key:tab\n1:key:spc\n1:key:tab\n1:key:spc\n1:key:tab\n1:key:spc\n1:key:tab\n1:key:down\n1:key:down\n1:shot:end\n1:key:tab'
    local rc=0 d="$OUT/keys"
    analyse window "$d/end.ppm" || rc=1
    expect keys "Tab/Space through the buttons" \
        "focus QPushButton 'Click me'" "button clicked 1" "button clicked 2" \
        "focus QPushButton 'Toggle me'" "toggle on" \
        "focus QToolButton 'Tool button'" "toolbutton on" \
        "focus QCheckBox 'Enable the thing'" "checkbox on" \
        "focus QRadioButton 'Fast'" "focus QRadioButton 'Balanced'" "radio Careful" "focus QRadioButton 'Careful'" \
        "focus QPushButton 'Quit'" || rc=1
    verdict "keys" $rc
}

# Sliders tab: keyboard moves the focused slider, the dial/scroll bar/spin box/progress bar
# follow; a mouse drag moves the handle; a click on the spin box arrow steps it.
do_sliders() {
    run sliders $'5:move:'"$TAB_SLIDERS"$'\n1:click\n2:shot:tab\n1:key:tab\n1:key:right\n1:key:right\n1:key:right\n1:shot:keys\n1:move:'"$SLIDER_HANDLE_28"$'\n1:drag:'"$SLIDER_TO_50"$'\n1:shot:dragged\n1:move:'"$SPIN_UP"$'\n1:click\n1:shot:stepped'
    local rc=0 d="$OUT/sliders"
    analyse window "$d/tab.ppm" || rc=1
    analyse differs "$d/tab.ppm" "$d/dragged.ppm" 500 || rc=1
    expect sliders "tab switch, Tab to the slider, the arrow keys move it" "tab 1" "focus QSlider" "slider 26" "slider 27" "slider 28" || rc=1
    has sliders "dial, scroll bar and spin box followed to 28" "dial 28" "scrollbar 28" "spinbox 28" || rc=1
    agree sliders "after the drag and the spin step, all four controls agree" || rc=1
    lastval sliders "dragged from 28 to ~50, then +1 from the spin box arrow" slider 46 56 || rc=1
    verdict "sliders" $rc
}

# Input tab: typing, the cursor keys, Backspace, Home, Ctrl+A (QKeySequence bindings).
do_input() {
    run input $'5:move:'"$TAB_INPUT"$'\n1:click\n1:move:'"$EDIT"$'\n1:click\n1:type:hello world\n1:shot:typed\n1:key:left\n1:key:backspace\n1:key:home\n1:type:x\n1:key:ctrl-a\n1:type:z\n1:shot:replaced'
    local rc=0 d="$OUT/input"
    analyse differs "$d/typed.ppm" "$d/replaced.ppm" 100 || rc=1
    expect input "typing and editing keys" "tab 2" "focus QLineEdit" \
        "text 'hello world'" "text 'hello word'" "text 'xhello word'" "text 'z'" || rc=1
    verdict "input" $rc
}

# The combo-box popups, with the mouse and with Esc, and the run-time style switch.
do_combos() {
    run combos $'5:move:'"$TAB_INPUT"$'\n1:click\n1:shot:closed\n1:move:'"$FRUIT"$'\n1:click\n1:shot:popup\n1:move:'"$FRUIT_CHERRY"$'\n1:click\n1:key:down\n1:move:'"$FRUIT"$'\n1:shot:damson\n1:click\n1:key:esc\n1:shot:escaped\n1:move:'"$STYLE"$'\n1:click\n1:move:'"$STYLE_WINDOWS"$'\n1:click\n1:shot:windows\n1:move:'"$STYLE"$'\n1:click\n1:move:'"$STYLE_FUSION_BELOW"$'\n1:click\n1:shot:fusion'
    local rc=0 d="$OUT/combos"
    analyse differs "$d/closed.ppm" "$d/popup.ppm" 4000 || rc=1
    analyse differs "$d/closed.ppm" "$d/windows.ppm" 400 || rc=1
    analyse same "$d/escaped.ppm" "$d/damson.ppm" 1500 || rc=1
    expect combos "Fruit popup: pick with the mouse, then with the arrow keys" \
        "tab 2" "fruit 2 'Cherry'" "fruit 3 'Damson'" || rc=1
    expect combos "Style popup: Fusion -> Windows -> Fusion" \
        "combo 0 'Windows'" "combo 1 'Fusion'" || rc=1
    verdict "combos" $rc
}

# Mouse only on the Buttons tab, then the Quit button: the program exits with status 0
# and the desktop is back.
do_mouse() {
    run mouse $'5:shot:before\n1:move:'"$BTN_CLICK"$'\n1:click\n1:click\n1:move:'"$BTN_TOGGLE"$'\n1:click\n1:move:'"$BTN_TOOL"$'\n1:click\n1:move:'"$BTN_CHECK"$'\n1:click\n1:move:'"$RADIO_CAREFUL"$'\n1:click\n1:shot:after\n1:move:'"$BTN_QUIT"$'\n1:click\n3:shot:desktop\n1:serial:ls /rd\n3:shot:shell'
    local rc=0 d="$OUT/mouse"
    analyse differs "$d/before.ppm" "$d/after.ppm" 800 || rc=1
    analyse desktop "$d/desktop.ppm" || rc=1
    expect mouse "mouse clicks hit the right controls" \
        "button clicked 1" "button clicked 2" "toggle on" "toolbutton on" "checkbox on" "radio Careful" "quit" || rc=1
    grep -q "Process returned with exit code 0" "$d/serial.log" \
        && echo "    program exited with status 0: ok" || { echo "    program exited with status 0: FAIL"; rc=1; }
    grep -q "hqtwid" "$d/serial.log" && grep -q "font" "$d/serial.log" \
        && echo "    shell answers 'ls /rd' after the app: ok" || { echo "    shell answers 'ls /rd' after the app: FAIL"; rc=1; }
    verdict "mouse" $rc
}

# Esc through the raw-scancode path ends the program with status 0 and hands the screen back.
do_esc() {
    run esc $'5:shot:before\n1:key:esc\n3:shot:after'
    local rc=0 d="$OUT/esc"
    analyse window "$d/before.ppm" || rc=1
    analyse desktop "$d/after.ppm" || rc=1
    expect esc "Esc quits" "quit" || rc=1
    grep -q "Process returned with exit code 0" "$d/serial.log" \
        && echo "    program exited with status 0: ok" || { echo "    program exited with status 0: FAIL"; rc=1; }
    verdict "esc" $rc
}

case "${1:-all}" in
    start)   do_start ;;
    keys)    do_keys ;;
    sliders) do_sliders ;;
    input)   do_input ;;
    combos)  do_combos ;;
    mouse)   do_mouse ;;
    esc)     do_esc ;;
    all)     do_start; do_keys; do_sliders; do_input; do_combos; do_mouse; do_esc ;;
    *)       echo "usage: $0 [start|keys|sliders|input|combos|mouse|esc|all]"; exit 1 ;;
esac
echo "output in $OUT"
exit $FAILED
