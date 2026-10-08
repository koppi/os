#!/usr/bin/env python3
"""usbpad.py -- a USB game controller for QEMU, spoken over the usbredir protocol.

QEMU has no emulated gamepad. It does have `usb-redir`, which takes a USB device
from the other end of a socket, so this script *is* that other end: it connects
to a QEMU chardev, announces a controller, answers the guest's enumeration
(descriptors, SET_CONFIGURATION, the HID report descriptor) and then sends
interrupt-IN reports on command. The guest cannot tell it from hardware, so the
whole driver -- host controller, class driver, descriptor parser, `getpad` --
runs for real.

    qemu-system-i386 ... \\
        -chardev socket,id=pad,path=/tmp/pad.sock,server=on,wait=off \\
        -device usb-redir,chardev=pad
    test/usbpad.py --sock /tmp/pad.sock --profile ds4 < commands

Commands, one per line on stdin (blank lines and #comments are ignored):

    set lx=-1 ly=0 rx=0 ry=0 lt=0 rt=1 hat=ne buttons=1,3
        change the pad's state and send a report. Sticks run -1..1 with +y down
        (like the kernel's), triggers 0..1, hat is n/ne/e/se/s/sw/w/nw/none, and
        buttons are the numbers `pad` prints on the console. Unmentioned
        fields keep their value.
    tap N [ms]     press button N for ms (default 150), then release it
    sleep SECONDS  wait before running the next command
    unplug / plug  pull the pad out of the port / push it back in
    raw HEX        send these bytes as an interrupt report, as they are
    quit

Profiles (--profile):

    generic   a plain DirectInput pad: Game Pad, four 8-bit axes, a hat, 12
              buttons, no report IDs. The common cheap pad.
    ds4       a DualShock 4 shaped one: report ID 1, 64-byte reports, trigger
              axes on Rx/Ry, a descriptor of ~450 bytes with vendor collections
              and feature reports around the part that matters.
    xinput    an Xbox 360 wired controller: vendor class 0xFF/0x5D/0x01, a fixed
              20-byte report, no HID anything.

--speed full|high picks the link speed it announces (EHCI only takes high).
"""
import argparse
import os
import select
import socket
import struct
import sys
import time

# ---- usbredir protocol (usbredirproto.h; 32-bit ids, i.e. no 64bits_ids cap) --
T_HELLO, T_DEVICE_CONNECT, T_DEVICE_DISCONNECT, T_RESET = 0, 1, 2, 3
T_INTERFACE_INFO, T_EP_INFO = 4, 5
T_SET_CONFIGURATION, T_GET_CONFIGURATION, T_CONFIGURATION_STATUS = 6, 7, 8
T_SET_ALT_SETTING, T_GET_ALT_SETTING, T_ALT_SETTING_STATUS = 9, 10, 11
T_START_INTERRUPT, T_STOP_INTERRUPT, T_INTERRUPT_STATUS = 15, 16, 17
T_CONTROL_PACKET, T_INTERRUPT_PACKET = 100, 103

CAP_CONNECT_DEVICE_VERSION = 1
CAP_EP_INFO_MAX_PACKET_SIZE = 4
CAP_64BITS_IDS = 5
CAP_32BITS_BULK_LENGTH = 6

ST_SUCCESS, ST_STALL = 0, 4
SPEED = {"low": 0, "full": 1, "high": 2}

HDR32 = struct.Struct("<III")                      # type, length, id
HDR64 = struct.Struct("<IIQ")                      # ... once both sides have 64bits_ids

# ---- the pads -----------------------------------------------------------------

HAT_DIRS = {"n": 0, "ne": 1, "e": 2, "se": 3, "s": 4, "sw": 5, "w": 6, "nw": 7}


def axis8(v):
    """-1..1 -> 0..255 with 128 at rest, the way a real pad's ADC reads."""
    return max(0, min(255, int(round((v + 1.0) * 127.5))))


def item(prefix, *data):
    return bytes([prefix]) + bytes(data)


GENERIC_RDESC = bytes([
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,         # Generic Desktop / Game Pad / Application
    0x15, 0x00, 0x26, 0xFF, 0x00,               # logical 0..255
    0x75, 0x08, 0x95, 0x04,                     # 4 x 8 bits
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35,   # X Y Z Rz
    0x81, 0x02,
    0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14,   # 0..7, degrees
    0x75, 0x04, 0x95, 0x01, 0x09, 0x39, 0x81, 0x42,         # hat, null state
    0x65, 0x00,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x03,                     # 4 bits padding
    0x05, 0x09, 0x19, 0x01, 0x29, 0x0C,                     # buttons 1..12
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0C, 0x81, 0x02,
    0x75, 0x01, 0x95, 0x04, 0x81, 0x03,                     # 4 bits padding
    0xC0,
])


