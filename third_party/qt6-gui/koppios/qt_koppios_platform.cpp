/* koppios addition, not upstream Qt: the platform glue for this port.
 *
 * Qt's own real "minimal" QPA plugin (src/plugins/platforms/minimal/, compiled
 * as-is) already provides a complete, generic QPlatformIntegration. What is
 * koppios-specific, and subclassed or added here:
 *   1. the factory function, so no dlopen()-based plugin loader is needed;
 *   2. windows get an expose event and keyboard focus when shown (the minimal
 *      plugin's windows get neither);
 *   3. a tiny compositor: every visible top-level window (the main window,
 *      combo-box popups, ...) is drawn into one screen-sized image at its
 *      geometry, which is quantized to an adaptive 256-colour palette and
 *      presented with gfx_blit (syscall 25) -- the full-screen-grab path
 *      apps/doom uses (syscalls 22..26). A drawn arrow is overlaid for the
 *      pointer;
 *   4. a platform theme that names Fusion as the default widget style;
 *   5. input: the raw scancode ring (syscall 26) is translated into Qt key
 *      events, and the raw pointer ring (syscall 36) into mouse events.
 *
 * The screen size comes from KOPPIOS_SCREEN_WIDTH/HEIGHT, which the patched
 * qminimalintegration.cpp uses for the QScreen geometry too.
 */
#include <qminimalintegration.h>
#include <qminimalbackingstore.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qplatformtheme.h>
#include <qpa/qwindowsysteminterface.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qwindow.h>
#include <QtGui/qimage.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qtimer.h>
#include <algorithm>

#ifndef KOPPIOS_SCREEN_WIDTH
#  define KOPPIOS_SCREEN_WIDTH 640
#endif
#ifndef KOPPIOS_SCREEN_HEIGHT
#  define KOPPIOS_SCREEN_HEIGHT 400
#endif

extern "C" unsigned syscall3(int n, unsigned a, unsigned b, unsigned c);

namespace {

constexpr int W = KOPPIOS_SCREEN_WIDTH;
constexpr int H = KOPPIOS_SCREEN_HEIGHT;

// ===================================================================
//  Presentation: screen image -> adaptive 8-bit palette -> gfx_blit
// ===================================================================
//
// gfx_blit takes an indexed frame with a 256-entry palette. A fixed colour
// cube bands badly on smooth gradients (hue shifts at every channel step), so
// the palette is built from the picture itself: 16 reserved greys (text,
// outlines, the pointer's black and white) plus 240 median-cut colours of a
// sparse sample. A 32 K-entry RGB555 table maps pixels to the nearest entry;
// an ordered 4x4 dither offsets all three channels equally (luminance only),
// so it cannot tint.

bool g_gfxOpen = false;
unsigned g_palette[256];
unsigned char g_lut[32768];
unsigned char g_frame[W * H];       // quantized screen, without pointer
unsigned char g_shown[W * H];       // g_frame + pointer, what gfx_blit gets
int g_paletteAge = 1 << 30;         // flushes since the palette was built

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
    syscall3(24, unsigned(reinterpret_cast<quintptr>(g_palette)), 0, 0);   // gfx_palette
    g_paletteAge = 0;
}

void quantize(const QImage &image, bool rebuild)
{
    if (!g_gfxOpen) {
        if (!syscall3(22, W, H, 0))                                   // gfx_open
            return;
        g_gfxOpen = true;
        rebuild = true;
    }
    if (rebuild || ++g_paletteAge > 90)
        buildPalette(image);
    static const signed char bayer[4][4] = {
        {-4,  0, -3,  1}, { 2, -2,  3, -1}, {-3,  1, -4,  0}, { 3, -1,  2, -2}};
    unsigned char *out = g_frame;
    for (int y = 0; y < H; y++) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < W; x++) {
            const int d = bayer[y & 3][x & 3];
            const QRgb c = line[x];
            int r = qRed(c) + d, g = qGreen(c) + d, b = qBlue(c) + d;
            r = r < 0 ? 0 : r > 255 ? 255 : r;
            g = g < 0 ? 0 : g > 255 ? 255 : g;
            b = b < 0 ? 0 : b > 255 ? 255 : b;
            *out++ = g_lut[(r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)];
        }
    }
}

// ---- the pointer: a drawn arrow in palette index space (0 = black, 15 = white) ----
int g_mx = W / 2, g_my = H / 2;
bool g_cursorMoved = false;

const char *const kCursor[] = {
    "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ", "X....X      ",
    "X.....X     ", "X......X    ", "X.......X   ", "X........X  ", "X.....XXXXX ", "X..X..X     ",
    "X.X X..X    ", "XX  X..X    ", "X    X..X   ", "     X..X   ", "      X..X  ", "      X..X  ",
    "       XX   ",
};

