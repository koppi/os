/**
 * @file third_party/qt6/koppios/qt_libc_compat.c
 * @brief The handful of real C standard library functions vendored Qt6
 *        source calls that this kernel's own minimal libc (lib/, see
 *        include/lib/) doesn't provide, and that don't belong there:
 *        fprintf/fputs/fflush/stderr/snprintf/getenv/abort exist here
 *        specifically to satisfy Qt's QT_BOOTSTRAPPED config (qlogging.cpp,
 *        qassert.cpp, qtenvironmentvariables.cpp, qlocale_tools.cpp), not
 *        as a general-purpose addition to this OS's own C library.
 *
 * %f/%e/%g formatting in format_into() is deliberately not accurate --
 * apps/hello-qt never formats a double, so it is never exercised, and a
 * real correct dtoa implementation is out of scope for what this file is
 * for. Good enough to link and not crash; not good enough for real
 * floating-point output.
 */
#include <stdarg.h>

typedef struct {
    int unused;
} FILE;
static FILE stderr_obj;
FILE *stderr = &stderr_obj;

extern int _write(const void *buf, unsigned int len);
extern int strlen(const char *s);

void abort(void) {
    for (;;) {
    }
}

int fflush(FILE *f) {
    (void) f;
    return 0;
}

int fputs(const char *s, FILE *f) {
    (void) f;
    _write(s, (unsigned int) strlen(s));
    return 0;
}

static int format_into(char *buf, unsigned int cap, const char *fmt, va_list ap) {
    unsigned int n = 0;
#define PUT(c)                       \
    do {                             \
        if (n + 1 < cap)             \
            buf[n] = (char) (c);     \
        n++;                         \
    } while (0)
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            PUT(*p);
            continue;
        }
        p++;
        /* skip width/precision/flags -- not implemented precisely */
        while (*p == '-' || *p == '+' || *p == '0' || *p == ' ' || *p == '#') p++;
        while (*p >= '0' && *p <= '9') p++;
        if (*p == '.') {
            p++;
            while (*p >= '0' && *p <= '9') p++;
        }
        switch (*p) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                for (; *s; s++) PUT(*s);
                break;
            }
            case 'c':
                PUT((char) va_arg(ap, int));
                break;
            case 'd':
            case 'i': {
                int v = va_arg(ap, int);
                unsigned int u = (v < 0) ? (unsigned int) (-(long) v) : (unsigned int) v;
                char tmp[16];
                int t = 0;
                if (v < 0) PUT('-');
                if (u == 0) tmp[t++] = '0';
                while (u) {
                    tmp[t++] = (char) ('0' + (u % 10));
                    u /= 10;
                }
                while (t) PUT(tmp[--t]);
                break;
            }
            case 'u':
            case 'x':
            case 'X': {
                unsigned int u = va_arg(ap, unsigned int);
                unsigned int base = (*p == 'u') ? 10 : 16;
                char tmp[16];
                int t = 0;
                if (u == 0) tmp[t++] = '0';
                while (u) {
                    unsigned int d = u % base;
                    tmp[t++] = (char) (d < 10 ? '0' + d : 'a' + d - 10);
                    u /= base;
                }
                while (t) PUT(tmp[--t]);
                break;
            }
            case 'f':
            case 'e':
            case 'g': {
                /* Not accurate -- see file comment. */
                double v = va_arg(ap, double);
                long whole = (long) v;
                char tmp[16];
                int t = 0;
                unsigned long uw = (whole < 0) ? (unsigned long) (-whole) : (unsigned long) whole;
                if (whole < 0) PUT('-');
                if (uw == 0) tmp[t++] = '0';
                while (uw) {
                    tmp[t++] = (char) ('0' + (uw % 10));
                    uw /= 10;
                }
                while (t) PUT(tmp[--t]);
                PUT('.');
                PUT('0');
                break;
            }
            case '%':
                PUT('%');
                break;
            default:
                PUT('%');
                PUT(*p);
                break;
        }
    }
    if (cap) buf[n < cap ? n : cap - 1] = 0;
#undef PUT
    return (int) n;
}

int fprintf(FILE *f, const char *fmt, ...) {
    (void) f;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = format_into(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    _write(buf, (unsigned int) (n < (int) sizeof(buf) ? n : (int) sizeof(buf) - 1));
    return n;
}

int snprintf(char *buf, unsigned int cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = format_into(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}

char *getenv(const char *name) {
    (void) name;
    return 0;
}