def ds4_rdesc():
    d = bytearray([
        0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
        0x85, 0x01,                                         # Report ID 1
        0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35,     # X Y Z Rz = LX LY RX RY
        0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02,
        0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00,     # hat, 8 = centred
        0x46, 0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42,
        0x65, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0E,     # 14 buttons
        0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0E, 0x81, 0x02,
        0x06, 0x00, 0xFF, 0x09, 0x20, 0x75, 0x06, 0x95, 0x01,    # vendor counter
        0x15, 0x00, 0x25, 0x7F, 0x81, 0x02,
        0x05, 0x01, 0x09, 0x33, 0x09, 0x34,                 # Rx Ry = L2 R2
        0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
        0x06, 0x00, 0xFF, 0x09, 0x21, 0x95, 0x36, 0x81, 0x02,    # 54 vendor bytes
        0x85, 0x05, 0x09, 0x22, 0x95, 0x1F, 0x91, 0x02,     # output report 5
    ])
    # feature reports, then a vendor application collection of more of them:
    # the parts of a real descriptor that make it long and that mean nothing here
    for rid in range(0x02, 0x1E):
        d += bytes([0x85, rid, 0x09, 0x24, 0x95, 0x24, 0xB1, 0x02])
    d += bytes([0xC0, 0x06, 0xF0, 0xFF, 0x09, 0x40, 0xA1, 0x01])
    for rid in range(0xF0, 0xF8):
        d += bytes([0x85, rid, 0x09, 0x47, 0x95, 0x3F, 0xB1, 0x02])
    d += bytes([0xC0])
    return bytes(d)


class Pad:
    """Shared state and the report each profile builds from it."""

    def __init__(self):
        self.lx = self.ly = self.rx = self.ry = 0.0
        self.lt = self.rt = 0.0
        self.hat = None
        self.buttons = set()

    def hat_value(self, neutral):
        return HAT_DIRS[self.hat] if self.hat in HAT_DIRS else neutral


class Generic(Pad):
    name = "generic"
    vid, pid = 0x0079, 0x0011          # a very common clone's ids
    rdesc = GENERIC_RDESC
    ep_in, ep_mps = 0x81, 8
    ep_out = None
    xinput = False

    def report(self):
        b = sum(1 << (n - 1) for n in self.buttons if 1 <= n <= 12)
        return bytes([axis8(self.lx), axis8(self.ly), axis8(self.rx), axis8(self.ry),
                      self.hat_value(15) & 0x0F, b & 0xFF, (b >> 8) & 0x0F])


class DS4(Pad):
    name = "ds4"
    vid, pid = 0x054C, 0x09CC
    rdesc = ds4_rdesc()
    ep_in, ep_out, ep_mps = 0x84, 0x03, 64
    xinput = False
    counter = 0

    def report(self):
        # buttons 1..4 sit in the high nibble beside the hat, 5..12 in the next
        # byte, 13..14 in the low bits of the one after (with a counter above)
        b = sum(1 << (n - 1) for n in self.buttons if 1 <= n <= 14)
        DS4.counter = (DS4.counter + 1) & 0x3F
        r = bytearray(64)
        r[0] = 1
        r[1:5] = bytes([axis8(self.lx), axis8(self.ly), axis8(self.rx), axis8(self.ry)])
        r[5] = (self.hat_value(8) & 0x0F) | ((b & 0x0F) << 4)
        r[6] = (b >> 4) & 0xFF
        r[7] = ((b >> 12) & 0x03) | (DS4.counter << 2)
        r[8] = int(round(self.lt * 255))
        r[9] = int(round(self.rt * 255))
        return bytes(r)


