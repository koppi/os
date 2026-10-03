#!/bin/bash
# chipnomad-boot.sh — boot os.iso headless and drive apps/chipnomad through
# QEMU, so the port can be checked without sitting in front of a display.
#
# Same arrangement as doom-boot.sh: the shell is reached over the serial
# console (uart.c feeds serial RX into the console input ring) and keystrokes
# are injected with the monitor's `sendkey`, which exercises the tracker's own
# input path — raw scancodes, make *and* break, which is what its chords and
# key repeat are built on. Every step leaves a PNG behind.
#
#   test/chipnomad-boot.sh [start|tour|browse|play|edit|all]   (default: all)
#
# Scenarios that need a song to work with stage one of the bundled demos as
# the autosave file, which is what the tracker opens on startup, into a copy
# of the ISO under $OUT. Output, one subdirectory per scenario, in $OUT.
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/chipnomad-boot}
mkdir -p "$OUT"
BOOT_WAIT=${BOOT_WAIT:-15}     # seconds from power-on to a usable shell prompt
VGA=${VGA:-virtio}
KVM=${KVM:--enable-kvm}

for t in qemu-system-i386 socat mcopy grub-mkrescue; do
    command -v "$t" >/dev/null || { echo "chipnomad-boot: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "chipnomad-boot: no os.iso -- run 'make iso' first"; exit 1; }

AUDIO=()
ISO=os.iso

# An ISO whose autosave.cnm is one of the bundled demo songs, so the tracker
# comes up with real material instead of an empty project. Built once.
DEMO_ISO="$OUT/with-song.iso"
make_demo_iso() {
    [ -f "$DEMO_ISO" ] && return
    local d="$OUT/.demo"
    rm -rf "$d"; mkdir -p "$d"
    cp -r iso "$d/iso"
    cp initrd.img "$d/initrd.img"
    mcopy -i "$d/initrd.img" -D o \
        third_party/chipnomad/tracker/packaging/common/projects/MICROEGGZ.cnm \
        ::autosave.cnm
    cp "$d/initrd.img" "$d/iso/boot/initrd.img"
    grub-mkrescue -o "$DEMO_ISO" "$d/iso" 1>&2 2>/dev/null
    rm -rf "$d"
}

# run <name> <command line> <script>
#
# <script> is one "delay:action" per line, applied in order after the command
# has been sent: key:<k> taps a key, hold:<k> holds it ~400 ms, shot:<n>
# screenshots, serial:<text> types a line at the shell.
run() {
    local name=$1 cmd=$2 script=$3
    local d="$OUT/$name"
    rm -rf "$d"; mkdir -p "$d"
    echo "=== $name: $cmd"

    qemu-system-i386 -vga "$VGA" -m 512M -no-reboot -smp 4 $KVM \
        -rtc base=localtime,clock=vm \
        -drive file="$ISO",if=ide,index=1,media=cdrom \
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
                    if [ -f "$d/$arg.ppm" ] && command -v pnmtopng >/dev/null; then
                        pnmtopng "$d/$arg.ppm" > "$d/$arg.png" 2>/dev/null && rm -f "$d/$arg.ppm"
                    fi ;;
        esac
    done <<< "$script"

    sleep 1
    exec 9>&-
    kill $qpid $spid 2>/dev/null
    wait $qpid 2>/dev/null
    grep -E "chipnomad:|Page fault|returned with error" "$d/serial.log" | sed 's/^/    /'
    ls "$d"/*.png 2>/dev/null | sed 's/^/    /'
}

# Peak and RMS per second of a 16-bit stereo WAV: enough to see that audio came
# out, when, and that it is music rather than a constant tone.
wav_report() {
    local f=$1
    [ -f "$f" ] || { echo "    no capture written"; return; }
    command -v python3 >/dev/null || { echo "    capture: $(du -h "$f" | cut -f1)"; return; }
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

# An empty project: the first-run path, with no autosave to read.
do_start() {
    ISO=os.iso
    run start "start /rd/cnomad" $'4:shot:01-song\n2:hold:esc\n2:shot:02-desktop'
}

# Every screen reachable with SHIFT + a direction.
do_tour() {
    make_demo_iso; ISO=$DEMO_ISO
    run tour "start /rd/cnomad" \
        $'4:shot:01-song\n1:key:shift-right\n2:shot:02-chain\n1:key:shift-right\n2:shot:03-phrase\n1:key:shift-left\n1:key:shift-left\n1:key:shift-up\n2:shot:04-project\n1:key:shift-down\n1:key:shift-down\n2:shot:05-settings\n1:hold:esc'
    ISO=os.iso
}

# The file browser over the kernel's listdir, and loading what it finds.
do_browse() {
    make_demo_iso; ISO=$DEMO_ISO
    run browse "start /rd/cnomad" \
        $'4:key:shift-up\n2:key:x\n3:shot:01-browser\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:shot:02-selected\n1:key:x\n3:shot:03-loaded\n1:hold:esc'
    ISO=os.iso
}

# Playback, with the codec's output captured to a WAV instead of a speaker.
do_play() {
    make_demo_iso; ISO=$DEMO_ISO
    local d="$OUT/play"
    mkdir -p "$d"
    AUDIO=(-audiodev "wav,id=snd0,path=$d/capture.wav"
           -device intel-hda -device hda-output,audiodev=snd0)
    run play "start /rd/cnomad" \
        $'4:shot:01-song\n1:key:spc\n6:shot:02-playing\n4:shot:03-playing\n1:key:spc\n1:hold:esc'
    AUDIO=()
    ISO=os.iso
    wav_report "$d/capture.wav"
}

# Edit, quit, and come back: the project and settings writers, through the
# whole-file spit syscall.
do_edit() {
    make_demo_iso; ISO=$DEMO_ISO
    run edit "start /rd/cnomad" \
        $'4:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:down\n1:key:x\n2:shot:01-edited\n1:hold:esc\n3:serial:cat /rd/settings.txt\n3:serial:start /rd/cnomad\n6:shot:02-reloaded\n1:hold:esc'
    ISO=os.iso
    echo "    settings.txt as written:"
    sed -n '/^screenWidth:/,/^wavetablePath:/p' "$OUT/edit/serial.log" | sed 's/^/      /'
}

case "${1:-all}" in
    start)  do_start ;;
    tour)   do_tour ;;
    browse) do_browse ;;
    play)   do_play ;;
    edit)   do_edit ;;
    all)    do_start; do_tour; do_browse; do_play; do_edit ;;
    *)      echo "usage: $0 [start|tour|browse|play|edit|all]"; exit 1 ;;
esac
