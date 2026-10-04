/* Functional test: real QGuiApplication + QImage + QPainter + QFont::drawText
 * (FreeType + HarfBuzz) on the koppios kernel. Prints an ASCII rendering. */
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QFile>
#include <QColor>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <ft2build.h>
#include FT_FREETYPE_H

extern "C" unsigned _write(const void *buf, unsigned len);
extern "C" char *pwd();
void qt_koppios_install_file_engine_handler();

static unsigned koppios_strlen(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static void report(const char *msg) { _write(msg, koppios_strlen(msg)); }
static void report_int(const char *label, long v) {
    char buf[40]; int n = 0; unsigned long u = v < 0 ? -v : v;
    char tmp[24]; int t = 0;
    do { tmp[t++] = '0' + u % 10; u /= 10; } while (u);
    if (v < 0) buf[n++] = '-';
    while (t) buf[n++] = tmp[--t];
    buf[n++] = '\n'; buf[n] = 0;
    report(label); report(buf);
}

static char arg0[] = "/rd/hqtgui";
static char *fake_argv[] = {arg0, 0};

int main(void) {
    qt_koppios_install_file_engine_handler();
    {
        static const char *tries[] = {"/rd/font.ttf", "rd/font.ttf", "font.ttf", 0};
        for (int i = 0; tries[i]; i++) {
            QFile f(QString::fromLatin1(tries[i]));
            report(tries[i]);
            report_int(" exists: ", f.exists());
            if (f.open(QIODevice::ReadOnly))
                report_int("   size: ", f.size());
        }
        report("cwd: "); report(pwd()); report("\n");
    }
    {   // direct FreeType probe
        QFile ff(QStringLiteral("/rd/font.ttf"));
        QByteArray fd;
        if (ff.open(QIODevice::ReadOnly)) fd = ff.readAll();
        FT_Library lib = nullptr;
        report_int("FT_Init_FreeType: ", FT_Init_FreeType(&lib));
        FT_Face face = nullptr;
        report_int("FT_New_Memory_Face: ", FT_New_Memory_Face(lib, (const FT_Byte *) fd.constData(), fd.size(), 0, &face));
        if (face) {
            report_int("  num_glyphs: ", face->num_faces ? face->num_glyphs : -1);
            report_int("  Set_Pixel_Sizes: ", FT_Set_Pixel_Sizes(face, 0, 30));
            report_int("  Load_Char('H'): ", FT_Load_Char(face, 'H', FT_LOAD_RENDER));
            if (face->glyph) { report_int("  bitmap rows: ", face->glyph->bitmap.rows); report_int("  bitmap width: ", face->glyph->bitmap.width); }
        }
    }
    int argc = 1;
    char **argv = fake_argv;
    QGuiApplication app(argc, argv);
    report("marker: after QGuiApplication ctor\n");

    {
        const QStringList fams = QFontDatabase::families();
        report_int("families: ", fams.size());
        for (const QString &f : fams) { report("  family: "); report(f.toUtf8().constData()); report("\n"); }
        QFont q; q.setPixelSize(30);
        report("default family: "); report(q.family().toUtf8().constData()); report("\n");
        report("resolved family: "); report(QFontInfo(q).family().toUtf8().constData()); report("\n");
        report_int("advance of H: ", QFontMetrics(q).horizontalAdvance(QLatin1Char('H')));
        report_int("exactMatch: ", QFontInfo(q).exactMatch());
    }
    const int W = 220, H = 48;
    QImage img(W, H, QImage::Format_ARGB32);
    img.fill(Qt::white);
    {
        QPainter p(&img);
        p.fillRect(2, 2, 28, 44, QColor(200, 0, 0));
        QFont font;
        font.setPixelSize(30);
        p.setFont(font);
        p.setPen(Qt::black);
        p.drawText(38, 34, QStringLiteral("Hello, Qt6!"));
        report_int("font family resolved? ", font.family().isEmpty() ? 0 : 1);
    }
    long rectPx = 0, textPx = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            QRgb c = img.pixel(x, y);
            if (c == 0xffffffff) continue;
            if (x < 32) rectPx++; else textPx++;
        }
    report_int("rect pixels: ", rectPx);
    report_int("text pixels: ", textPx);

    for (int y = 0; y < H; y += 4) {
        char row[W / 2 + 2]; int n = 0;
        for (int x = 0; x < W; x += 2) {
            int sum = 0;
            for (int dy = 0; dy < 4; dy++) for (int dx = 0; dx < 2; dx++)
                sum += 255 - qGray(img.pixel(x + dx, y + dy));
            sum /= 8;
            row[n++] = sum > 160 ? '#' : sum > 90 ? '+' : sum > 30 ? '.' : ' ';
        }
        row[n++] = '\n'; row[n] = 0;
        report(row);
    }
    report(textPx > 100 ? "RESULT: text rendered\n" : "RESULT: NO TEXT\n");
    return 0;
}
