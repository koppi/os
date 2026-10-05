#!/bin/bash
# Re-points every generated CamelCase forward whose target is a *_p.h / *_impl.h file at the real
# public header (see the comment below). Run after gen_camel.py; setup.sh calls it.
set -u
SCRATCH="${1:?usage: redirect_forwards.sh <scratchpad-dir>}"
QTBASE="$SCRATCH/qtbase"
QTGEN="$SCRATCH/qtgen/include"
echo "=== 5/6: redirect any forward whose target is a _p.h/_impl.h file to the real public header ==="
# CRITICAL: only touch files gen_camel.py itself generated (its forwards are
# named after the CLASS, e.g. "QString" -- no .h suffix). $QTGEN/QtCore/*.h
# and $QTGEN/QtGui/*.h are SYMLINKS into the real qtbase clone from step 2 --
# writing through one of those with `echo ... > "$f"` overwrites the real
# Qt source file it points at, corrupting the clone itself. Learned this the
# hard way: an earlier version of this loop with no such guard silently
# replaced qlocale_tools_p.h (and probably others) with a 1-line stub.
count=0
for dir in "$QTGEN/QtCore" "$QTGEN/QtGui" "$QTGEN/QtWidgets"; do
    for f in "$dir"/*; do
        [ -f "$f" ] || continue
        case "$(basename "$f")" in *.h) continue;; esac
        [ -L "$f" ] && continue
        target=$(grep -o '"[^"]*"' "$f" 2>/dev/null | tr -d '"' | head -1)
        case "$target" in
            *_p.h|*_impl.h)
                base="${target%_p.h}"; base="${base%_impl.h}"
                pub=$(find "$QTBASE/src/corelib" "$QTBASE/src/gui" "$QTBASE/src/widgets" -maxdepth 3 -name "${base}.h" 2>/dev/null | grep -v "/3rdparty/" | head -1)
                if [ -n "$pub" ]; then
                    echo "#include \"$(basename "$pub")\"" > "$f"
                    count=$((count+1))
                fi
                ;;
        esac
    done
done
echo "redirected: $count"
