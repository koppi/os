/**
 * @file apps/hello-qt-widgets/hello_qt_widgets.cpp
 * @brief A QtWidgets test app: tabs, buttons, check and radio boxes, sliders, a
 *        dial, a scroll bar, a progress bar, a spin box, a line edit and a
 *        combo box that switches the application style at run time.
 *
 * Real Qt 6.8 QtWidgets (Fusion and Windows styles) painted into the kernel
 * framebuffer by the platform glue in third_party/qt6-gui/koppios/. Keyboard
 * (Tab, Shift+Tab, arrows, Space, Enter, typing) and mouse both work; Esc quits.
 *
 * Every state change is also written to the serial console as "widgets: ...",
 * which is how test/qt-widgets-boot.sh checks behaviour rather than pixels.
 */
#include <QApplication>
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QSlider>
#include <QDial>
#include <QScrollBar>
#include <QProgressBar>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStyle>
#include <QStyleFactory>
#include <QKeyEvent>
#include <QFont>
#include <QAbstractButton>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QAtomicInt>
#include <QList>
#include <QtCore/qthreadpool.h>
#include <QtCore/qrunnable.h>
#include <QtCore/private/qthreadpool_p.h>
#include <QTimer>

extern "C" unsigned _write(const void *buf, unsigned len);
void qt_koppios_install_file_engine_handler();
void qt_koppios_gfx_close();

static void say(const QString &text)
{
    const QByteArray line = "widgets: " + text.toUtf8() + "\n";
    _write(line.constData(), unsigned(line.size()));
}

// ---- Qt threads: real thread_local storage, QThread, QThreadPool, cross-thread signals ------------

static thread_local int tl_marker;               // every thread has its own copy

class Worker : public QThread
{
public:
    int sum = 0;
    bool ownThread = false;
    int tlAtEnd = -1;

protected:
    void run() override
    {
        ownThread = QThread::currentThread() == this;
        tl_marker = 0;
        for (int i = 0; i < 100; i++) {
            sum += i;
            ++tl_marker;
            if (i % 25 == 0)
                QThread::yieldCurrentThread();
        }
        tlAtEnd = tl_marker;
    }
};

struct Job : QRunnable
{
    QMutex *mutex = nullptr;
    QList<QThread *> *seen = nullptr;
    QAtomicInt *sum = nullptr;
    int n = 0;

    void run() override
    {
        QThread::msleep(20);                       // long enough for the pool to bring up a second worker
        sum->fetchAndAddRelaxed(n);
        QMutexLocker locker(mutex);
        if (!seen->contains(QThread::currentThread()))
            seen->append(QThread::currentThread());
    }
};

// Runs before the event loop starts and logs what it saw, so test/qt-widgets-boot.sh can check that
// Qt's threads really work and not only that nothing crashed. The queued `finished` connection is
// delivered once the main thread is in exec().
static void threadSelfTest(QObject *context)
{
    tl_marker = 1000;

    Worker w;
    QObject::connect(&w, &QThread::finished, context,
                     [] { say(QStringLiteral("threads: finished signal reached the main thread")); },
                     Qt::QueuedConnection);
    w.start();
    const bool done = w.wait(5000);
    say(QStringLiteral("threads: QThread done=%1 sum=%2 own_thread=%3 its_tl=%4 main_tl=%5")
            .arg(int(done)).arg(w.sum).arg(int(w.ownThread)).arg(w.tlAtEnd).arg(tl_marker));

    QThreadPool pool;
    pool.setMaxThreadCount(4);
    QMutex mutex;
    QList<QThread *> seen;
    QAtomicInt sum(0);
    for (int n = 1; n <= 8; n++) {
        auto *job = new Job;
        job->mutex = &mutex;
        job->seen = &seen;
        job->sum = &sum;
        job->n = n;
        pool.start(job);
    }
    pool.waitForDone();
    say(QStringLiteral("threads: QThreadPool jobs=8 sum=%1 parallel=%2")
            .arg(sum.loadRelaxed()).arg(int(seen.size() >= 2)));
}

class MainWindow : public QWidget
{
protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            say(QStringLiteral("quit"));
            QCoreApplication::quit();
        } else {
            QWidget::keyPressEvent(e);
        }
    }
};

