#!/usr/bin/env python3
"""Derive and (optionally) copy the exact closure of real Qt/FreeType/HarfBuzz/
PCRE2/double-conversion files the koppios GUI app compiles, into third_party/qt6-gui/.

usage: vendor.py <scratchpad-with-built-tree> [--copy]

The scratch tree (qtbase clone + qtgen/include + qtguitest/{obj,hbobj,ftobj,pcre2obj,mocgen})
is produced by the bootstrap scripts (setup.sh + compile_all.sh). Every translation unit
that tree compiled is re-run through `g++ -MM` with the same flags; the union of the
dependencies, classified by where it lives, is the vendor set. Nothing is guessed.
"""
import os, sys, subprocess, shutil, json, concurrent.futures as cf

SCRATCH = os.path.abspath(sys.argv[1])
BOOT = os.path.dirname(os.path.abspath(__file__))        # tools/
COPY = "--copy" in sys.argv
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
DEST = os.path.join(REPO, "third_party", "qt6-gui")
QT = f"{SCRATCH}/qtbase"
QTGEN = f"{SCRATCH}/qtgen/include"
QTT = f"{SCRATCH}/qtguitest"
MOCGEN = f"{QTT}/mocgen"

def sh(cmd):
    return subprocess.run(["bash", "-c", f"source {BOOT}/compile_flags.sh {SCRATCH}; {cmd}"],
                          capture_output=True, text=True)

units = []   # (group, name, compiler-command-prefix, file)  -- cwd for pcre handled via 'cwd'

def add(group, name, kind, path, extra="", cwd=None):
    units.append(dict(group=group, name=name, kind=kind, path=path, extra=extra, cwd=cwd))

# ---- Qt corelib/gui + minimal plugin ----
for rel in open(f"{QTT}/all_candidate_files.txt").read().split():
    add("qt", os.path.basename(rel)[:-4], "cxx", f"{QT}/{rel}")
for rel in ["src/plugins/platforms/minimal/qminimalintegration.cpp",
            "src/plugins/platforms/minimal/qminimalbackingstore.cpp",
            "src/gui/platform/unix/qgenericunixeventdispatcher.cpp",
            "src/gui/platform/unix/qunixeventdispatcher.cpp"]:
    add("qt", os.path.basename(rel)[:-4], "cxx", f"{QT}/{rel}")
SIMD = {"qimagescale_sse4": "-msse4.1", "qdrawhelper_sse4": "-msse4.1", "qdrawhelper_ssse3": "-mssse3",
        "qdrawhelper_avx2": "-mavx2 -mbmi -mbmi2 -mf16c -mfma -mlzcnt -mpopcnt"}
for n, fl in SIMD.items():
    p = subprocess.run(["find", f"{QT}/src/gui", "-name", n + ".cpp"], capture_output=True, text=True).stdout.split()[0]
    add("qt", n, "cxx", p, fl)
add("qt", "qgrayraster", "qgray", f"{QT}/src/gui/painting/qgrayraster.c")
# ---- moc: orphan *.cpp compiled standalone (+ moc_qnamespace) ----
orph = set(open(f"{QTT}/moc_orphans.txt").read().split()) | {"moc_qnamespace.cpp"}
for m in sorted(orph):
    if os.path.exists(f"{QTT}/obj/{m[:-4]}.o"):
        add("moc", m[:-4], "cxx", f"{MOCGEN}/{m}")
# ---- koppios glue (authored files live in BOOT or lib/) ----
for src in ["qstandardpaths_koppios.cpp", "qshader_koppios.cpp", "qkoppiosfontdatabase.cpp", "qt_koppios_platform.cpp"]:
    add("koppios", src[:-4], "cxx", f"{DEST}/koppios/{src}")
for src in ["qfileengine_koppios.cpp", "qfsfileengine_koppios_stub.cpp"]:
    add("koppios", src[:-4], "cxx", f"{REPO}/lib/{src}")
# ---- the demo app itself: scanned for the headers it includes, not listed in sources.mk ----
add("app", "hello_qt_gui", "cxx", f"{REPO}/apps/hello-qt-gui/hello_qt_gui.cpp")
# ---- double-conversion ----
DC = f"{QT}/src/3rdparty/double-conversion/double-conversion"
for f in "bignum bignum-dtoa cached-powers double-to-string fast-dtoa fixed-dtoa strtod string-to-double".split():
    p = f"{QTT}/dc_patched/{f}.cc" if f == "string-to-double" else f"{DC}/{f}.cc"
    add("dc", f, "dc", p)
