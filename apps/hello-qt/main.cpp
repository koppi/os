/**
 * @file apps/hello-qt/main.cpp
 * @brief First real Qt6 userspace app: real, vendored Qt 6.8.4 QString
 *        source (third_party/qt6/, QT_BOOTSTRAPPED config -- no QObject, no
 *        threading, see third_party/qt6/README.md) linked against this
 *        kernel's own runtime.
 *
 * Self-checking like every other STL-container demo in apps/, for the same
 * reason: a broken build of a huge third-party dependency is exactly the
 * kind of thing that "looks fine" until a specific code path is wrong.
 *
 * No koppios <stdio.h> here -- same reasoning as apps/hello-str: real Qt
 * headers transitively reach the real system <cstdio>, which conflicts
 * with our own. Output goes through _write (syscall 12) instead.
 */
#include <QString>
#include <QStringList>

extern "C" unsigned int _write(const void *buf, unsigned int len);

static void put(const char *s) {
    unsigned int n = 0;
    while (s[n]) {
        n++;
    }
    _write(s, n);
}

static void put(const QString &s) {
    QByteArray utf8 = s.toUtf8();
    _write(utf8.constData(), (unsigned int) utf8.size());
}

int main() {
    bool ok = true;

    QString hello = QString("Hello") + " " + "World";
    put(hello);
    put("\n");
    if (hello != QLatin1String("Hello World")) {
        put("FAIL concat\n");
        ok = false;
    }

    QString tail = hello.mid(6);
    put("mid(6): ");
    put(tail);
    put("\n");
    if (tail != QLatin1String("World")) {
        put("FAIL mid\n");
        ok = false;
    }

    QString num = QString::number(42);
    put("number(42): ");
    put(num);
    put("\n");
    if (num != QLatin1String("42")) {
        put("FAIL number\n");
        ok = false;
    }

    QStringList parts = hello.split(QLatin1Char(' '));
    put("split count: ");
    put(QString::number(parts.size()));
    put("\n");
    if (parts.size() != 2 || parts[0] != QLatin1String("Hello") || parts[1] != QLatin1String("World")) {
        put("FAIL split\n");
        ok = false;
    }

    QString upper = hello.toUpper();
    put("toUpper: ");
    put(upper);
    put("\n");
    if (upper != QLatin1String("HELLO WORLD")) {
        put("FAIL toUpper\n");
        ok = false;
    }

    put(ok ? "PASS\n" : "OVERALL FAIL\n");
    return ok ? 0 : 1;
}
