/* <stdlib.h> / <errno.h> / <locale.h> support for the koppi-os Lua port. */
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>

int errno = 0;

char *setlocale(int cat, const char *loc) {
    (void) cat;
    return (loc == 0 || loc[0] == 0 || loc[0] == 'C') ? (char *) "C" : 0;
}

struct lconv *localeconv(void) {
    static struct lconv c = { (char *) ".", (char *) "", (char *) "" };
    return &c;
}

void exit(int code);            /* lib/unistd.o (exit syscall) */

void abort(void) {
    exit(134);
    for (;;) ;
}

int abs(int x) { return x < 0 ? -x : x; }

char *getenv(const char *name) { (void) name; return 0; }

/* Only base 10 and 16 are needed (Lua's own integer parser handles the rest). */
long strtol(const char *s, char **end, int base) {
    const char *p = s;
    while (isspace((unsigned char) *p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-') neg = (*p++ == '-');
    if ((base == 16 || base == 0) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2; base = 16;
    } else if (base == 0) {
        base = 10;
    }
    long acc = 0;
    for (;; p++) {
        int d;
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if (*p >= 'a' && *p <= 'z') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z') d = *p - 'A' + 10;
        else break;
        if (d >= base) break;
        acc = acc * base + d;
    }
    if (end) *end = (char *) p;
    return neg ? -acc : acc;
}

/* Decimal floating point: [ws][sign]digits[.digits][(e|E)[sign]digits], plus
 * inf / nan. Hexadecimal floats go through Lua's own lua_strx2number. */
double strtod(const char *s, char **end) {
    const char *p = s;
    while (isspace((unsigned char) *p)) p++;

    int neg = 0;
    if (*p == '+' || *p == '-') neg = (*p++ == '-');

    if ((p[0] == 'i' || p[0] == 'I') && (p[1] == 'n' || p[1] == 'N') &&
        (p[2] == 'f' || p[2] == 'F')) {
        p += 3;
        if (end) *end = (char *) p;
        return neg ? -__builtin_huge_val() : __builtin_huge_val();
    }
    if ((p[0] == 'n' || p[0] == 'N') && (p[1] == 'a' || p[1] == 'A') &&
        (p[2] == 'n' || p[2] == 'N')) {
        p += 3;
        if (end) *end = (char *) p;
        return __builtin_nan("");
    }

    double mant = 0.0;
    int any = 0;
    while (*p >= '0' && *p <= '9') { mant = mant * 10.0 + (*p++ - '0'); any = 1; }
    if (*p == '.') {
        p++;
        double scale = 0.1;
        while (*p >= '0' && *p <= '9') {
            mant += (*p++ - '0') * scale;
            scale *= 0.1;
            any = 1;
        }
    }
    if (!any) {                 /* no digits consumed */
        if (end) *end = (char *) s;
        return 0.0;
    }

    int exp = 0, eneg = 0;
    if (*p == 'e' || *p == 'E') {
        const char *save = p++;
        if (*p == '+' || *p == '-') eneg = (*p++ == '-');
        if (*p >= '0' && *p <= '9') {
            while (*p >= '0' && *p <= '9') exp = exp * 10 + (*p++ - '0');
        } else {
            p = save;           /* malformed exponent: don't consume 'e' */
        }
    }

    /* Powers of ten up to 10^22 are exact in a double, so for those a single
     * multiply/divide of an exact mantissa is correctly rounded. Beyond that we
     * chunk by 10^22 and accept a small error in the (rare) tail. */
    static const double pow10[] = {
        1e0,1e1,1e2,1e3,1e4,1e5,1e6,1e7,1e8,1e9,1e10,1e11,1e12,1e13,1e14,1e15,
        1e16,1e17,1e18,1e19,1e20,1e21,1e22
    };
    double result = mant;
    int e = eneg ? -exp : exp;          /* signed decimal exponent */
    while (e > 22)  { result *= 1e22; e -= 22; }
    while (e < -22) { result /= 1e22; e += 22; }
    if (e >= 0)      result *= pow10[e];
    else             result /= pow10[-e];

    if (end) *end = (char *) p;
    return neg ? -result : result;
}