class XInput(Pad):
    name = "xinput"
    vid, pid = 0x045E, 0x028E
    rdesc = None
    ep_in, ep_out, ep_mps = 0x81, 0x01, 32
    xinput = True

    def report(self):
        lo = hi = 0
        dirs = {"n": 0x01, "ne": 0x09, "e": 0x08, "se": 0x0A, "s": 0x02,
                "sw": 0x06, "w": 0x04, "nw": 0x05}
        lo |= dirs.get(self.hat, 0)
        for n, (byte, bit) in {1: ("hi", 0x10), 2: ("hi", 0x20), 3: ("hi", 0x40),
                               4: ("hi", 0x80), 5: ("hi", 0x01), 6: ("hi", 0x02),
                               9: ("lo", 0x20), 10: ("lo", 0x10), 11: ("lo", 0x40),
                               12: ("lo", 0x80), 13: ("hi", 0x04)}.items():
            if n in self.buttons:
                if byte == "lo":
                    lo |= bit
                else:
                    hi |= bit
        lt = 1.0 if 7 in self.buttons else self.lt      # buttons 7/8: the triggers
        rt = 1.0 if 8 in self.buttons else self.rt

        def s16(v):
            return max(-32768, min(32767, int(round(v * 32767))))

        # the pad's y axes grow upwards; the state's grow down
        return struct.pack("<BBBBBBhhhh6x", 0, 20, lo, hi,
                           int(round(lt * 255)), int(round(rt * 255)),
                           s16(self.lx), s16(-self.ly), s16(self.rx), s16(-self.ry))


PROFILES = {"generic": Generic, "ds4": DS4, "xinput": XInput}


# ---- USB descriptors ------------------------------------------------------------

def device_descriptor(pad):
    cls = 0xFF if pad.xinput else 0
    return struct.pack("<BBHBBBBHHHBBBB", 18, 1, 0x0200, cls, 0, 0, 64,
                       pad.vid, pad.pid, 0x0100, 0, 0, 0, 1)


def config_descriptor(pad, speed):
    # bInterval: full speed counts frames, high speed is 2^(n-1) microframes
    interval = 4 if speed == "high" else 8
    iface = struct.pack("<BBBBBBBBB", 9, 4, 0, 0, 2 if pad.ep_out else 1,
                        0xFF if pad.xinput else 3,
                        0x5D if pad.xinput else 0, 1 if pad.xinput else 0, 0)
    if pad.xinput:
        # the vendor descriptor an Xbox 360 pad puts here: type 0x21 like a HID
        # descriptor, but not one -- a walker that trusts the type byte reads a
        # report-descriptor length out of it
        mid = bytes.fromhex("11210001012581140000000013010800" "00")
    else:
        mid = struct.pack("<BBHBBBH", 9, 0x21, 0x0111, 0, 1, 0x22, len(pad.rdesc))
    eps = struct.pack("<BBBBHB", 7, 5, pad.ep_in, 3, pad.ep_mps, interval)
    if pad.ep_out:
        eps += struct.pack("<BBBBHB", 7, 5, pad.ep_out, 3, pad.ep_mps, interval)
    body = iface + mid + eps
    return struct.pack("<BBHBBBBB", 9, 2, 9 + len(body), 1, 1, 0, 0x80, 50) + body


# ---- the redirect endpoint --------------------------------------------------------

