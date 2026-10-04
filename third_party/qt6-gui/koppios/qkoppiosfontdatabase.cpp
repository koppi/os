/* koppios addition, not upstream Qt: the font database for the minimal QPA
 * integration. Qt's own QFreeTypeFontDatabase already does all the real work
 * (FreeType face loading, family/style registration, font-engine creation);
 * the only platform-specific part is *where fonts come from*. There is no
 * fontconfig and no font directory convention here, so populateFontDatabase()
 * reads one TrueType file, "/rd/font.ttf" (the boot ramdisk),
 * through the koppios file engine (QFile) and registers it with the same
 * memory-based entry point QFontDatabase::addApplicationFont() uses. */
#include <private/qfreetypefontdatabase_p.h>
#include <QtCore/qfile.h>
#include <QtCore/qbytearray.h>

QT_BEGIN_NAMESPACE

class QKoppiosFontDatabase : public QFreeTypeFontDatabase
{
public:
    void populateFontDatabase() override
    {
        // Registration needs the bytes: with empty data FreeType would open
        // the file itself (FT_New_Face -> FT_Stream_Open, which koppios'
        // memory-only ftsystem does not provide). The absolute path becomes
        // the face id, and QFreetypeFace::getFace() later re-reads it
        // through QFile when it creates the font engine.
        const QByteArray path("/rd/font.ttf");
        QFile f(QString::fromLatin1(path));
        if (!f.open(QIODevice::ReadOnly))
            return;
        const QByteArray data = f.readAll();
        if (!data.isEmpty())
            QFreeTypeFontDatabase::addTTFile(data, path);
    }
};

QPlatformFontDatabase *qt_koppios_create_font_database()
{
    return new QKoppiosFontDatabase;
}

QT_END_NAMESPACE
