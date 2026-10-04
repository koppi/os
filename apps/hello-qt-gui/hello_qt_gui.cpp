/**
 * @file apps/hello-qt-gui/hello_qt_gui.cpp
 * @brief Graphical "Hello, Qt6!": a real QRasterWindow painted by real Qt 6.8
 *        (QPainter, gradients, antialiasing, FreeType + HarfBuzz text in the
 *        Unifont subset staged as /rd/font.ttf) driven by a real QTimer and
 *        app.exec(), presented on the kernel framebuffer through the same
 *        gfx syscalls Doom uses (see third_party/qt6-gui/koppios/
 *        qt_koppios_platform.cpp). Esc / Enter quits and returns the desktop.
 */
#include <QGuiApplication>
#include <QRasterWindow>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QTimer>
#include <QFont>
#include <QKeyEvent>
#include <qpa/qwindowsysteminterface.h>
#include <cmath>

extern "C" unsigned syscall3(int n, unsigned a, unsigned b, unsigned c);
void qt_koppios_install_file_engine_handler();
void qt_koppios_gfx_close();

class HelloWindow : public QRasterWindow
{
public:
    HelloWindow()
    {
        setTitle(QStringLiteral("Hello, Qt6!"));
        resize(480, 270);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = width(), h = height();

        QLinearGradient bg(0, 0, 0, h);
        bg.setColorAt(0.0, QColor(18, 24, 64));
        bg.setColorAt(1.0, QColor(70, 130, 180));
        p.fillRect(0, 0, w, h, bg);

        // A ball bouncing on the sine of the uptime clock proves the timer,
        // repaint and present path are all live.
        const unsigned ms = syscall3(15, 0, 0, 0);
        const double t = ms / 1000.0;
        const double bx = w / 2.0 + std::sin(t * 1.7) * (w / 2.0 - 30);
        const double by = h - 40 - std::fabs(std::sin(t * 2.3)) * (h / 3.0);
        QRadialGradient ball(bx - 6, by - 6, 22);
        ball.setColorAt(0.0, QColor(255, 240, 200));
        ball.setColorAt(1.0, QColor(220, 60, 40));
        p.setPen(Qt::NoPen);
        p.setBrush(ball);
        p.drawEllipse(QPointF(bx, by), 18, 18);

        QPainterPath panel;
        panel.addRoundedRect(QRectF(40, 50, w - 80, 96), 14, 14);
        p.setPen(QPen(QColor(255, 255, 255, 180), 2));
        p.setBrush(QColor(255, 255, 255, 40));
        p.drawPath(panel);

        QFont big;
        big.setPixelSize(48);   // 3 x Unifont's 16-px grid
        p.setFont(big);
        p.setPen(Qt::white);
        p.drawText(QRect(40, 54, w - 80, 56), Qt::AlignCenter, QStringLiteral("Hello, Qt6!"));

        QFont small;
        small.setPixelSize(16);  // 1 x the grid
        p.setFont(small);
        p.setPen(QColor(220, 235, 255));
        p.drawText(QRect(40, 112, w - 80, 24), Qt::AlignCenter,
                   QStringLiteral("real Qt 6.8 on koppios — Esc to quit"));
    }

    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return)
            QCoreApplication::quit();
    }
};

static char arg0[] = "/rd/hqtgui";
static char *fake_argv[] = {arg0, nullptr};

int main()
{
    qt_koppios_install_file_engine_handler();
    int argc = 1;
    QGuiApplication app(argc, fake_argv);

    HelloWindow window;
    window.show();

    QTimer frame;
    QObject::connect(&frame, &QTimer::timeout, &window, [&window] {
        window.update();
        // Raw scancode ring (syscall 26), same layout as keyboard.h: bit 16 =
        // valid event, bit 8 = E0-prefixed, bit 7 = release, low 7 bits = code.
        // Set-1 make codes: 0x01 = Esc, 0x1C = Enter.
        for (unsigned ev; ((ev = syscall3(26, 0, 0, 0)) & 0x10000) != 0;) {
            if (ev & 0x80)
                continue;                       // key release
            const unsigned code = ev & 0x7F;
            if (!(ev & 0x100) && code == 0x01)
                QWindowSystemInterface::handleKeyEvent(&window, QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            else if (!(ev & 0x100) && code == 0x1C)
                QWindowSystemInterface::handleKeyEvent(&window, QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        }
    });
    frame.start(50);

    const int rc = app.exec();
    qt_koppios_gfx_close();
    return rc;
}
