/*
 * The remaining C-library entry points this port needs and neither lib/ nor
 * shim/stdio_koppios.c provides. Everything here is small, real and
 * self-contained; where this kernel has no backing concept the function says
 * so rather than inventing an answer.
 *
 * The exact set was taken from the link, not from guesswork: compile the
 * whole tracker, diff its undefined symbols against what lib/string.o,
 * lib/stdlib.o, lib/libm.o, lib/cxxabi.o and lib/libc_ext.o define, and this
 * is what is left over.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "ksys.h"

/* ------------------------------------------------------------------ *
 *  printf family                                                      *
 * ------------------------------------------------------------------ */

/*
 * Vendored mpaland printf (apps/doom/shim/printf.c) with its SOFT alias
 * config, which defines only the trailing-underscore names; these are the
 * standard spellings on top. lib/libc_ext.c has its own snprintf, but it
 * ignores width and precision entirely -- "%02X" comes out as "A" -- and the
 * tracker's whole 40x20 grid is built out of those, so this app compiles
 * libc_ext.c with -DKOPPIOS_APP_STDIO and uses the real formatter.
 */
int vsnprintf_(char *s, size_t count, const char *format, va_list arg);
int vprintf_(const char *format, va_list arg);

int koppios_vsnprintf(char *s, size_t n, const char *fmt, va_list ap) __asm__("vsnprintf");
int koppios_vsnprintf(char *s, size_t n, const char *fmt, va_list ap) {
    return vsnprintf_(s, n, fmt, ap);
}

int koppios_snprintf(char *s, size_t n, const char *fmt, ...) __asm__("snprintf");
int koppios_snprintf(char *s, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf_(s, n, fmt, ap);
    va_end(ap);
    return r;
}

int koppios_printf(const char *fmt, ...) __asm__("printf");
int koppios_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf_(fmt, ap);
    va_end(ap);
    return r;
}

/* _FORTIFY_SOURCE is off for this app's own code, but libsupc++-style objects
 * and anything else built with it on call the _chk forms. */
int __snprintf_chk(char *s, size_t n, int flag, size_t slen, const char *fmt, ...) {
    (void) flag; (void) slen;
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf_(s, n, fmt, ap);
    va_end(ap);
    return r;
}

/* ------------------------------------------------------------------ *
 *  ctype                                                              *
 * ------------------------------------------------------------------ */

/*
 * Real functions, not the __ctype_b_loc() table lookups glibc's macros
 * expand to in C: this app is C++, where <ctype.h> declares these as
 * functions and the call needs a symbol. (lib/libc_ext.c supplies the table
 * for anything that does use the macro form.)
 *
 * The #undefs are for this file only: in C, which this file is, glibc's
 * <ctype.h> defines the same names as macros.
 */
#undef isspace
#undef isxdigit

int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

int isxdigit(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* ------------------------------------------------------------------ *
 *  strings                                                            *
 * ------------------------------------------------------------------ */

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int strcasecmp(const char *a, const char *b) {
    for (;; a++, b++) {
        int ca = lower((unsigned char) *a), cb = lower((unsigned char) *b);
        if (ca != cb) return ca - cb;
        if (!ca) return 0;
    }
}

int strncasecmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) {
        int ca = lower((unsigned char) *a), cb = lower((unsigned char) *b);
        if (ca != cb) return ca - cb;
        if (!ca) return 0;
    }
    return 0;
}

char *strtok(char *s, const char *delim) {
    static char *save;
    if (!s) s = save;
    if (!s) return NULL;

    while (*s && strchr(delim, (unsigned char) *s))
        s++;
    if (!*s) { save = NULL; return NULL; }

    char *tok = s;
    while (*s && !strchr(delim, (unsigned char) *s))
        s++;
    if (*s) { *s = 0; save = s + 1; } else { save = NULL; }
    return tok;
}

/* ------------------------------------------------------------------ *
 *  strtod                                                             *
 * ------------------------------------------------------------------ */