void present()
{
    if (!g_gfxOpen)
        return;
    memcpy(g_shown, g_frame, sizeof g_shown);
    for (int row = 0; row < int(sizeof kCursor / sizeof *kCursor); row++) {
        const int y = g_my + row;
        if (y < 0 || y >= H)
            continue;
        for (int col = 0; kCursor[row][col]; col++) {
            const int x = g_mx + col;
            if (x < 0 || x >= W)
                continue;
            if (kCursor[row][col] == 'X')
                g_shown[y * W + x] = 0;
            else if (kCursor[row][col] == '.')
                g_shown[y * W + x] = 15;
        }
    }
    syscall3(25, unsigned(reinterpret_cast<quintptr>(g_shown)), 0, 0);   // gfx_blit
}

// ===================================================================
//  Windows, backing stores and the compositor
// ===================================================================

class QKoppiosBackingStore;
QList<QKoppiosBackingStore *> g_stores;
QImage g_screen;

class QKoppiosBackingStore : public QMinimalBackingStore
{
public:
    explicit QKoppiosBackingStore(QWindow *window) : QMinimalBackingStore(window), m_window(window)
    {
        g_stores.append(this);
    }
    ~QKoppiosBackingStore() override
    {
        g_stores.removeAll(this);
    }

    QWindow *window() const { return m_window; }
    QImage *image() { return static_cast<QImage *>(paintDevice()); }
    bool painted() const { return m_painted; }   // false until the first flush: the image is uninitialized

    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;

private:
    QWindow *m_window;
    bool m_painted = false;
};

// Draw every visible window at its geometry, quantize, present.
void recomposite(bool rebuildPalette)
{
    if (g_screen.isNull())
        g_screen = QImage(W, H, QImage::Format_RGB32);
    g_screen.fill(Qt::black);
    {
        QPainter p(&g_screen);
        for (QKoppiosBackingStore *s : std::as_const(g_stores)) {
            QWindow *w = s->window();
            QImage *im = s->image();
            if (!w || !im || im->isNull() || !w->isVisible() || !s->painted())
                continue;
            p.drawImage(w->geometry().topLeft(), *im);
        }
    }
    quantize(g_screen, rebuildPalette);
    present();
}

void QKoppiosBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    QMinimalBackingStore::flush(window, region, offset);
    m_painted = true;
    recomposite(false);
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
        if (visible && wantsFocus())
            requestActivateWindow();
        if (!visible)
            QTimer::singleShot(0, [] { recomposite(true); });   // uncover what the window hid
    }

    void propagateSizeHints() override {}   // no window manager to tell

    void requestActivateWindow() override
    {
        if (wantsFocus())
            QWindowSystemInterface::handleFocusWindowChanged(window(), Qt::ActiveWindowFocusReason);
    }

private:
    bool wantsFocus() const
    {
        // Qt::Popup / Qt::ToolTip are composite values that include the Window bit, so they
        // must be compared as the window *type*, not masked against flags().
        const Qt::WindowType type = window()->type();
        return type != Qt::Popup && type != Qt::ToolTip && type != Qt::SubWindow
            && !(window()->flags() & Qt::WindowDoesNotAcceptFocus);
    }
};

// ===================================================================
//  Input: raw scancodes (syscall 26) and pointer events (syscall 36)
// ===================================================================

struct KeyDef { int key; char normal; char shifted; };

