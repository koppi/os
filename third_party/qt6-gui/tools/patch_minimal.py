#!/usr/bin/env python3
"""koppios addition, not upstream Qt: QMinimalIntegration::fontDatabase()'s
non-fontconfig fallback returns the empty base QPlatformFontDatabase. On
koppios, hand it to qt_koppios_create_font_database() (qkoppiosfontdatabase.cpp),
a QFreeTypeFontDatabase that loads one TTF from the ramdisk."""
import sys
p = sys.argv[1] + "/src/plugins/platforms/minimal/qminimalintegration.cpp"
s = open(p).read()
if "qt_koppios_create_font_database" in s:
    sys.exit(0)
old = """#if QT_CONFIG(fontconfig)
            m_fontDatabase = new QGenericUnixFontDatabase;
#else
            m_fontDatabase = QPlatformIntegration::fontDatabase();
#endif"""
new = """#if defined(__KOPPIOS__)
            m_fontDatabase = qt_koppios_create_font_database();  // koppios addition, not upstream Qt
#elif QT_CONFIG(fontconfig)
            m_fontDatabase = new QGenericUnixFontDatabase;
#else
            m_fontDatabase = QPlatformIntegration::fontDatabase();
#endif"""
decl_anchor = "QT_BEGIN_NAMESPACE\n\nusing namespace Qt::StringLiterals;"
decl = """QT_BEGIN_NAMESPACE

#if defined(__KOPPIOS__)
QPlatformFontDatabase *qt_koppios_create_font_database();  // qkoppiosfontdatabase.cpp
#endif

using namespace Qt::StringLiterals;"""
assert old in s and decl_anchor in s
s = s.replace(old, new, 1).replace(decl_anchor, decl, 1)
open(p, "w").write(s)
print("patched qminimalintegration.cpp")
