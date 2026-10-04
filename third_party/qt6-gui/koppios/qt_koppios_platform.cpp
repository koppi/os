/* koppios addition, not upstream Qt: the platform glue for this port.
 *
 * Qt's own real "minimal" QPA plugin (src/plugins/platforms/minimal/, compiled
 * as-is) already provides a complete, generic QPlatformIntegration. Three
 * things are koppios-specific, and are all that is subclassed here:
 *   1. the factory function, so no dlopen()-based plugin loader is needed;
 *   2. windows send an expose event when shown (the minimal plugin's windows
 *      never do, so a QRasterWindow would never be asked to paint);
 *   3. the backing store's flush() presents the finished frame on the
 *      screen: the QImage Qt painted into is quantized to an adaptive
 *      256-colour palette and handed to gfx_blit (syscall 25), the same
 *      full-screen-grab path apps/doom uses (syscalls 22-26, video.c).
 */
#include <qminimalintegration.h>
#include <qminimalbackingstore.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>
#include <QtGui/qwindow.h>
#include <QtGui/qimage.h>
#include <QtCore/qbytearray.h>
#include <algorithm>

extern "C" unsigned syscall3(int n, unsigned a, unsigned b, unsigned c);

namespace {

bool g_gfxOpen = false;
QSize g_gfxSize;
QByteArray g_frame;

// The kernel presents an 8-bit indexed frame (gfx_blit) with a 256-entry palette,
// so the QImage Qt painted has to be quantized. A fixed colour cube bands badly
// on smooth gradients (hue shifts at every channel step), so the palette is built
// from the picture itself: 16 reserved greys (text, outlines) plus 240 median-cut
// colours of a sparse sample of the first frame. A 32 K-entry RGB555 table maps
// pixels to their nearest palette entry; an ordered 4x4 dither offsets all three
// channels equally (luminance only), so it cannot tint.
unsigned g_palette[256];
unsigned char g_lut[32768];

struct Box { int lo, hi; };

void buildPalette(const QImage &image)
{
    static unsigned samples[16384];
    int n = 0;
    for (int y = 0; y < image.height(); y += 3) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width() && n < 16384; x += 3)
            samples[n++] = line[x] & 0xffffff;
    }
    for (int i = 0; i < 16; i++)
        g_palette[i] = unsigned(i * 17) * 0x010101u;

    Box boxes[256];
    int nb = 1;
    boxes[0] = {0, n};
    const int target = 240;
    while (nb < target) {
        int best = -1, bestRange = 0, bestCh = 0;
        for (int i = 0; i < nb; i++) {
            if (boxes[i].hi - boxes[i].lo < 2)
                continue;
            int mn[3] = {255, 255, 255}, mx[3] = {0, 0, 0};
            for (int k = boxes[i].lo; k < boxes[i].hi; k++)
                for (int c = 0; c < 3; c++) {
                    const int v = (samples[k] >> (16 - 8 * c)) & 255;
                    if (v < mn[c]) mn[c] = v;
                    if (v > mx[c]) mx[c] = v;
                }
            for (int c = 0; c < 3; c++)
                if (mx[c] - mn[c] > bestRange) { bestRange = mx[c] - mn[c]; best = i; bestCh = c; }
        }
        if (best < 0)
            break;
        const int shift = 16 - 8 * bestCh;
        unsigned *p = samples + boxes[best].lo;
        const int len = boxes[best].hi - boxes[best].lo;
        std::sort(p, p + len, [shift](unsigned l, unsigned r) { return ((l >> shift) & 255) < ((r >> shift) & 255); });
        const int mid = boxes[best].lo + len / 2;
        boxes[nb] = {mid, boxes[best].hi};
        boxes[best].hi = mid;
        nb++;
    }
    for (int i = 0; i < nb; i++) {
        unsigned long long sum[3] = {0, 0, 0};
        const int len = boxes[i].hi - boxes[i].lo;
        for (int k = boxes[i].lo; k < boxes[i].hi; k++)
            for (int c = 0; c < 3; c++)
                sum[c] += (samples[k] >> (16 - 8 * c)) & 255;
        const unsigned r = len ? unsigned(sum[0] / len) : 0, g = len ? unsigned(sum[1] / len) : 0,
                       b = len ? unsigned(sum[2] / len) : 0;
        g_palette[16 + i] = r << 16 | g << 8 | b;
    }
    for (int i = 16 + nb; i < 256; i++)
        g_palette[i] = 0;

    for (int idx = 0; idx < 32768; idx++) {
        const int r = ((idx >> 10) & 31) * 255 / 31, g = ((idx >> 5) & 31) * 255 / 31, b = (idx & 31) * 255 / 31;
        int bestD = 1 << 30, bestI = 0;
        for (int i = 0; i < 16 + nb; i++) {
            const int dr = r - int((g_palette[i] >> 16) & 255), dg = g - int((g_palette[i] >> 8) & 255),
                      db = b - int(g_palette[i] & 255);
            const int d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;   // eye is most sensitive to green
            if (d < bestD) { bestD = d; bestI = i; }
        }
        g_lut[idx] = static_cast<unsigned char>(bestI);
    }
}

