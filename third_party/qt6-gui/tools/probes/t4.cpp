/* Heap probe: large allocations through the libc paths Qt uses. */
#include <QImage>
#include <cstdlib>
#include <cstring>
extern "C" unsigned _write(const void *buf, unsigned len);
static unsigned sl(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static void rep(const char *m) { _write(m, sl(m)); }
static void repn(const char *l, unsigned long v) {
    char b[24]; int n = 0; char t[24]; int k = 0;
    do { t[k++] = '0' + v % 10; v /= 10; } while (v);
    while (k) b[n++] = t[--k];
    b[n++] = '\n'; b[n] = 0; rep(l); rep(b);
}
int main() {
    const unsigned long sizes[] = {1000, 100000, 300000, 518400, 2000000};
    for (unsigned long sz : sizes) {
        unsigned char *p = (unsigned char *) malloc(sz);
        repn("malloc size: ", sz); repn("  addr: ", (unsigned long) p);
        if (p) { memset(p, 0x5a, sz); rep("  memset ok\n"); }
        unsigned char *c = (unsigned char *) calloc(1, sz);
        repn("calloc addr: ", (unsigned long) c);
        if (c) { unsigned long bad = 0; for (unsigned long i = 0; i < sz; i++) bad += c[i]; repn("  calloc nonzero sum: ", bad); memset(c, 1, sz); rep("  memset ok\n"); }
    }
    rep("-- QImage\n");
    QImage img(480, 270, QImage::Format_ARGB32);
    repn("img bits: ", (unsigned long) img.bits());
    repn("img bytes: ", img.sizeInBytes());
    img.fill(Qt::white);
    rep("QImage fill ok\n");
    return 0;
}
