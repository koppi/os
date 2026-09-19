/*
 * <math.h> for the koppi-os Doom port. The engine's fixed-point maths needs
 * almost none of this -- its trig comes from the precomputed tables in
 * tables.c -- so this is the x87 subset already written for the Lua port
 * (libm.S / libm.c), plus atan().
 */
#ifndef DOOM_SHIM_MATH_H
#define DOOM_SHIM_MATH_H

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
double atan(double);
double atan2(double, double);
double frexp(double, int *);
double ldexp(double, int);
double trunc(double);

int __k_isnan(double);
int __k_isinf(double);

#define HUGE_VAL (__builtin_huge_val())
#define M_PI 3.14159265358979323846
#define isnan(x) __k_isnan(x)
#define isinf(x) __k_isinf(x)

#endif