class Redir:
    def __init__(self, sock, pad, speed, verbose):
        self.s = sock
        self.pad = pad
        self.speed = speed
        self.verbose = verbose
        self.rx = b""
        self.hdr = HDR32       # hello is always 32-bit; switched once both sides say 64
        self.config = 0
        self.streaming = False
        self.plugged = False
        self.got_hello = False
        self.stall_idle = False

    def log(self, *a):
        if self.verbose:
            print("usbpad:", *a, file=sys.stderr, flush=True)

    def send(self, typ, payload=b"", pid=0):
        self.s.sendall(self.hdr.pack(typ, len(payload), pid) + payload)

    # What this end offers. QEMU's xHCI refuses a redirected device unless the
    # far end has the last three (it needs 64-bit ids and 32-bit bulk lengths to
    # drive SuperSpeed streams); UHCI and EHCI ask for nothing.
    CAPS = ((1 << CAP_CONNECT_DEVICE_VERSION) | (1 << CAP_EP_INFO_MAX_PACKET_SIZE) |
            (1 << CAP_64BITS_IDS) | (1 << CAP_32BITS_BULK_LENGTH))

    def hello(self):
        self.send(T_HELLO, b"usbpad.py".ljust(64, b"\0") + struct.pack("<I", self.CAPS))

    def plug(self):
        pad = self.pad
        n = 2 if pad.ep_out else 1
        info = struct.pack("<I", 1)
        info += bytes([0]).ljust(32, b"\0")                                  # interface
        info += bytes([0xFF if pad.xinput else 3]).ljust(32, b"\0")          # class
        info += bytes([0x5D if pad.xinput else 0]).ljust(32, b"\0")          # subclass
        info += bytes([1 if pad.xinput else 0]).ljust(32, b"\0")             # protocol
        self.send(T_INTERFACE_INFO, info)

        # endpoint tables are indexed ep & 0x0F, plus 16 for IN endpoints
        typ = bytearray([255] * 32)
        ival = bytearray(32)
        ifc = bytearray(32)
        mps = [0] * 32
        typ[0] = typ[16] = 0                                                 # control
        mps[0] = mps[16] = 64
        idx = (pad.ep_in & 0x0F) | 16
        typ[idx], ival[idx], mps[idx] = 3, 8, pad.ep_mps
        if pad.ep_out:
            idx = pad.ep_out & 0x0F
            typ[idx], ival[idx], mps[idx] = 3, 8, pad.ep_mps
        self.send(T_EP_INFO, bytes(typ) + bytes(ival) + bytes(ifc) +
                  struct.pack("<32H", *mps))
        self.send(T_DEVICE_CONNECT, struct.pack("<BBBBHHH", SPEED[self.speed],
                  0xFF if pad.xinput else 0, 0, 0, pad.vid, pad.pid, 0x0100))
        self.plugged = True
        self.log("plugged", pad.name, self.speed)

    def unplug(self):
        if self.plugged:
            self.send(T_DEVICE_DISCONNECT)
            self.plugged = False
            self.streaming = False
            self.log("unplugged")

    def report(self, data=None):
        if not (self.plugged and self.streaming):
            return
        data = self.pad.report() if data is None else data
        self.send(T_INTERRUPT_PACKET,
                  struct.pack("<BBH", self.pad.ep_in, ST_SUCCESS, len(data)) + data)
        self.log("report", data.hex())

    # -- control transfers ----------------------------------------------------
    def control(self, pid, ep, req, rtype, value, index, length, data):
        pad = self.pad
        self.log("control ep=%02x type=%02x req=%02x value=%04x index=%04x len=%d" %
                 (ep, rtype, req, value, index, length))
        status, out = ST_SUCCESS, b""
        if rtype == 0x80 and req == 6:                      # GET_DESCRIPTOR (device)
            dt = value >> 8
            if dt == 1:
                out = device_descriptor(pad)
            elif dt == 2:
                out = config_descriptor(pad, self.speed)
            else:
                status = ST_STALL                           # strings, qualifiers, ...
        elif rtype == 0x81 and req == 6 and not pad.xinput:  # GET_DESCRIPTOR (interface)
            dt = value >> 8
            if dt == 0x22:
                out = pad.rdesc
            elif dt == 0x21:
                out = struct.pack("<BBHBBBH", 9, 0x21, 0x0111, 0, 1, 0x22, len(pad.rdesc))
            else:
                status = ST_STALL
        elif rtype == 0x21 and req in (0x0A, 0x0B):          # SET_IDLE / SET_PROTOCOL
            status = ST_STALL if self.stall_idle else ST_SUCCESS
        elif rtype == 0x80 and req == 0:                    # GET_STATUS
            out = b"\0\0"
        else:
            self.log("control: stalling", hex(rtype), hex(req), hex(value))
            status = ST_STALL
        out = out[:length]
        self.send(T_CONTROL_PACKET,
                  struct.pack("<BBBBHHH", ep, req, rtype, status, value, index,
                              len(out)) + out, pid)

    # -- the receive side -----------------------------------------------------
    def feed(self, chunk):
        self.rx += chunk
        while len(self.rx) >= self.hdr.size:
            typ, length, pid = self.hdr.unpack_from(self.rx)
            n = self.hdr.size
            if len(self.rx) < n + length:
                return
            body = self.rx[n:n + length]
            self.rx = self.rx[n + length:]
            self.handle(typ, pid, body)

    def handle(self, typ, pid, body):
        if typ == T_HELLO:
            self.got_hello = True
            self.log("hello from", body[:64].rstrip(b"\0").decode(errors="replace"))
            peer = struct.unpack_from("<I", body, 64)[0] if len(body) >= 68 else 0
            if peer & self.CAPS & (1 << CAP_64BITS_IDS):
                self.hdr = HDR64            # every packet after the hello
                self.log("64-bit ids")
        elif typ == T_RESET:
            self.config = 0
            self.streaming = False
            self.log("reset")
        elif typ == T_SET_CONFIGURATION:
            self.config = body[0]
            self.send(T_CONFIGURATION_STATUS, bytes([ST_SUCCESS, self.config]), pid)
            self.log("configuration", self.config)
        elif typ == T_GET_CONFIGURATION:
            self.send(T_CONFIGURATION_STATUS, bytes([ST_SUCCESS, self.config]), pid)
        elif typ == T_SET_ALT_SETTING:
            self.send(T_ALT_SETTING_STATUS, bytes([ST_SUCCESS, body[0], body[1]]), pid)
        elif typ == T_GET_ALT_SETTING:
            self.send(T_ALT_SETTING_STATUS, bytes([ST_SUCCESS, body[0], 0]), pid)
        elif typ == T_START_INTERRUPT:
            self.send(T_INTERRUPT_STATUS, bytes([ST_SUCCESS, body[0]]), pid)
            if body[0] == self.pad.ep_in:
                self.streaming = True
                self.log("interrupt receiving on", hex(body[0]))
                self.report()                       # a real pad reports at once
        elif typ == T_STOP_INTERRUPT:
            self.send(T_INTERRUPT_STATUS, bytes([ST_SUCCESS, body[0]]), pid)
            self.streaming = False
        elif typ == T_CONTROL_PACKET:
            ep, req, rtype, _st, value, index, length = struct.unpack_from("<BBBBHHH", body)
            self.control(pid, ep, req, rtype, value, index, length, body[10:])
        elif typ == T_INTERRUPT_PACKET:
            # host -> device on the OUT endpoint (an LED command, say): take it
            ep = body[0]
            self.send(T_INTERRUPT_PACKET, struct.pack("<BBH", ep, ST_SUCCESS, 0), pid)
        else:
            self.log("ignoring packet type", typ)