# ---- HarfBuzz ----
HB = f"{QT}/src/3rdparty/harfbuzz-ng/src"
for o in sorted(os.listdir(f"{QTT}/hbobj")):
    if not o.endswith(".o"): continue
    n = o[:-2]
    if n == "hb-dummy": add("hb", n, "hbd", f"{QT}/src/3rdparty/harfbuzz-ng/hb-dummy.cc")
    elif n == "VARC": add("hb", n, "hbv", f"{HB}/OT/Var/VARC/VARC.cc")
    else: add("hb", n, "hb", f"{HB}/{n}.cc")
# ---- PCRE2 ----
P2 = f"{QT}/src/3rdparty/pcre2/src"
for o in sorted(os.listdir(f"{QTT}/pcre2obj")):
    if o.endswith(".o"): add("pcre2", o[:-2], "pcre", f"{P2}/{o[:-2]}.c", cwd=P2)
# ---- FreeType ----
FTSRC = "autofit/autofit base/ftbase base/ftbbox base/ftbdf base/ftbitmap base/ftcid base/ftfstype base/ftgasp base/ftglyph base/ftgxval base/ftinit base/ftmm base/ftotval base/ftpatent base/ftpfr base/ftstroke base/ftsynth base/fttype1 base/ftwinfnt base/ftdebug psnames/psnames raster/raster sfnt/sfnt smooth/smooth truetype/truetype".split()
for r in FTSRC:
    add("ft", os.path.basename(r), "ft", f"{QT}/src/3rdparty/freetype/src/{r}.c")
for r in ["ftsystem_koppios", "ftgzip_koppios"]:
    add("ft", r, "ft", f"{DEST}/koppios/freetype/{r}.c")

# --------------------------------------------------------------------------------
HBDEFS = ("-DHAVE_ATEXIT -DHAVE_CONFIG_H -DHB_EXTERN= -DHB_NDEBUG -DHB_NO_UNICODE_FUNCS -DQT_NO_VERSION_TAGGING "
          "-DHAVE_PTHREAD -DHAVE_SCHED_H -DHAVE_SCHED_YIELD -DHAVE_OT")
FTFLAGS = (f"-m32 -Os -w -fno-builtin -fno-stack-protector -DFT2_BUILD_LIBRARY -DTT_CONFIG_OPTION_SUBPIXEL_HINTING "
           f"-I {DEST}/koppios/freetype -I {QT}/src/3rdparty/freetype/include")
def cmd_for(u):
    k, p, e = u["kind"], u["path"], u["extra"]
    if k == "cxx":  return f'g++ $CXXCOMMON {e} -MM {p}'
    if k == "qgray": return f'gcc -m32 -fno-builtin -DQT_BUILD_GUI_LIB -DQT_BUILD_CORE_LIB -DQT_DISABLE_DEPRECATED_UP_TO=QT_VERSION -include $QTGEN/QtCore/qglobal.h -include dirent.h -w $INCS -MM {p}'
    if k == "dc":   return f'g++ -m32 -std=gnu++17 -w -I {QT}/src/3rdparty/double-conversion -I {DC} -MM {p}'
    if k == "hb":   return f'g++ $CXXCOMMON {HBDEFS} -I {HB} -MM {p}'
    if k == "hbd":  return f'g++ $CXXCOMMON {HBDEFS} -I {HB} -I {QT}/src/3rdparty/harfbuzz-ng -MM {p}'
    if k == "hbv":  return f'g++ $CXXCOMMON {HBDEFS} -I {HB} -I {HB}/OT/Var/VARC -MM {p}'
    if k == "pcre": return f'gcc -m32 -w -DHAVE_CONFIG_H -DPCRE2_CODE_UNIT_WIDTH=16 -DPCRE2_STATIC -I. -MM {p}'
    if k == "ft":   return f'gcc {FTFLAGS} -MM {p}'
    raise SystemExit(k)

def run(u):
    cmd = cmd_for(u)
    if u["cwd"]: cmd = f'cd {u["cwd"]} && {cmd}'
    r = sh(cmd)
    if r.returncode != 0:
        return u, None, r.stderr[:400]
    txt = r.stdout.replace("\\\n", " ").split(":", 1)[1].split()
    return u, [os.path.normpath(os.path.join(u["cwd"] or ".", t)) for t in txt], ""

files = set(); bad = []
with cf.ThreadPoolExecutor(8) as ex:
    for u, deps, err in ex.map(run, units):
        if deps is None: bad.append((u["name"], err)); continue
        u["deps"] = deps
        files.update(deps)
if bad:
    for n, e in bad: print("DEP FAIL", n, e)
    sys.exit(1)

