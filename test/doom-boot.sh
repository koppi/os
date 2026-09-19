#!/bin/bash
# doom-boot.sh — boot os.iso headless and drive apps/doom through QEMU, so the
# port can be checked without sitting in front of a display.
#
# The shell is reached over the serial console (uart.c feeds serial RX into the
# console input ring), and keystrokes are injected with the monitor's `sendkey`,
# which is how the game's own input path — raw scancodes, make *and* break —
# gets exercised. Every step leaves a PNG behind.
#
#   test/doom-boot.sh [demo|menu|play|quit|timedemo|noiwad|sound|all]
#                                                              (default: all)
#
# Needs a WAD staged on the RAM disk (apps/doom/PORTING.md) for everything
# except `noiwad`. Output, one subdirectory per scenario, in $OUT.
#
# The `sound` scenario points QEMU's audio backend at a WAV file instead of a
# speaker, so the sound path can be checked the same way as the video one:
# what came out is a file you can measure (and listen to).
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/doom-boot}
mkdir -p "$OUT"
BOOT_WAIT=${BOOT_WAIT:-14}     # seconds from power-on to a usable shell prompt
VGA=${VGA:-virtio}             # virtio | std (std gives a 24-bpp 800x600 mode)
KVM=${KVM:--enable-kvm}

for t in qemu-system-i386 socat; do
    command -v "$t" >/dev/null || { echo "doom-boot: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "doom-boot: no os.iso -- run 'make iso' first"; exit 1; }

# Audio: normally off (a headless run has nothing to play to). The `sound`
# scenario sets this to capture the codec's output to a WAV.
AUDIO=()

# run <name> <command line> <script>
#
# <script> is one "delay:action" per line, applied in order after the command
# has been sent: key:<k> taps a key, hold:<k> holds it ~400 ms (long enough for
# the engine to see several tics of it), shot:<n> screenshots, serial:<text>
# types a line at the shell.
run() {
    local name=$1 cmd=$2 script=$3
    local d="$OUT/$name"
    rm -rf "$d"; mkdir -p "$d"
    echo "=== $name: $cmd"

    qemu-system-i386 -vga "$VGA" -m 512M -no-reboot -smp 4 $KVM \
        -rtc base=localtime,clock=vm \
        -drive file=os.iso,if=ide,index=1,media=cdrom \
        -boot d,menu=off -display none "${AUDIO[@]}" \
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
    printf '%s\n' "$cmd" >&9

    local step delay rest kind arg
    while IFS= read -r step; do
        [ -z "$step" ] && continue
        delay=${step%%:*}; rest=${step#*:}
        kind=${rest%%:*}; arg=${rest#*:}
        sleep "$delay"
        case "$kind" in
            key)    mon "sendkey $arg" ;;
            hold)   mon "sendkey $arg 400" ;;
            serial) printf '%s\n' "$arg" >&9 ;;
            shot)   mon "screendump $d/$arg.ppm"; sleep 1
                    [ -f "$d/$arg.ppm" ] && command -v pnmtopng >/dev/null \
                        && pnmtopng "$d/$arg.ppm" > "$d/$arg.png" 2>/dev/null ;;
        esac
    done <<< "$script"

    mon quit
    sleep 1
    exec 9>&-
    kill $qpid $spid 2>/dev/null; wait $qpid $spid 2>/dev/null
    rm -f "$d"/*.sock "$d"/in.fifo

    # Strip the kernel's timestamped log lines: what matters here is what the
    # game printed.
    sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$d/serial.log" \
        | grep -vE '^\[ *[0-9]+\.[0-9]+\]' | grep -v '^$' | tail -"${TAIL:-12}"
    echo "    -> $d"
    echo
}

# The attract-mode demo: proves the WAD loaded and frames are reaching the
# framebuffer at the right scale.
do_demo() {
    run demo doom $'6:shot:early\n10:shot:demo'
}

# Escape opens the menu, the arrows move the skull: the raw-scancode path,
# including the 0xE0-prefixed keys the ASCII ring cannot carry.
do_menu() {
    run menu doom $'8:key:esc\n1:shot:menu\n1:key:down\n1:key:down\n2:shot:moved'
}

# Held movement keys: a make with no break until the hold expires, which is the
# thing a press-only keyboard ring gets wrong.
do_play() {
    run play 'doom -warp 1 1 -skill 2' \
        $'8:shot:start\n1:hold:up\n1:hold:up\n1:hold:up\n1:shot:forward\n1:hold:right\n1:hold:right\n2:shot:turned'
}

# Quit from the menu, then use the shell: the screen has to come back and the
# keyboard has to return to the console.
do_quit() {
    run quit doom \
        $'8:key:esc\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:ret\n2:shot:prompt\n1:key:y\n3:shot:desktop\n2:serial:ls /rd\n3:shot:shell'
}

# Doom's own benchmark. It ends in I_Error, so the exit status is -1 by design.
do_timedemo() {
    run timedemo 'doom -timedemo demo1' $'25:shot:done'
}

# No IWAD: the error has to be readable, which means the screen must never have
# been grabbed.
do_noiwad() {
    run noiwad 'doom -iwad /rd/nope.wad' $'6:shot:error'
}

# Sound. Record the codec's output to a WAV and report what is in it: silence
# while the machine boots (the module is muted from boot), then the demo's
# gunfire once the game has the PCM stream, then the module again after the
# game gives it back.
do_sound() {
    local d="$OUT/sound"
    AUDIO=(-audiodev "wav,id=snd0,path=$d/capture.wav"
           -device intel-hda -device hda-output,audiodev=snd0)
    run sound doom \
        $'16:key:esc\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:ret\n2:key:y\n3:serial:sound on\n6:shot:desktop'
    AUDIO=()
    wav_report "$d/capture.wav"
}

# Peak and RMS per second of a 16-bit stereo WAV: enough to see that audio
# came out, when, and that it is not a constant tone or a wall of clipping.
wav_report() {
    local f=$1
    [ -f "$f" ] || { echo "    no capture written"; return; }
    command -v python3 >/dev/null || { echo "    capture: $(du -h "$f" | cut -f1) (install python3 for a breakdown)"; return; }
    python3 - "$f" <<'PY'
import sys, wave, array
w = wave.open(sys.argv[1], 'rb')
n, rate, ch = w.getnframes(), w.getframerate(), w.getnchannels()
a = array.array('h'); a.frombytes(w.readframes(n))
print(f"    {n} frames, {rate} Hz, {ch} ch")
print("    sec  peak    rms  pan")
for s in range(n // rate):
    seg = a[s*rate*ch:(s+1)*rate*ch]
    if not seg:
        break
    pk = max(max(seg), -min(seg))
    rms = int((sum(x*x for x in seg) / len(seg)) ** 0.5)
    pan = "-"
    if ch == 2:
        l, r = seg[0::2], seg[1::2]
        pan = "stereo" if any(x != y for x, y in zip(l, r)) else "mono"
    print(f"    {s:3d} {pk:6d} {rms:6d}  {pan}")
PY
}

case "${1:-all}" in
    demo)     do_demo ;;
    menu)     do_menu ;;
    play)     do_play ;;
    quit)     do_quit ;;
    timedemo) do_timedemo ;;
    noiwad)   do_noiwad ;;
    sound)    do_sound ;;
    all)      do_demo; do_menu; do_play; do_quit; do_timedemo; do_noiwad
              do_sound ;;
    *)        echo "usage: $0 [demo|menu|play|quit|timedemo|noiwad|sound|all]"
              exit 1 ;;
esac
