import os, re, sys

if len(sys.argv) < 2:
    print("usage: gen_camel.py <scratchpad-dir>")
    sys.exit(1)
SCRATCH = sys.argv[1]
QTGEN = os.path.join(SCRATCH, "qtgen/include")
QTBASE = os.path.join(SCRATCH, "qtbase")

# Only a REAL definition: class/struct NAME optionally followed by a base-class
# list, ending in '{' on the same or a continuation line -- never a bare
# forward declaration ('class Foo;'), which appears for the same name in many
# unrelated files and must never win over the file that actually defines it.
# A 'template <...>' line immediately before class/struct is skipped over --
# template classes (class QVarLengthArray, ...) split the template<> and
# class keywords across two lines as often as not.
cls_pat = re.compile(r'^\s*(?:class|struct)\s+(?:Q_[A-Z0-9_]*EXPORT\s+)?([A-Z][A-Za-z0-9_]*)')
fwd_pat = re.compile(r'^\s*(?:class|struct)\s+(?:Q_[A-Z0-9_]*EXPORT\s+)?([A-Z][A-Za-z0-9_]*)\s*;')
typedef_pat = re.compile(r'^\s*using\s+([A-Z][A-Za-z0-9_]*)\s*=')
template_pat = re.compile(r'^\s*template\s*<')

def scan(basedir, module):
    outdir = os.path.join(QTGEN, module)
    real = {}    # name -> header basename, from an actual definition
    weak = {}    # name -> header basename, from a bare forward declaration
    for root, dirs, files in os.walk(basedir):
        if '3rdparty' in root:
            continue
        for fn in files:
            if not fn.endswith('.h'):
                continue
            path = os.path.join(root, fn)
            try:
                with open(path, 'r', errors='ignore') as f:
                    lines = f.readlines()
            except IOError:
                continue
            i = 0
            n = len(lines)
            while i < n:
                line = lines[i]
                if template_pat.match(line):
                    # skip to the matching '>' (may span several lines for
                    # nested template parameter lists), then re-check the
                    # class/struct line right after it.
                    depth = line.count('<') - line.count('>')
                    j = i + 1
                    while depth > 0 and j < n:
                        depth += lines[j].count('<') - lines[j].count('>')
                        j += 1
                    i = j
                    if i >= n:
                        break
                    line = lines[i]

                m = cls_pat.match(line)
                if m and not fwd_pat.match(line):
                    name = m.group(1)
                    # find the '{' -- same line, or scan forward a few lines
                    # past a base-class list (': public Foo, private Bar')
                    k = i
                    found_brace = '{' in line
                    while not found_brace and k + 1 < n and k < i + 20:
                        k += 1
                        stripped = lines[k].lstrip()
                        if stripped.startswith('#'):
                            continue  # preprocessor conditionals in a base-class list
                        if ';' in lines[k] and '{' not in lines[k]:
                            break
                        if '{' in lines[k]:
                            found_brace = True
                    if found_brace and len(name) >= 2 and name[1].isupper():
                        real.setdefault(name, fn)
                    i += 1
                    continue

                m = typedef_pat.match(line)
                if m:
                    name = m.group(1)
                    if len(name) >= 2 and name[1].isupper():
                        real.setdefault(name, fn)
                    i += 1
                    continue

                m = fwd_pat.match(line)
                if m:
                    name = m.group(1)
                    if len(name) >= 2 and name[1].isupper():
                        weak.setdefault(name, fn)
                i += 1
    count = 0
    for name, fn in {**weak, **real}.items():
        target = os.path.join(outdir, name)
        with open(target, 'w') as out:
            out.write('#include "%s"\n' % fn)
        count += 1
    return count

n1 = scan(os.path.join(QTBASE, "src/corelib"), "QtCore")
n2 = scan(os.path.join(QTBASE, "src/gui"), "QtGui")
print("QtCore forwards: %d, QtGui forwards: %d" % (n1, n2))
