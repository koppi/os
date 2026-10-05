#!/usr/bin/env python3
"""koppios additions to Qt's minimal QPA plugin (qminimalintegration.cpp), each hunk idempotent:

1. fontDatabase(): the non-fontconfig fallback returns the empty base QPlatformFontDatabase.
   On koppios, hand it to qt_koppios_create_font_database() (koppios/qkoppiosfontdatabase.cpp),
   a QFreeTypeFontDatabase that loads one TTF from the ramdisk.
2. The QScreen geometry is the framebuffer frame the platform glue composites into
   (KOPPIOS_SCREEN_WIDTH x KOPPIOS_SCREEN_HEIGHT, default 640x400) instead of the
   plugin's hardcoded 240x320; popups and window placement are computed against it.
"""
import sys
p = sys.argv[1] + "/src/plugins/platforms/minimal/qminimalintegration.cpp"
s = open(p).read()

if "qt_koppios_create_font_database" not in s:
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
    anchor = "QT_BEGIN_NAMESPACE\n\nusing namespace Qt::StringLiterals;"
    decl = """QT_BEGIN_NAMESPACE

#if defined(__KOPPIOS__)
QPlatformFontDatabase *qt_koppios_create_font_database();  // qkoppiosfontdatabase.cpp
#endif

using namespace Qt::StringLiterals;"""
    assert old in s and anchor in s
    s = s.replace(old, new, 1).replace(anchor, decl, 1)
    print("patched fontDatabase()")

if "KOPPIOS_SCREEN_WIDTH" not in s:
    old = "    m_primaryScreen->mGeometry = QRect(0, 0, 240, 320);\n"
    new = """#if defined(__KOPPIOS__)
    // koppios addition, not upstream Qt: the screen is the frame the platform glue presents
#  ifndef KOPPIOS_SCREEN_WIDTH
#    define KOPPIOS_SCREEN_WIDTH 640
#  endif
#  ifndef KOPPIOS_SCREEN_HEIGHT
#    define KOPPIOS_SCREEN_HEIGHT 400
#  endif
    m_primaryScreen->mGeometry = QRect(0, 0, KOPPIOS_SCREEN_WIDTH, KOPPIOS_SCREEN_HEIGHT);
#else
    m_primaryScreen->mGeometry = QRect(0, 0, 240, 320);
#endif
"""
    assert old in s
    s = s.replace(old, new, 1)
    print("patched screen geometry")
open(p, "w").write(s)
