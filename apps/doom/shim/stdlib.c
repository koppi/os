/* <stdlib.h> implementation for the koppi-os Doom port. */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "ksys.h"

int errno;

void *malloc(size_t n)             { return (void *) ksys1(SYS_MALLOC, n); }
void  free(void *p)                { ksys1(SYS_FREE, (unsigned long) p); }
void *realloc(void *p, size_t n)   { return (void *) ksys2(SYS_REALLOC, (unsigned long) p, n); }

void *calloc(size_t n, size_t sz) {
    size_t total = n * sz;
    if (sz && total / sz != n)       /* overflow */
        return 0;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

/* ---------------- exit ---------------- */

/*
 * A tiny atexit(). The engine's own I_AtExit list only runs on the paths that
 * reach I_Quit or I_Error; this one also covers a plain exit(), which is how
 * the port makes sure the screen is given back however the game ends.
 */
#define ATEXIT_MAX 8
static void (*atexit_fns[ATEXIT_MAX])(void);
static int atexit_n;
static int atexit_running;

int atexit(void (*fn)(void)) {
    if (!fn || atexit_n >= ATEXIT_MAX)
        return -1;
    atexit_fns[atexit_n++] = fn;
    return 0;
}

void exit(int code) {
    if (!atexit_running) {           /* an exit() from a handler must not loop */
        atexit_running = 1;
        while (atexit_n > 0)
            atexit_fns[--atexit_n]();
    }
    ksys1(SYS_EXIT, (unsigned long) code);
    for (;;) { }                     /* not reached */
}

void abort(void) { exit(1); }

/* ---------------- numbers ---------------- */

int abs(int v) { return v < 0 ? -v : v; }

long strtol(const char *s, char **end, int base) {
    const char *p = s;
    while (isspace((unsigned char) *p)) p++;

    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }

    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')
        && isxdigit((unsigned char) p[2])) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = (p[0] == '0') ? 8 : 10;
    }

    const char *start = p;
    long v = 0;
    for (;;) {
        int c = (unsigned char) *p, d;
        if (isdigit(c))      d = c - '0';
        else if (isalpha(c)) d = (c | 32) - 'a' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        p++;
    }

    if (end)
        *end = (char *) (p == start ? s : p);
    return neg ? -v : v;
}

int atoi(const char *s) { return (int) strtol(s, 0, 10); }

/*
 * strtod: decimal digits, an optional fraction and an optional exponent.
 * The engine has exactly one float setting (mouse acceleration), so this does
 * not need the hex-float or inf/nan forms.
 */
double strtod(const char *s, char **end) {
    const char *p = s;
    while (isspace((unsigned char) *p)) p++;

    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }

    const char *start = p;
    double v = 0.0;
    while (isdigit((unsigned char) *p))
        v = v * 10.0 + (*p++ - '0');

    if (*p == '.') {
        p++;
        double scale = 0.1;
        while (isdigit((unsigned char) *p)) {
            v += (*p++ - '0') * scale;
            scale *= 0.1;
        }
    }

    if (p == start) {                /* nothing consumed */
        if (end) *end = (char *) s;
        return 0.0;
    }

    if (*p == 'e' || *p == 'E') {
        const char *save = p;
        p++;
        int eneg = 0;
        if (*p == '+' || *p == '-') { eneg = (*p == '-'); p++; }
        if (isdigit((unsigned char) *p)) {
            int e = 0;
            while (isdigit((unsigned char) *p))
                e = e * 10 + (*p++ - '0');
            if (e > 308) e = 308;
            double f = 1.0;
            while (e--) f *= 10.0;
            v = eneg ? v / f : v * f;
        } else {
            p = save;                /* "1e" is a number followed by 'e' */
        }
    }

    if (end) *end = (char *) p;
    return neg ? -v : v;
}

double atof(const char *s) { return strtod(s, 0); }

/* ---------------- environment ---------------- */

/* No environment: every getenv() in the live code paths (DOOMWADDIR, TEMP)
 * has a sensible NULL branch. */
char *getenv(const char *name) { (void) name; return 0; }

/* No subprocesses. i_system.c probes for /usr/bin/zenity with this and takes
 * the "no GUI error box" path when it fails, which is what we want. */
int system(const char *cmd) { (void) cmd; return -1; }