static QWidget *buttonsPage()
{
    auto *page = new QWidget;
    auto *grid = new QGridLayout(page);

    auto *click = new QPushButton(QStringLiteral("Click me"));
    auto *count = new QLabel(QStringLiteral("clicks: 0"));
    auto *clicks = new int(0);
    QObject::connect(click, &QPushButton::clicked, page, [=] {
        ++*clicks;
        count->setText(QStringLiteral("clicks: %1").arg(*clicks));
        say(QStringLiteral("button clicked %1").arg(*clicks));
    });

    auto *toggle = new QPushButton(QStringLiteral("Toggle me"));
    toggle->setCheckable(true);
    QObject::connect(toggle, &QPushButton::toggled, page, [](bool on) {
        say(QStringLiteral("toggle %1").arg(on ? "on" : "off"));
    });

    auto *check = new QCheckBox(QStringLiteral("Enable the thing"));
    QObject::connect(check, &QCheckBox::toggled, page, [](bool on) {
        say(QStringLiteral("checkbox %1").arg(on ? "on" : "off"));
    });

    auto *modes = new QGroupBox(QStringLiteral("Mode"));
    auto *modeLayout = new QVBoxLayout(modes);
    auto *group = new QButtonGroup(modes);
    const char *names[] = {"Fast", "Balanced", "Careful"};
    for (int i = 0; i < 3; i++) {
        auto *r = new QRadioButton(QString::fromLatin1(names[i]));
        if (i == 1)
            r->setChecked(true);
        group->addButton(r, i);
        modeLayout->addWidget(r);
    }
    QObject::connect(group, &QButtonGroup::idClicked, page, [=](int id) {
        say(QStringLiteral("radio %1").arg(QString::fromLatin1(names[id])));
    });

    auto *tool = new QToolButton;
    tool->setText(QStringLiteral("Tool button"));
    tool->setCheckable(true);
    QObject::connect(tool, &QToolButton::toggled, page, [](bool on) {
        say(QStringLiteral("toolbutton %1").arg(on ? "on" : "off"));
    });

    grid->addWidget(click, 0, 0);
    grid->addWidget(count, 0, 1);
    grid->addWidget(toggle, 1, 0);
    grid->addWidget(tool, 1, 1);
    grid->addWidget(check, 2, 0, 1, 2);
    grid->addWidget(modes, 0, 2, 3, 1);
    grid->setRowStretch(3, 1);
    return page;
}

static QWidget *slidersPage()
{
    auto *page = new QWidget;
    auto *grid = new QGridLayout(page);

    auto *slider = new QSlider(Qt::Horizontal);
    auto *vslider = new QSlider(Qt::Vertical);
    auto *dial = new QDial;
    auto *scroll = new QScrollBar(Qt::Horizontal);
    auto *spin = new QSpinBox;
    auto *bar = new QProgressBar;
    for (QAbstractSlider *s : {static_cast<QAbstractSlider *>(slider), static_cast<QAbstractSlider *>(vslider),
                               static_cast<QAbstractSlider *>(dial), static_cast<QAbstractSlider *>(scroll)})
        s->setRange(0, 100);
    spin->setRange(0, 100);
    bar->setRange(0, 100);
    dial->setNotchesVisible(true);

    // One value, six views: whichever control changes drives the others.
    auto set = [=](int v) {
        for (QAbstractSlider *s : {static_cast<QAbstractSlider *>(slider), static_cast<QAbstractSlider *>(vslider),
                                   static_cast<QAbstractSlider *>(dial), static_cast<QAbstractSlider *>(scroll)})
            s->setValue(v);
        spin->setValue(v);
        bar->setValue(v);
    };
    for (QAbstractSlider *s : {static_cast<QAbstractSlider *>(slider), static_cast<QAbstractSlider *>(vslider),
                               static_cast<QAbstractSlider *>(dial), static_cast<QAbstractSlider *>(scroll)})
        QObject::connect(s, &QAbstractSlider::valueChanged, page, set);
    QObject::connect(spin, &QSpinBox::valueChanged, page, set);
    QObject::connect(slider, &QSlider::valueChanged, page, [](int v) { say(QStringLiteral("slider %1").arg(v)); });
    QObject::connect(spin, &QSpinBox::valueChanged, page, [](int v) { say(QStringLiteral("spinbox %1").arg(v)); });
    QObject::connect(dial, &QDial::valueChanged, page, [](int v) { say(QStringLiteral("dial %1").arg(v)); });
    QObject::connect(scroll, &QScrollBar::valueChanged, page, [](int v) { say(QStringLiteral("scrollbar %1").arg(v)); });

    grid->addWidget(new QLabel(QStringLiteral("Slider")), 0, 0);
    grid->addWidget(slider, 0, 1, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("Scroll bar")), 1, 0);
    grid->addWidget(scroll, 1, 1, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("Spin box")), 2, 0);
    grid->addWidget(spin, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("Progress")), 3, 0);
    grid->addWidget(bar, 3, 1, 1, 2);
    grid->addWidget(dial, 0, 3, 4, 1);
    grid->addWidget(vslider, 0, 4, 4, 1);
    grid->setColumnStretch(2, 1);
    set(25);
    return page;
}

