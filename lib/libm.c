/**
 * @file lib/libm.c
 * @brief A minimal, real (not approximated) libm for this -m32 target,
 *        backed directly by the x87 FPU's own hardware transcendental
 *        instructions (fsin/fcos/fptan/fsqrt/fpatan/f2xm1/fyl2x) rather
 *        than a software polynomial approximation. i386 has always had
 *        these in hardware; real glibc's own historical i386 sysdeps
 *        used exactly this approach before SSE2 became universal.
 *
 * There is no libm.a for this target (this whole tree is -nostdlib), and
 * the host's own static libm.a is not usable here: it pulls in glibc's
 * IFUNC CPU-dispatch machinery (_dl_x86_cpu_features) and real errno --
 * same class of problem as lib/cxx_pmr.cpp's memory_resource.o trap, just
 * deeper. Writing real hardware-instruction-backed functions sidesteps
 * that entirely: no OS dependency, no errno, no CPU dispatch needed.
 *
 * exp2()/log2() (f2xm1 + fyl2x, the textbook x87 sequences for these) are
 * the two building blocks expf/logf/pow/powf are composed from; acos()
 * and hypot() are composed from atan2()/sqrt() the same way. floor/ceil/
 * round/trunc/fmod need no FPU trickery at all -- plain truncating casts
 * are exact and portable for every value this kernel's UI/text code
 * actually produces (window coordinates, font metrics, color values),
 * all always comfortably within a 64-bit integer's range.
 */

static double x87_sqrt(double x) {
    double result;
    __asm__("fsqrt" : "=t"(result) : "0"(x));
    return result;
}

static double x87_fabs(double x) {
    double result;
    __asm__("fabs" : "=t"(result) : "0"(x));
    return result;
}

/* atan2(y, x): fpatan computes atan(st(1)/st(0)) in place, popping one
 * level -- "0"(x) binds x to st(0), "u"(y) binds y to st(1), matching
 * the instruction's expected stack order. */
static double x87_atan2(double y, double x) {
    double result;
    __asm__("fpatan" : "=t"(result) : "0"(x), "u"(y));
    return result;
}

/* 2^x, valid for any double x: frndint splits x into an integer part
 * (round-to-nearest, so the remainder is always within f2xm1's required
 * [-1,1] domain) and a fractional part; f2xm1 handles the fractional
 * part, fscale reapplies the integer part as a power-of-two multiply. */
static double x87_exp2(double x) {
    double result;
    __asm__(
        "fld %%st(0)\n\t"
        "frndint\n\t"
        "fsub %%st(0), %%st(1)\n\t" /* st(1) = st(0) - st(1) = round(x) - x = -frac */
        "fxch\n\t"
        "fchs\n\t" /* negate: st(0) = frac */
        "f2xm1\n\t"
        "fld1\n\t"
        "faddp\n\t"
        "fscale\n\t"
        "fstp %%st(1)"
        : "=t"(result)
        : "0"(x));
    return result;
}

/* log2(x): fyl2x computes st(1)*log2(st(0)) in place -- with st(1)=1.0
 * this is exactly log2(x). */
static double x87_log2(double x) {
    double result;
    __asm__(
        "fld1\n\t"
        "fxch\n\t"
        "fyl2x"
        : "=t"(result)
        : "0"(x));
    return result;
}

double sqrt(double x) { return x87_sqrt(x); }
float sqrtf(float x) { return (float) x87_sqrt((double) x); }

double fabs(double x) { return x87_fabs(x); }

double sin(double x) {
    double result;
    __asm__("fsin" : "=t"(result) : "0"(x));
    return result;
}
float sinf(float x) { return (float) sin((double) x); }

double cos(double x) {
    double result;
    __asm__("fcos" : "=t"(result) : "0"(x));
    return result;
}
float cosf(float x) { return (float) cos((double) x); }