void presentImage(const QImage &image)
{
    if (image.isNull())
        return;
    if (!g_gfxOpen || g_gfxSize != image.size()) {
        if (g_gfxOpen)
            syscall3(23, 0, 0, 0);                                   // gfx_close
        if (!syscall3(22, unsigned(image.width()), unsigned(image.height()), 0))   // gfx_open
            return;
        g_gfxOpen = true;
        g_gfxSize = image.size();
        g_frame.resize(image.width() * image.height());
        buildPalette(image);
        syscall3(24, unsigned(reinterpret_cast<quintptr>(g_palette)), 0, 0);   // gfx_palette
    }
    static const signed char bayer[4][4] = {
        {-4,  0, -3,  1}, { 2, -2,  3, -1}, {-3,  1, -4,  0}, { 3, -1,  2, -2}};
    unsigned char *out = reinterpret_cast<unsigned char *>(g_frame.data());
    for (int y = 0; y < image.height(); y++) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x++) {
            const int d = bayer[y & 3][x & 3];
            const QRgb c = line[x];
            int r = qRed(c) + d, g = qGreen(c) + d, b = qBlue(c) + d;
            r = r < 0 ? 0 : r > 255 ? 255 : r;
            g = g < 0 ? 0 : g > 255 ? 255 : g;
            b = b < 0 ? 0 : b > 255 ? 255 : b;
            *out++ = g_lut[(r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)];
        }
    }
    syscall3(25, unsigned(reinterpret_cast<quintptr>(g_frame.constData())), 0, 0);   // gfx_blit
}

class QKoppiosPlatformWindow : public QPlatformWindow
{
public:
    explicit QKoppiosPlatformWindow(QWindow *window) : QPlatformWindow(window) {}

    void setVisible(bool visible) override
    {
        QPlatformWindow::setVisible(visible);
        QWindowSystemInterface::handleExposeEvent(
            window(), visible ? QRegion(QRect(QPoint(0, 0), window()->size())) : QRegion());
    }
};

class QKoppiosBackingStore : public QMinimalBackingStore
{
public:
    explicit QKoppiosBackingStore(QWindow *window) : QMinimalBackingStore(window) {}

    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override
    {
        QMinimalBackingStore::flush(window, region, offset);
        presentImage(*static_cast<QImage *>(paintDevice()));
    }
};

class QKoppiosIntegration : public QMinimalIntegration
{
public:
    using QMinimalIntegration::QMinimalIntegration;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override
    {
        QPlatformWindow *w = new QKoppiosPlatformWindow(window);
        w->requestActivateWindow();
        return w;
    }

    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override
    {
        return new QKoppiosBackingStore(window);
    }
};

} // namespace

QPlatformIntegration *qt_koppios_create_platform_integration() {
    return new QKoppiosIntegration(QStringList() << QStringLiteral("enable_fonts"));
}

// Hand the screen back to the desktop (gfx_close also leaves raw-key mode).
void qt_koppios_gfx_close()
{
    if (g_gfxOpen) {
        syscall3(23, 0, 0, 0);
        g_gfxOpen = false;
    }
}