# ---- command interpreter --------------------------------------------------------

def apply_set(pad, args):
    for a in args:
        k, _, v = a.partition("=")
        if k in ("lx", "ly", "rx", "ry", "lt", "rt"):
            setattr(pad, k, float(v))
        elif k == "hat":
            pad.hat = v if v in HAT_DIRS else None
        elif k == "buttons":
            pad.buttons = {int(x) for x in v.split(",") if x}
        else:
            raise ValueError("unknown field " + k)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--sock", required=True, help="QEMU chardev unix socket to connect to")
    ap.add_argument("--profile", choices=sorted(PROFILES), default="generic")
    ap.add_argument("--speed", choices=("full", "high"), default="full")
    ap.add_argument("--stall-idle", action="store_true",
                    help="stall SET_IDLE / SET_PROTOCOL, as many pads do")
    ap.add_argument("--start-unplugged", action="store_true",
                    help="wait for a `plug` command instead of attaching at once")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    for _ in range(100):
        try:
            s.connect(args.sock)
            break
        except OSError:
            time.sleep(0.1)
    else:
        sys.exit("usbpad: cannot connect to " + args.sock)

    pad = PROFILES[args.profile]()
    r = Redir(s, pad, args.speed, args.verbose)
    r.stall_idle = args.stall_idle
    r.hello()
    plug_pending = not args.start_unplugged   # once the guest's hello says how to talk

    cmds = []          # lines waiting to run
    resume = 0.0       # no command runs before this time
    stdin_open = True
    buf = b""
    quitting = False
    while not quitting:
        now = time.monotonic()
        timeout = None
        if cmds:
            timeout = max(0.0, resume - now)
        rl = [s] + ([sys.stdin] if stdin_open else [])
        ready, _, _ = select.select(rl, [], [], timeout)
        if s in ready:
            chunk = s.recv(65536)
            if not chunk:
                break
            r.feed(chunk)
            if plug_pending and r.got_hello:
                plug_pending = False
                r.plug()
        if stdin_open and sys.stdin in ready:
            chunk = os.read(sys.stdin.fileno(), 4096)
            if not chunk:
                stdin_open = False
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.decode().split("#")[0].strip()
                if line:
                    cmds.append(line)
        while cmds and time.monotonic() >= resume:
            words = cmds.pop(0).split()
            op, rest = words[0], words[1:]
            try:
                if op == "set":
                    apply_set(pad, rest)
                    r.report()
                elif op == "tap":
                    n = int(rest[0])
                    ms = int(rest[1]) if len(rest) > 1 else 150
                    pad.buttons.add(n)
                    r.report()
                    cmds.insert(0, "untap %d" % n)
                    resume = time.monotonic() + ms / 1000.0
                elif op == "untap":
                    pad.buttons.discard(int(rest[0]))
                    r.report()
                elif op == "sleep":
                    resume = time.monotonic() + float(rest[0])
                elif op == "unplug":
                    r.unplug()
                elif op == "plug":
                    r.plug()
                elif op == "raw":
                    r.report(bytes.fromhex("".join(rest)))
                elif op == "quit":
                    quitting = True
                    break
                else:
                    print("usbpad: unknown command", op, file=sys.stderr)
            except (ValueError, IndexError) as e:
                print("usbpad: bad command %r: %s" % (op, e), file=sys.stderr)
        if not stdin_open and not cmds:
            # nothing more to say; keep serving the guest until QEMU hangs up
            pass


if __name__ == "__main__":
    main()