# classify
real, gen, moc, glue = set(), set(), set(), set()
for f in files:
    f = os.path.normpath(f)
    if f.startswith(QT + "/"): real.add(os.path.relpath(f, QT))
    elif f.startswith(QTGEN + "/"): gen.add(os.path.relpath(f, QTGEN))
    elif f.startswith(MOCGEN + "/"): moc.add(os.path.relpath(f, MOCGEN))
    elif f.startswith(QTT + "/dc_patched/"): glue.add(f)
    elif f.startswith((BOOT, REPO, DEST)): glue.add(f)
    else: print("OTHER", f)
# every unit's own source also counts
for u in units:
    f = os.path.normpath(u["path"])
    if f.startswith(QT + "/"): real.add(os.path.relpath(f, QT))
    elif f.startswith(MOCGEN + "/"): moc.add(os.path.relpath(f, MOCGEN))
# the generated include entries: symlinks may point at other include-tree entries
# (qpa/x_p.h -> private/x_p.h) or into qtbase; follow chains until everything is covered
todo = list(gen)
while todo:
    g = todo.pop()
    p = f"{QTGEN}/{g}"
    if not os.path.islink(p): continue
    nxt = os.path.normpath(os.path.join(os.path.dirname(p), os.readlink(p)))
    if nxt.startswith(QTGEN + "/"):
        rel = os.path.relpath(nxt, QTGEN)
        if rel not in gen: gen.add(rel); todo.append(rel)
    else:
        t = os.path.realpath(p)
        assert t.startswith(QT + "/"), (g, t)
        real.add(os.path.relpath(t, QT))
sz = sum(os.path.getsize(f"{QT}/{r}") for r in real if os.path.isfile(f"{QT}/{r}"))
sz_gen = sum(os.path.getsize(f"{QTGEN}/{g}") for g in gen if os.path.isfile(f"{QTGEN}/{g}") and not os.path.islink(f"{QTGEN}/{g}"))
sz_moc = sum(os.path.getsize(f"{MOCGEN}/{m}") for m in moc)
print(f"units: {len(units)}  real qtbase files: {len(real)} ({sz/1e6:.1f} MB)  include-tree entries: {len(gen)} "
      f"(generated {sz_gen/1e3:.0f} KB)  moc files: {len(moc)} ({sz_moc/1e6:.1f} MB)  glue: {len(glue)}")
by = {}
for r in real:
    top = "/".join(r.split("/")[:3]) if r.startswith("src/3rdparty") else "/".join(r.split("/")[:2])
    by[top] = by.get(top, 0) + os.path.getsize(f"{QT}/{r}")
for k, v in sorted(by.items(), key=lambda kv: -kv[1])[:14]: print(f"  {k:40s} {v/1e6:6.2f} MB")

json.dump(dict(units=[{k: v for k, v in u.items() if k != "deps"} for u in units],
               real=sorted(real), gen=sorted(gen), moc=sorted(moc)),
          open(f"{SCRATCH}/vendor_manifest.json", "w"), indent=1)
if not COPY: sys.exit(0)

# ============================== copy ==============================
import re, stat
# regenerate only the derived parts; koppios/*.cpp and koppios/freetype/ are hand-written and stay
for d in ("qtbase", "LICENSES", "koppios/include", "koppios/mocgen", "koppios/dc_patched"):
    shutil.rmtree(f"{DEST}/{d}", ignore_errors=True)
def cp(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)

# 1. real Qt/third-party files, original paths under qtbase/
for r in sorted(real):
    cp(f"{QT}/{r}", f"{DEST}/qtbase/{r}")
# licence/attribution files of every bundled third-party dir (not found by -MM)
for d in ["freetype", "harfbuzz-ng", "pcre2", "double-conversion", "siphash", "tinycbor"]:
    base = f"{QT}/src/3rdparty/{d}"
    for fn in os.listdir(base):
        if re.match(r"(LICEN[CS]E|COPYING|AUTHORS|THANKS|README|qt_attribution|BDF-|PCF-|ZLIB-)", fn) and os.path.isfile(f"{base}/{fn}"):
            cp(f"{base}/{fn}", f"{DEST}/qtbase/src/3rdparty/{d}/{fn}")
# 2. include trees (symlinks stay relative: same depth as in the scratch tree)
for g in sorted(gen):
    sp = f"{QTGEN}/{g}"; dp = f"{DEST}/koppios/include/{g}"
    os.makedirs(os.path.dirname(dp), exist_ok=True)
    if os.path.islink(sp):
        os.symlink(os.readlink(sp), dp)
        assert os.path.exists(dp), ("dangling", g, os.readlink(sp))
    else:
        shutil.copy2(sp, dp)