/*
 * Decimal (and hex-float-free) strtod, used by this port's sscanf for the
 * two `%f` fields in the project format -- the tick rate and the mix volume,
 * both written back with "%f", so the round trip has to land on the same
 * value. Digits are accumulated as an integer mantissa and scaled once at the
 * end rather than multiplied in per digit, which keeps the result to within
 * an ulp or two instead of accumulating one rounding error per digit.
 */
double strtod(const char *s, char **end) {
    const char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ||
           *p == '\v' || *p == '\f')
        p++;

    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }

    unsigned long long mant = 0;
    int digits = 0;      /* significant digits consumed into mant */
    int exp10 = 0;       /* decimal exponent to apply to mant */
    int any = 0;

    for (; *p >= '0' && *p <= '9'; p++) {
        any = 1;
        if (digits < 19) { mant = mant * 10u + (unsigned) (*p - '0'); digits++; }
        else exp10++;    /* too many digits to keep: scale instead */
    }

    if (*p == '.') {
        p++;
        for (; *p >= '0' && *p <= '9'; p++) {
            any = 1;
            if (digits < 19) { mant = mant * 10u + (unsigned) (*p - '0'); digits++; exp10--; }
        }
    }

    if (!any) {
        if (end) *end = (char *) s;
        return 0.0;
    }

    if (*p == 'e' || *p == 'E') {
        const char *q = p + 1;
        int eneg = 0;
        if (*q == '+' || *q == '-') { eneg = (*q == '-'); q++; }
        if (*q >= '0' && *q <= '9') {
            int e = 0;
            while (*q >= '0' && *q <= '9') {
                if (e < 100000) e = e * 10 + (*q - '0');
                q++;
            }
            exp10 += eneg ? -e : e;
            p = q;
        }
    }

    double v = (double) mant;
    /* Scale by repeated squaring, so the result takes O(log|exp10|) roundings
     * rather than |exp10| of them. */
    if (exp10) {
        double base = (exp10 > 0) ? 10.0 : 0.1;
        int n = (exp10 > 0) ? exp10 : -exp10;
        double f = 1.0;
        while (n) {
            if (n & 1) f *= base;
            base *= base;
            n >>= 1;
        }
        v *= f;
    }

    if (end) *end = (char *) p;
    return neg ? -v : v;
}

double atof(const char *s) { return strtod(s, NULL); }

/* ------------------------------------------------------------------ *
 *  math                                                               *
 * ------------------------------------------------------------------ */

/*
 * lib/libm.o has the rest of the float set (expf, sqrtf, floorf, ceilf,
 * sinf, cosf, powf, logf) but not this one -- GCC inlines fabsf for most
 * callers, so nothing had needed the symbol until the AY period search in
 * playback_ay.cpp took its address in a comparison chain.
 */
float fabsf(float x) { return x < 0.0f ? -x : x; }

/* ------------------------------------------------------------------ *
 *  rand / time                                                        *
 * ------------------------------------------------------------------ */

/*
 * ChipNomad's one rand() call is the AY noise generator's seed for the
 * waveform display. A plain LCG is all that needs; the constants are the
 * ones C99's own example uses.
 */
static unsigned long rand_state = 1;

int rand(void) {
    rand_state = rand_state * 1103515245u + 12345u;
    return (int) ((rand_state >> 16) & 0x7FFF);
}

void srand(unsigned seed) { rand_state = seed; }

clock_t clock(void) {
    /* The PIT millisecond counter. CLOCKS_PER_SEC is 1000000 in the real
     * headers, so scale to it rather than reporting milliseconds as if they
     * were microseconds. */
    return (clock_t) (ksys0(SYS_CLOCK) * (CLOCKS_PER_SEC / 1000));
}

time_t time(time_t *t) {
    time_t now = (time_t) ksys0(SYS_TIME);
    if (t) *t = now;
    return now;
}
