/* Bisect the hello_qt_gui paint sequence one primitive at a time. */
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <cstdlib>
#include <QRadialGradient>
extern "C" unsigned _write(const void *buf, unsigned len);
void qt_koppios_install_file_engine_handler();
static unsigned sl(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static void rep(const char *m) { _write(m, sl(m)); }
static void repn(const char *l, unsigned long v) {
    char b[24]; int n = 0; char t[24]; int k = 0;
    do { t[k++] = "0123456789abcdef"[v % 16]; v /= 16; } while (v);
    while (k) b[n++] = t[--k];
    b[n++] = '\n'; b[n] = 0; rep(l); rep(b);
}
static QImage *g_img;
static void touch(const char *tag) {
    volatile unsigned char *p = g_img->bits(); long n = g_img->sizeInBytes(); unsigned sum = 0;
    for (long i = 0; i < n; i += 4096) sum += p[i];
    rep("  touched all pages after "); rep(tag); rep("\n");
    (void) sum;
}
static char arg0[] = "/rd/hqtgui"; static char *fake_argv[] = {arg0, 0};
int main() {
    qt_koppios_install_file_engine_handler();
    int argc = 1; QGuiApplication app(argc, fake_argv);
    QImage img(480, 270, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    g_img = &img;
    touch("fill");
    QPainter p(&img);
    rep("1 painter ok\n");
    p.setRenderHint(QPainter::Antialiasing);
    rep("2 antialiasing set\n");
    QLinearGradient bg(0, 0, 0, 270);
    bg.setColorAt(0.0, QColor(18, 24, 64)); bg.setColorAt(1.0, QColor(70, 130, 180));
    p.fillRect(0, 0, 480, 270, bg);
    rep("3 linear gradient fill ok\n"); touch("3 linear gradient fill ok");
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(10, 10, 50, 50, QColor(255, 255, 255, 40));
    rep("A translucent fillRect, no AA ok\n"); touch("A translucent fillRect, no AA ok");
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(70, 10, 50, 50, QColor(255, 255, 255, 40));
    rep("B translucent fillRect, AA ok\n"); touch("B translucent fillRect, AA ok");
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 0, 0, 255));
    p.drawEllipse(QPointF(200, 40), 20, 20);
    rep("C opaque AA ellipse ok\n"); touch("C opaque AA ellipse ok");
    p.setBrush(QColor(255, 255, 255, 40));
    p.drawEllipse(QPointF(260, 40), 20, 20);
    rep("D translucent AA ellipse ok\n"); touch("D translucent AA ellipse ok");
    repn("img bits (hex): ", (unsigned long) img.bits());
    repn("img end  (hex): ", (unsigned long) img.bits() + img.sizeInBytes());
    repn("fresh malloc(16) (hex): ", (unsigned long) malloc(16));
    QPainterPath rr; rr.addRoundedRect(QRectF(300, 10, 100, 60), 14, 14);
    p.setBrush(QColor(255, 0, 0, 255));
    p.drawPath(rr);
    rep("E opaque rounded path ok\n");
    p.setBrush(QColor(255, 255, 255, 40));
    p.drawPath(rr);
    rep("F translucent rounded path ok\n");
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 180), 2));
    p.drawPath(rr);
    rep("G stroked translucent pen ok\n");
    return 0;
}