static QWidget *inputPage()
{
    auto *page = new QWidget;
    auto *grid = new QGridLayout(page);

    auto *edit = new QLineEdit;
    edit->setPlaceholderText(QStringLiteral("type here"));
    auto *echo = new QLabel(QStringLiteral("(nothing typed yet)"));
    QObject::connect(edit, &QLineEdit::textChanged, page, [=](const QString &t) {
        echo->setText(t.isEmpty() ? QStringLiteral("(nothing typed yet)") : QStringLiteral("you typed: ") + t);
        say(QStringLiteral("text '%1'").arg(t));
    });

    auto *combo = new QComboBox;
    combo->addItems(QStyleFactory::keys());
    combo->setCurrentIndex(qMax(0, combo->findText(QApplication::style()->objectName(), Qt::MatchFixedString)));   // case-insensitive
    QObject::connect(combo, &QComboBox::activated, page, [=](int i) {
        const QString name = combo->itemText(i);
        say(QStringLiteral("combo %1 '%2'").arg(i).arg(name));
        if (QStyle *style = QStyleFactory::create(name))
            QApplication::setStyle(style);
    });

    auto *fruit = new QComboBox;
    fruit->addItems({QStringLiteral("Apple"), QStringLiteral("Banana"), QStringLiteral("Cherry"), QStringLiteral("Damson")});
    QObject::connect(fruit, &QComboBox::activated, page, [=](int i) {
        say(QStringLiteral("fruit %1 '%2'").arg(i).arg(fruit->itemText(i)));
    });

    grid->addWidget(new QLabel(QStringLiteral("Line edit")), 0, 0);
    grid->addWidget(edit, 0, 1);
    grid->addWidget(echo, 1, 1);
    grid->addWidget(new QLabel(QStringLiteral("Style")), 2, 0);
    grid->addWidget(combo, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("Fruit")), 3, 0);
    grid->addWidget(fruit, 3, 1);
    grid->setRowStretch(4, 1);
    grid->setColumnStretch(1, 1);
    return page;
}

static QWidget *aboutPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *label = new QLabel;
    label->setWordWrap(true);
    label->setText(QStringLiteral("Qt %1 QtWidgets on koppios\nStyle: %2\nFont: %3 %4px\nScreen: %5x%6")
                       .arg(QString::fromLatin1(qVersion()), QApplication::style()->objectName(),
                            QApplication::font().family()).arg(QApplication::font().pixelSize())
                       .arg(QGuiApplication::primaryScreen()->size().width())
                       .arg(QGuiApplication::primaryScreen()->size().height()));
    layout->addWidget(label);
    layout->addStretch(1);
    return page;
}

static char arg0[] = "/rd/hqtwid";
static char *fake_argv[] = {arg0, nullptr};

int main()
{
    qt_koppios_install_file_engine_handler();
    int argc = 1;
    QApplication app(argc, fake_argv);

    QFont font;
    font.setPixelSize(16);                         // one cell of Unifont's 16-px grid
    app.setFont(font);

    threadSelfTest(&app);

    MainWindow window;
    window.setWindowTitle(QStringLiteral("Qt widgets on koppios"));
    auto *root = new QVBoxLayout(&window);
    auto *tabs = new QTabWidget;
    tabs->addTab(buttonsPage(), QStringLiteral("Buttons"));
    tabs->addTab(slidersPage(), QStringLiteral("Sliders"));
    tabs->addTab(inputPage(), QStringLiteral("Input"));
    tabs->addTab(aboutPage(), QStringLiteral("About"));
    QObject::connect(tabs, &QTabWidget::currentChanged, &window, [](int i) { say(QStringLiteral("tab %1").arg(i)); });

    auto *bottom = new QHBoxLayout;
    auto *hint = new QLabel(QStringLiteral("Tab / arrows / Space / mouse   -   Esc quits"));
    auto *quit = new QPushButton(QStringLiteral("Quit"));
    QObject::connect(quit, &QPushButton::clicked, &window, [] {
        say(QStringLiteral("quit"));
        QCoreApplication::quit();
    });
    bottom->addWidget(hint, 1);
    bottom->addWidget(quit);

    root->addWidget(tabs, 1);
    root->addLayout(bottom);

    QObject::connect(&app, &QApplication::focusChanged, &window, [](QWidget *, QWidget *now) {
        if (!now)
            return;
        QString what = QString::fromLatin1(now->metaObject()->className());
        if (auto *b = qobject_cast<QAbstractButton *>(now))
            what += QStringLiteral(" '%1'").arg(b->text());
        say(QStringLiteral("focus %1").arg(what));
    });

    window.setGeometry(QRect(QPoint(0, 0), QGuiApplication::primaryScreen()->size()));
    window.show();

    // Qt hands big image fills and conversions to its own GUI thread pool; once the first frames are
    // painted it must have started a worker (this machine reports one ideal thread, so exactly one).
    QTimer::singleShot(300, &window, [] {
        QThreadPool *gui = QThreadPoolPrivate::qtGuiInstance();
        int workers = -1;
        if (gui) {
            auto *d = static_cast<QThreadPoolPrivate *>(QObjectPrivate::get(gui));
            const QMutexLocker locker(&d->mutex);
            workers = int(d->allThreads.size());
        }
        say(QStringLiteral("threads: GUI pool workers=%1").arg(workers));
    });

    say(QStringLiteral("ready style=%1 tabs=%2").arg(QApplication::style()->objectName()).arg(tabs->count()));

    const int rc = app.exec();
    qt_koppios_gfx_close();
    return rc;
}