// Set-1 make codes for the main block, US layout. Letters carry their lowercase char; Shift or
// Caps Lock selects the uppercase one.
KeyDef keyForScancode(unsigned sc, bool e0)
{
    static const KeyDef main[0x59] = {
        /*00*/ {0, 0, 0}, {Qt::Key_Escape, 0, 0},
        /*02*/ {Qt::Key_1, '1', '!'}, {Qt::Key_2, '2', '@'}, {Qt::Key_3, '3', '#'}, {Qt::Key_4, '4', '$'},
        /*06*/ {Qt::Key_5, '5', '%'}, {Qt::Key_6, '6', '^'}, {Qt::Key_7, '7', '&'}, {Qt::Key_8, '8', '*'},
        /*0a*/ {Qt::Key_9, '9', '('}, {Qt::Key_0, '0', ')'}, {Qt::Key_Minus, '-', '_'}, {Qt::Key_Equal, '=', '+'},
        /*0e*/ {Qt::Key_Backspace, 0, 0}, {Qt::Key_Tab, '\t', '\t'},
        /*10*/ {Qt::Key_Q, 'q', 'Q'}, {Qt::Key_W, 'w', 'W'}, {Qt::Key_E, 'e', 'E'}, {Qt::Key_R, 'r', 'R'},
        /*14*/ {Qt::Key_T, 't', 'T'}, {Qt::Key_Y, 'y', 'Y'}, {Qt::Key_U, 'u', 'U'}, {Qt::Key_I, 'i', 'I'},
        /*18*/ {Qt::Key_O, 'o', 'O'}, {Qt::Key_P, 'p', 'P'}, {Qt::Key_BracketLeft, '[', '{'}, {Qt::Key_BracketRight, ']', '}'},
        /*1c*/ {Qt::Key_Return, '\r', '\r'}, {Qt::Key_Control, 0, 0},
        /*1e*/ {Qt::Key_A, 'a', 'A'}, {Qt::Key_S, 's', 'S'}, {Qt::Key_D, 'd', 'D'}, {Qt::Key_F, 'f', 'F'},
        /*22*/ {Qt::Key_G, 'g', 'G'}, {Qt::Key_H, 'h', 'H'}, {Qt::Key_J, 'j', 'J'}, {Qt::Key_K, 'k', 'K'},
        /*26*/ {Qt::Key_L, 'l', 'L'}, {Qt::Key_Semicolon, ';', ':'}, {Qt::Key_Apostrophe, '\'', '"'}, {Qt::Key_QuoteLeft, '`', '~'},
        /*2a*/ {Qt::Key_Shift, 0, 0}, {Qt::Key_Backslash, '\\', '|'},
        /*2c*/ {Qt::Key_Z, 'z', 'Z'}, {Qt::Key_X, 'x', 'X'}, {Qt::Key_C, 'c', 'C'}, {Qt::Key_V, 'v', 'V'},
        /*30*/ {Qt::Key_B, 'b', 'B'}, {Qt::Key_N, 'n', 'N'}, {Qt::Key_M, 'm', 'M'}, {Qt::Key_Comma, ',', '<'},
        /*34*/ {Qt::Key_Period, '.', '>'}, {Qt::Key_Slash, '/', '?'}, {Qt::Key_Shift, 0, 0}, {Qt::Key_Asterisk, '*', '*'},
        /*38*/ {Qt::Key_Alt, 0, 0}, {Qt::Key_Space, ' ', ' '}, {Qt::Key_CapsLock, 0, 0},
        /*3b*/ {Qt::Key_F1, 0, 0}, {Qt::Key_F2, 0, 0}, {Qt::Key_F3, 0, 0}, {Qt::Key_F4, 0, 0}, {Qt::Key_F5, 0, 0},
        /*40*/ {Qt::Key_F6, 0, 0}, {Qt::Key_F7, 0, 0}, {Qt::Key_F8, 0, 0}, {Qt::Key_F9, 0, 0}, {Qt::Key_F10, 0, 0},
        /*45*/ {Qt::Key_NumLock, 0, 0}, {Qt::Key_ScrollLock, 0, 0},
        /*47*/ {Qt::Key_Home, 0, 0}, {Qt::Key_Up, 0, 0}, {Qt::Key_PageUp, 0, 0}, {Qt::Key_Minus, '-', '-'},
        /*4b*/ {Qt::Key_Left, 0, 0}, {0, 0, 0}, {Qt::Key_Right, 0, 0}, {Qt::Key_Plus, '+', '+'},
        /*4f*/ {Qt::Key_End, 0, 0}, {Qt::Key_Down, 0, 0}, {Qt::Key_PageDown, 0, 0}, {Qt::Key_Insert, 0, 0},
        /*53*/ {Qt::Key_Delete, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {Qt::Key_F11, 0, 0}, {Qt::Key_F12, 0, 0},
    };
    KeyDef k = sc < sizeof main / sizeof *main ? main[sc] : KeyDef{0, 0, 0};
    if (e0) {                                     // extended block: Enter/slash on the keypad, right Ctrl/Alt
        if (sc == 0x1C) k = {Qt::Key_Enter, '\r', '\r'};
        else if (sc == 0x35) k = {Qt::Key_Slash, '/', '/'};
        else if (sc == 0x1D) k = {Qt::Key_Control, 0, 0};
        else if (sc == 0x38) k = {Qt::Key_AltGr, 0, 0};
    }
    return k;
}

class QKoppiosInput : public QObject
{
public:
    QKoppiosInput()
    {
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] { poll(); });
        m_timer.start(8);
    }

