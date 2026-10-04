#include <QGuiApplication>

extern "C" unsigned _write(const void *buf, unsigned len);

static unsigned koppios_strlen(const char *s) {
    unsigned n = 0;
    while (s[n]) n++;
    return n;
}
static void report(const char *msg) { _write(msg, koppios_strlen(msg)); }

static char arg0[] = "/rd/hqtgui";
static char *fake_argv[] = {arg0, 0};

int main(void) {
    report("marker: before QGuiApplication\n");
    int argc = 1;
    char **argv = fake_argv;
    QGuiApplication app(argc, argv);
    report("marker: after QGuiApplication ctor\n");
    return 0;
}
