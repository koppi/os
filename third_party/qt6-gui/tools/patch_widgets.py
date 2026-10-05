#!/usr/bin/env python3
"""koppios additions to QtWidgets sources, each hunk idempotent.

qwidget.cpp: QWidgetPrivate::flagsForDumping() (a debug-only helper behind
QObject::dumpObjectTree) formats the widget geometry with std::stringstream. This port has no
libstdc++ iostreams (they need locale machinery and exceptions), so build the same text with
QByteArray instead.
"""
import sys
p = sys.argv[1] + "/src/widgets/kernel/qwidget.cpp"
s = open(p).read()
if "koppios addition" not in s:
    old = """        std::stringstream s;
        s << '<'
          << q->width() << 'x' << q->height()
          << std::showpos << q->x() << q->y()
          << '>';
        flags += s.str();"""
    new = """        // koppios addition, not upstream Qt: no <sstream> here; same text via QByteArray
        flags += QByteArray("<" + QByteArray::number(q->width()) + "x" + QByteArray::number(q->height())
                            + (q->x() >= 0 ? "+" : "") + QByteArray::number(q->x())
                            + (q->y() >= 0 ? "+" : "") + QByteArray::number(q->y()) + ">").constData();"""
    assert old in s
    open(p, "w").write(s.replace(old, new, 1))
    print("patched qwidget.cpp")