private:
    void poll()
    {
        bool moved = false;
        for (unsigned ev; ((ev = syscall3(26, 0, 0, 0)) & 0x10000) != 0;)
            key(ev);
        for (unsigned ev; ((ev = syscall3(36, 0, 0, 0)) & 0x80000000u) != 0;)
            moved |= mouse(ev);
        if (moved) {
            present();
        }
    }

    void key(unsigned ev)
    {
        const bool release = ev & 0x80, e0 = ev & 0x100;
        const unsigned sc = ev & 0x7F;
        const KeyDef k = keyForScancode(sc, e0);
        if (k.key == 0)
            return;
        if (k.key == Qt::Key_Shift)   { m_shift = !release; }
        else if (k.key == Qt::Key_Control) { m_ctrl = !release; }
        else if (k.key == Qt::Key_Alt || k.key == Qt::Key_AltGr) { m_alt = !release; }
        else if (k.key == Qt::Key_CapsLock && !release) { m_caps = !m_caps; }

        Qt::KeyboardModifiers mods;
        if (m_shift) mods |= Qt::ShiftModifier;
        if (m_ctrl)  mods |= Qt::ControlModifier;
        if (m_alt)   mods |= Qt::AltModifier;

        QString text;
        const bool letter = k.normal >= 'a' && k.normal <= 'z';
        char c = (m_shift != (m_caps && letter)) ? k.shifted : k.normal;
        if (c && !m_ctrl && !m_alt && !(k.key == Qt::Key_Return || k.key == Qt::Key_Enter))
            text = QString(QChar::fromLatin1(c));
        else if (k.key == Qt::Key_Return || k.key == Qt::Key_Enter)
            text = QStringLiteral("\r");
        else if (k.key == Qt::Key_Backspace)
            text = QStringLiteral("\b");
        int qtKey = k.key;
        if (m_shift && k.key == Qt::Key_Tab)
            qtKey = Qt::Key_Backtab;               // Shift+Tab walks the focus chain backwards
        QWindowSystemInterface::handleKeyEvent(nullptr, release ? QEvent::KeyRelease : QEvent::KeyPress,
                                               qtKey, mods, text);
    }

    // Returns true when the pointer moved (the overlay needs redrawing).
    bool mouse(unsigned ev)
    {
        auto sext12 = [](unsigned v) { return int(v & 0x800 ? v | 0xFFFFF000u : v & 0xFFF); };
        const int dx = sext12(ev & 0xFFF), dy = sext12((ev >> 12) & 0xFFF);
        const unsigned buttons = (ev >> 24) & 7;
        const int ox = g_mx, oy = g_my;
        g_mx = std::clamp(g_mx + dx, 0, W - 1);
        g_my = std::clamp(g_my + dy, 0, H - 1);

        Qt::MouseButtons state;
        if (buttons & 1) state |= Qt::LeftButton;
        if (buttons & 2) state |= Qt::RightButton;
        if (buttons & 4) state |= Qt::MiddleButton;

        const QPoint global(g_mx, g_my);
        QWindow *w = QGuiApplication::topLevelAt(global);
        const QPointF local = w ? QPointF(global - w->position()) : QPointF(global);
        auto send = [&](Qt::MouseButton button, QEvent::Type type) {
            QWindowSystemInterface::handleMouseEvent(w, local, QPointF(global), state, button, type);
        };
        if (g_mx != ox || g_my != oy)
            send(Qt::NoButton, QEvent::MouseMove);
        for (auto [bit, qt] : {std::pair{1u, Qt::LeftButton}, std::pair{2u, Qt::RightButton}, std::pair{4u, Qt::MiddleButton}}) {
            const bool now = buttons & bit, was = m_buttons & bit;
            if (now != was)
                send(qt, now ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease);
        }
        m_buttons = buttons;
        return g_mx != ox || g_my != oy;
    }

    QTimer m_timer;
    bool m_shift = false, m_ctrl = false, m_alt = false, m_caps = false;
    unsigned m_buttons = 0;
};

// The platform theme exists only to name the default widget style: with no theme, QApplication
// falls back to the first style the factory lists, which is the classic "Windows" one.
class QKoppiosTheme : public QPlatformTheme
{
public:
    QVariant themeHint(ThemeHint hint) const override
    {
        if (hint == StyleNames)
            return QStringList{QStringLiteral("Fusion")};
        return QPlatformTheme::themeHint(hint);
    }
};

class QKoppiosIntegration : public QMinimalIntegration
{
public:
    using QMinimalIntegration::QMinimalIntegration;

    QStringList themeNames() const override { return {QStringLiteral("koppios")}; }

    QPlatformTheme *createPlatformTheme(const QString &name) const override
    {
        return name == QLatin1String("koppios") ? new QKoppiosTheme : nullptr;
    }

    QPlatformWindow *createPlatformWindow(QWindow *window) const override
    {
        static QKoppiosInput *input = new QKoppiosInput;   // needs the event dispatcher, so not in the ctor
        Q_UNUSED(input);
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

// Hand the screen back to the desktop (gfx_close also leaves raw-key and raw-mouse mode).
void qt_koppios_gfx_close()
{
    if (g_gfxOpen) {
        syscall3(23, 0, 0, 0);
        g_gfxOpen = false;
    }
}