# 3. moc output
for m in sorted(moc):
    cp(f"{MOCGEN}/{m}", f"{DEST}/koppios/mocgen/{m}")
# 4. the one generated glue file: patched double-conversion copy
cp(f"{QTT}/dc_patched/string-to-double.cc", f"{DEST}/koppios/dc_patched/string-to-double.cc")
# 5. SPDX licence texts
for lic in ["LGPL-3.0-only", "GPL-2.0-only", "GPL-2.0-or-later", "GPL-3.0-only", "Unicode-3.0", "CC0-1.0", "FTL", "MIT",
            "BSD-2-Clause", "BSD-3-Clause", "Zlib", "LicenseRef-BSD-3-Clause-with-PCRE2-Binary-Like-Packages-Exception"]:
    cp(f"{QT}/LICENSES/{lic}.txt", f"{DEST}/LICENSES/{lic}.txt")

# 6. build description for the Makefile
def group(name): return [u for u in units if u["group"] == name]
def rel_src(u):
    p = os.path.normpath(u["path"])
    if p.startswith(QT + "/"): return "qtbase/" + os.path.relpath(p, QT)
    if p.startswith(MOCGEN + "/"): return "koppios/mocgen/" + os.path.relpath(p, MOCGEN)
    if p.startswith(QTT + "/dc_patched/"): return "koppios/dc_patched/" + os.path.basename(p)
    if p.startswith(f"{REPO}/lib/"): return "../../lib/" + os.path.basename(p)   # relative to third_party/qt6-gui
    if p.startswith(f"{DEST}/koppios/freetype/"): return "koppios/freetype/" + os.path.basename(p)
    if p.startswith(f"{DEST}/koppios/"): return "koppios/" + os.path.basename(p)
    raise SystemExit(("unmapped", p))
mk = ["# GENERATED by tools/vendor.py -- do not edit. Paths are relative to third_party/qt6-gui/.\n"]
for gname, var in [("qt", "QT_SRCS"), ("moc", "MOC_SRCS"), ("koppios", "KOPPIOS_SRCS"), ("dc", "DC_SRCS"),
                   ("hb", "HB_SRCS"), ("pcre2", "PCRE2_SRCS"), ("ft", "FT_SRCS")]:
    us = group(gname)
    paths = [rel_src(u) for u in us]
    assert len(paths) == len(set(paths)), ("duplicate source in", gname)   # objects are named by path, not basename
    plain = [u for u in us if not u["extra"] and u["kind"] in ("cxx", "dc", "hb", "pcre", "ft")]
    mk.append(f"{var} := \\\n" + " \\\n".join("  " + rel_src(u) for u in plain) + "\n\n")
simd = [u for u in group("qt") if u["extra"]]
# SIMD files are inside QT_SRCS above? no: they were filtered (extra != ""); list separately with flags
mk.append("SIMD_SRCS := \\\n" + " \\\n".join("  " + rel_src(u) for u in simd) + "\n")
for u in simd:
    mk.append(f"SIMD_FLAGS_{u['name']} := {u['extra']}\n")
mk.append("\n")
odd = {u["kind"]: u for u in units if u["kind"] in ("qgray", "hbd", "hbv")}
mk.append("QGRAY_SRC := " + rel_src(odd["qgray"]) + "\n")
mk.append("HB_DUMMY_SRC := " + rel_src(odd["hbd"]) + "\n")
mk.append("HB_VARC_SRC := " + rel_src(odd["hbv"]) + "\n")
open(f"{DEST}/koppios/sources.mk", "w").write("".join(mk))

# ordered -I list of the Qt compile, rewritten to vendored locations (keeps only dirs that exist)
r = sh('printf "%s\\n" $INCS')
inc = []
for tok in r.stdout.split():
    if tok == "-I": continue
    t = tok[2:] if tok.startswith("-I") else tok
    t = t.replace(QTGEN, "koppios/include").replace(f"{QTT}/mocgen", "koppios/mocgen").replace(QT, "qtbase")
    if t.startswith("/"): continue
    if os.path.isdir(f"{DEST}/{t}"): inc.append(t)
seen = set(); inc = [x for x in inc if not (x in seen or seen.add(x))]
open(f"{DEST}/koppios/incs.mk", "w").write("# GENERATED by tools/vendor.py. Ordered -I list for Qt translation units (relative to third_party/qt6-gui/).\nQT_INCDIRS := " + " \\\n  ".join(inc) + "\n")
print("copied. DEST size:", subprocess.run(["du", "-sh", DEST], capture_output=True, text=True).stdout.split()[0])
