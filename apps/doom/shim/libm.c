/*
 * The <math.h> entries that are easier (and safer) in C than in x87 asm.
 * The heavy lifting lives in libm.S.
 */
#include <math.h>

double __pow_pos(double, double);   /* libm.S: x**y for x > 0 */

union du { double d; unsigned int u[2]; };

int __k_isnan(double x) {
    union du v; v.d = x;
    unsigned exp = (v.u[1] >> 20) & 0x7ff;
    unsigned man = (v.u[1] & 0xfffff) | v.u[0];
    return exp == 0x7ff && man != 0;
}

int __k_isinf(double x) {
    union du v; v.d = x;
    unsigned exp = (v.u[1] >> 20) & 0x7ff;
    unsigned man = (v.u[1] & 0xfffff) | v.u[0];
    return exp == 0x7ff && man == 0;
}

double pow(double x, double y) {
    if (y == 0.0 || x == 1.0)
        return 1.0;

    if (x > 0.0)
        return __pow_pos(x, y);

    if (x == 0.0)
        return (y > 0.0) ? 0.0 : HUGE_VAL;

    /* x < 0: real only for integer y. */
    double yt = (y < 0.0) ? -y : y;
    double yint = floor(yt + 0.5);
    if (yint != yt)
        return __builtin_nan("");           /* non-integer exponent */
    double mag = __pow_pos(-x, y);
    /* odd exponent keeps the sign */
    return (fmod(yint, 2.0) == 1.0) ? -mag : mag;
}

double asin(double x) { return atan2(x, sqrt(1.0 - x * x)); }
double acos(double x) { return atan2(sqrt(1.0 - x * x), x); }

double atan(double x) { return atan2(x, 1.0); }