double tan(double x) {
    double result;
    /* fptan leaves tan(x) in st(1) and pushes 1.0 onto st(0); fstp
     * %st(0) discards that placeholder, promoting the real result to
     * the new st(0) that "=t" reads. */
    __asm__("fptan\n\tfstp %%st(0)" : "=t"(result) : "0"(x));
    return result;
}
float tanf(float x) { return (float) tan((double) x); }

double atan2(double y, double x) { return x87_atan2(y, x); }

double acos(double x) { return x87_atan2(x87_sqrt(1.0 - x * x), x); }

double hypot(double x, double y) {
    x = x87_fabs(x);
    y = x87_fabs(y);
    if (x < y) {
        double t = x;
        x = y;
        y = t;
    }
    if (x == 0.0)
        return 0.0;
    double r = y / x;
    return x * x87_sqrt(1.0 + r * r);
}
float hypotf(float x, float y) { return (float) hypot((double) x, (double) y); }

/* ln(2) and log2(e), used to convert between exp2()/log2() and the
 * natural-log-based expf()/logf()/pow() this closure actually calls. */
#define LN2 0.6931471805599453
#define LOG2E 1.4426950408889634

float expf(float x) { return (float) x87_exp2((double) x * LOG2E); }
float logf(float x) { return (float) (x87_log2((double) x) * LN2); }

/* The double-precision siblings of expf()/logf(): same x87 sequences, no float round trip. */
double atan(double x) { return x87_atan2(x, 1.0); }
double exp2(double x) { return x87_exp2(x); }
double exp(double x) { return x87_exp2(x * LOG2E); }
double log2(double x) { return x87_log2(x); }
double log(double x) { return x87_log2(x) * LN2; }
double log10(double x) { return x87_log2(x) * 0.30102999566398120; }   /* log10(2) */

double pow(double x, double y) {
    if (y == 0.0)
        return 1.0;
    if (x == 0.0)
        return 0.0;
    if (x < 0.0) {
        double ay = x87_fabs(y);
        long iy = (long) ay;
        if ((double) iy != ay)
            return 0.0 / 0.0; /* pow(negative, non-integer) is NaN */
        double r = x87_exp2(x87_log2(-x) * y);
        return (iy % 2 == 1) ? -r : r;
    }
    return x87_exp2(x87_log2(x) * y);
}
float powf(float x, float y) { return (float) pow((double) x, (double) y); }

double trunc(double x) {
    if (x >= 0.0)
        return (double) (long long) x;
    return -(double) (long long) (-x);
}

double floor(double x) {
    double t = trunc(x);
    if (t > x)
        t -= 1.0;
    return t;
}
float floorf(float x) { return (float) floor((double) x); }

double ceil(double x) {
    double t = trunc(x);
    if (t < x)
        t += 1.0;
    return t;
}
float ceilf(float x) { return (float) ceil((double) x); }

double round(double x) {
    return (x >= 0.0) ? floor(x + 0.5) : ceil(x - 0.5);
}

/* fmod via the real hardware partial-remainder instruction, not a
 * division-then-truncate composition: computing x/y as a rounded double
 * first and truncating that can pick the wrong integer quotient right
 * at any x/y that rounds up to the next integer (e.g. 0.5/0.1, whose
 * true quotient is 4.999999999999999722... but rounds to exactly 5.0
 * as a double) -- fprem avoids this by working from the operands
 * directly, at full internal FPU precision. The loop only ever runs
 * more than once for arguments differing by more than 64 bits of
 * exponent, never the case for anything this kernel's UI/text code
 * computes. */
double fmod(double x, double y) {
    double result = x;
    unsigned short sw;
    do {
        __asm__("fprem\n\tfnstsw %%ax\n\tmov %%ax, %1"
                : "=t"(result), "=m"(sw)
                : "0"(result), "u"(y));
    } while (sw & 0x0400);
    return result;
}

int abs(int x) { return x < 0 ? -x : x; }
