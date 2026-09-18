/*
 * Minimal <math.h> for the koppi-os Lua port - just what Lua 5.4's core and
 * math library reference. Implemented with the x87 FPU in libm.c.
 */
#ifndef LUA_SHIM_MATH_H
#define LUA_SHIM_MATH_H

double floor(double);
double ceil(double);
double fabs(double);
double sqrt(double);
double fmod(double, double);
double pow(double, double);
double exp(double);
double log(double);
double log10(double);
double sin(double);
double cos(double);
double tan(double);
double asin(double);
double acos(double);
double atan2(double, double);
double frexp(double, int *);
double ldexp(double, int);
double trunc(double);

int __lua_isnan(double);
int __lua_isinf(double);

/* HUGE_VAL: +infinity as a compile-time constant. */
#define HUGE_VAL (__builtin_huge_val())
#define isnan(x) __lua_isnan(x)
#define isinf(x) __lua_isinf(x)

#endif
