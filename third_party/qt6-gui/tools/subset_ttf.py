#!/usr/bin/env python3
"""Subset a TrueType (glyf) font to a set of Unicode ranges. Pure Python, no
dependencies. Used to cut GNU Unifont (11 MB, 57k glyphs) down to the few
hundred glyphs the Qt demo app uses, so it fits the boot RAM disk and the repo.

usage: subset_ttf.py in.ttf out.ttf [U+0020-007E ...]

Keeps: glyf/loca, hmtx/hhea, maxp, head, OS/2, name, gasp; writes a fresh
format-4 cmap (Windows Unicode BMP) and a 'post' table without glyph names.
Composite glyphs pull in their components. Layout tables (GDEF/GPOS/GSUB) are
dropped: Unifont has no real shaping features, and HarfBuzz falls back to cmap.
"""
import struct, sys

def u16(b, o): return struct.unpack_from(">H", b, o)[0]
def i16(b, o): return struct.unpack_from(">h", b, o)[0]
def u32(b, o): return struct.unpack_from(">I", b, o)[0]

def read_tables(f):
    n = u16(f, 4)
    return {f[12+16*i:16+16*i].decode(): (u32(f, 20+16*i), u32(f, 24+16*i)) for i in range(n)}

def cmap_lookup(f, tab):
    off, _ = tab["cmap"]
    n = u16(f, off+2)
    best = None
    for i in range(n):
        pid, eid, so = u16(f, off+4+8*i), u16(f, off+6+8*i), u32(f, off+8+8*i)
        fmt = u16(f, off+so)
        if (pid, eid) == (3, 10) and fmt == 12: best = (so, 12)
        elif (pid, eid) == (3, 1) and fmt == 4 and best is None: best = (so, 4)
    so, fmt = best
    m = {}
    base = off + so
    if fmt == 12:
        ng = u32(f, base+12)
        for g in range(ng):
            s, e, gid = struct.unpack_from(">III", f, base+16+12*g)
            for c in range(s, e+1): m[c] = gid + (c - s)
    else:
        segx2 = u16(f, base+6); sc = segx2 // 2
        ends = struct.unpack_from(">%dH" % sc, f, base+14)
        starts = struct.unpack_from(">%dH" % sc, f, base+16+segx2)
        deltas = struct.unpack_from(">%dh" % sc, f, base+16+2*segx2)
        ro_off = base+16+3*segx2
        ros = struct.unpack_from(">%dH" % sc, f, ro_off)
        for k in range(sc):
            for c in range(starts[k], ends[k]+1):
                if c == 0xFFFF: continue
                if ros[k] == 0: gid = (c + deltas[k]) & 0xFFFF
                else:
                    p = ro_off + 2*k + ros[k] + 2*(c - starts[k])
                    gid = u16(f, p)
                    if gid: gid = (gid + deltas[k]) & 0xFFFF
                if gid: m[c] = gid
    return m

def parse_ranges(args):
    cps = set()
    for a in args:
        a = a.upper().replace("U+", "")
        lo, _, hi = a.partition("-")
        for c in range(int(lo, 16), int(hi or lo, 16)+1): cps.add(c)
    return cps

