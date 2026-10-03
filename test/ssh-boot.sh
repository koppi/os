#!/bin/bash
# ssh-boot.sh — log into the in-kernel SSH server with a stock OpenSSH client
# and check its shell's line editor, headless.
#
#   test/ssh-boot.sh
#
# The session's keystrokes go in as the bytes a real terminal sends, which is
# the point: the shell in ssh.c does its own line editing (no PTY is
# negotiated), so a cursor key arrives as ESC [ A and used to leave `[A` in the
# command -- the ESC was dropped for not being printable and the two bytes
# after it were kept. Each case below asserts on the *output* of a line that
# could only be built with a working cursor.
#
# Needs ssh and sshpass on the host, plus SLIRP port forwarding (host 2222 ->
# guest 22, the same as `make qemu-iso` sets up). Output in $OUT.
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/ssh-boot}
rm -rf "$OUT"; mkdir -p "$OUT"
BOOT_WAIT=${BOOT_WAIT:-90}     # seconds to wait for the ssh listener
KVM=${KVM:--enable-kvm}
PORT=${PORT:-2222}

for t in qemu-system-i386 ssh sshpass; do
    command -v "$t" >/dev/null || { echo "ssh-boot: need $t"; exit 1; }
done
[ -f os.iso ] || { echo "ssh-boot: no os.iso -- run 'make iso' first"; exit 1; }

mkfifo "$OUT/in.fifo"
qemu-system-i386 -m 512M -smp 2 -no-reboot -vga std $KVM \
    -drive file=os.iso,if=ide,index=1,media=cdrom -boot d,menu=off \
    -display none -serial "file:$OUT/serial.log" \
    -netdev user,id=n0,hostfwd=tcp::$PORT-:22 -device e1000,netdev=n0 &
QPID=$!

# The server comes up on the net thread, after DHCP.
for i in $(seq "$BOOT_WAIT"); do
    [ -f "$OUT/serial.log" ] && grep -qa 'ssh: listening' "$OUT/serial.log" && break
    sleep 1
done
if ! grep -qa 'ssh: listening' "$OUT/serial.log" 2>/dev/null; then
    echo "    FAIL: the guest never printed 'ssh: listening'"
    echo "    -> $OUT/serial.log"
    kill $QPID 2>/dev/null; wait $QPID 2>/dev/null
    exit 1
fi
sleep 3

# -T: no PTY, which is what this server supports -- it answers a `shell`
# request and edits the line itself.
sshpass -p os ssh -p "$PORT" -T \
    -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    -o PreferredAuthentications=password -o PubkeyAuthentication=no \
    -o LogLevel=ERROR -o ConnectTimeout=20 \
    koppi@127.0.0.1 < "$OUT/in.fifo" > "$OUT/ssh.log" 2>&1 &
SPID=$!
exec 9>"$OUT/in.fifo"
t() { printf '%s' "$1" >&9; sleep 1.2; }

sleep 6
t 'echo abcd'; t $'\033[D'; t $'\033[D'; t 'X'; t $'\n'      # insert mid-line
t 'cho hi';    t $'\033[H'; t 'e';            t $'\n'        # Home
t $'\033[A';   t $'\033[F'; t '!';            t $'\n'        # Up (history) + End
t 'echo 12';   t $'\033[3~'; t 'Z';           t $'\n'        # Delete: no key of
                                                             # its own, swallowed
t 'echo pqrs'; t $'\033[D'; t $'\033[D'; t $'\177'; t $'\n'  # backspace mid-line
t 'echo sp';   t $'\033'; sleep 1; t '[D'; t 'A'; t $'\n'    # split across packets
t 'exit'; t $'\n'
sleep 3
exec 9>&-
kill $SPID $QPID 2>/dev/null; wait $SPID $QPID 2>/dev/null
rm -f "$OUT/in.fifo"

echo "=== ssh shell line editor"
# The transcript repaints the line with CRs, so turn every CR into a newline:
# each repaint and each command's output then stands on its own line and the
# matches below can be exact -- `hi` must not be satisfied by `hi!`.
tr '\r' '\n' < "$OUT/ssh.log" > "$OUT/ssh.lines"
miss=
for want in abXcd hi 'hi!' 12Z prs sAp; do
    if grep -qxa "$want" "$OUT/ssh.lines"; then printf '    %-6s ok\n' "$want"
    else printf '    %-6s MISSING\n' "$want"; miss="$miss $want"; fi
done
if [ -n "$miss" ]; then
    echo "    FAIL: missing$miss"
    echo "    -> $OUT/ssh.log  (session transcript)"
    exit 1
fi
echo "    PASS: cursor keys, Home/End, history, and a sequence split across packets"