def checksum(b):
    b = b + b"\0" * (-len(b) % 4)
    return sum(struct.unpack(">%dI" % (len(b)//4), b)) & 0xFFFFFFFF

def main():
    src, dst = sys.argv[1], sys.argv[2]
    wanted = parse_ranges(sys.argv[3:])
    f = open(src, "rb").read()
    tab = read_tables(f)
    def T(name): o, l = tab[name]; return f[o:o+l]
    head = bytearray(T("head")); long_loca = i16(head, 50)
    nglyphs = u16(T("maxp"), 4)
    loca_o, _ = tab["loca"]; glyf_o, _ = tab["glyf"]
    def gl_range(g):
        if long_loca: a, b = u32(f, loca_o+4*g), u32(f, loca_o+4*(g+1))
        else: a, b = 2*u16(f, loca_o+2*g), 2*u16(f, loca_o+2*(g+1))
        return a, b
    cmap = cmap_lookup(f, tab)
    gids = {0}
    for c in wanted:
        if c in cmap: gids.add(cmap[c])
    # pull in composite components
    todo = list(gids)
    while todo:
        g = todo.pop(); a, b = gl_range(g)
        if b - a < 10 or i16(f, glyf_o+a) >= 0: continue
        p = glyf_o + a + 10
        while True:
            flags, comp = u16(f, p), u16(f, p+2); p += 4
            if comp not in gids: gids.add(comp); todo.append(comp)
            p += 4 if flags & 1 else 2
            p += 8 if flags & 0x80 else 4 if flags & 0x40 else 2 if flags & 8 else 0
            if not flags & 0x20: break
    order = sorted(gids)                     # new gid = index in this list
    remap = {g: i for i, g in enumerate(order)}
    glyf = bytearray(); loca = []
    for g in order:
        a, b = gl_range(g); data = bytearray(f[glyf_o+a:glyf_o+b])
        if len(data) >= 10 and struct.unpack_from(">h", data, 0)[0] < 0:   # remap components
            p = 10
            while True:
                flags, comp = struct.unpack_from(">HH", data, p)
                struct.pack_into(">H", data, p+2, remap[comp]); p += 4
                p += 4 if flags & 1 else 2
                p += 8 if flags & 0x80 else 4 if flags & 0x40 else 2 if flags & 8 else 0
                if not flags & 0x20: break
        loca.append(len(glyf)); glyf += data; glyf += b"\0" * (-len(glyf) % 4)
    loca.append(len(glyf))
    nh = u16(T("hhea"), 34); hm_o, _ = tab["hmtx"]
    hmtx = bytearray()
    for g in order:
        adv = u16(f, hm_o+4*g) if g < nh else u16(f, hm_o+4*(nh-1))
        lsb = i16(f, hm_o+4*g+2) if g < nh else i16(f, hm_o+4*nh+2*(g-nh))
        hmtx += struct.pack(">Hh", adv, lsb)
    hhea = bytearray(T("hhea")); struct.pack_into(">H", hhea, 34, len(order))
    maxp = bytearray(T("maxp")); struct.pack_into(">H", maxp, 4, len(order))
    struct.pack_into(">h", head, 50, 1)           # long loca
    struct.pack_into(">I", head, 8, 0)            # checkSumAdjustment, patched below
    # cmap format 4, BMP only
    pairs = sorted((c, remap[cmap[c]]) for c in wanted if c in cmap and c < 0xFFFF)
    segs = []
    for c, g in pairs:
        if segs and c == segs[-1][1] + 1 and g == segs[-1][3] + (c - segs[-1][0]): segs[-1][1] = c
        else: segs.append([c, c, 0, g])
    segs.append([0xFFFF, 0xFFFF, 0, 0])
    sc = len(segs)
    ends = [s[1] for s in segs]; starts = [s[0] for s in segs]
    deltas = [((s[3] - s[0]) & 0xFFFF) if s[0] != 0xFFFF else 1 for s in segs]
    p2 = 1
    while p2 * 2 <= sc: p2 *= 2
    sr = 2 * p2; es = p2.bit_length() - 1
    sub = struct.pack(">HHHHHHH", 4, 16 + 8*sc, 0, sc*2, sr, es, 2*sc - sr)
    sub += struct.pack(">%dH" % sc, *ends) + b"\0\0" + struct.pack(">%dH" % sc, *starts)
    sub += struct.pack(">%dH" % sc, *deltas) + b"\0\0" * sc
    sub = sub[:2] + struct.pack(">H", len(sub)) + sub[4:]
    cmap_t = struct.pack(">HH", 0, 1) + struct.pack(">HHI", 3, 1, 12) + sub
    post = struct.pack(">IIhhIIIII", 0x00030000, 0, *struct.unpack_from(">hh", T("post"), 8), 0, 0, 0, 0, 0)
    out_tables = {"head": bytes(head), "hhea": bytes(hhea), "maxp": bytes(maxp), "OS/2": T("OS/2"),
                  "hmtx": bytes(hmtx), "cmap": cmap_t, "loca": struct.pack(">%dI" % len(loca), *loca),
                  "glyf": bytes(glyf), "name": T("name"), "post": post}
    if "gasp" in tab: out_tables["gasp"] = T("gasp")
    names = sorted(out_tables)
    n = len(names); p2 = 1
    while p2 * 2 <= n: p2 *= 2
    hdr = struct.pack(">IHHHH", 0x00010000, n, p2*16, p2.bit_length()-1, n*16 - p2*16)
    off = 12 + 16*n; recs = b""; body = b""
    for nm in names:
        d = out_tables[nm]
        recs += struct.pack(">4sIII", nm.encode(), checksum(d), off + len(body), len(d))
        body += d + b"\0" * (-len(d) % 4)
    font = bytearray(hdr + recs + body)
    adj = (0xB1B0AFBA - checksum(bytes(font))) & 0xFFFFFFFF
    ho = next(struct.unpack_from(">III", font, 12+16*i+4)[1] for i, nm in enumerate(names) if nm == "head")
    struct.pack_into(">I", font, ho + 8, adj)
    open(dst, "wb").write(font)
    print("%s: %d glyphs, %d codepoints mapped, %d bytes" % (dst, len(order), len(pairs), len(font)))

main()
