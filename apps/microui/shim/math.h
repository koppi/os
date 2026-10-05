/**
 * @file apps/microui/shim/math.h
 * @brief <math.h> for the ring-3 microui apps: what lib/libm.c exports and
 *        these programs actually call.
 *
 * lib/libm.c is the x87 libm the userspace side of this tree uses (see
 * lib/Makefile); there is no include/lib/math.h to go with it, so the
 * declarations live here. An app that needs these links lib/libm.o by setting
 * APP_LIB_OBJ in its Makefile -- apps/clock does.
 */
#pragma once

double sin(double x);
double cos(double x);
double sqrt(double x);
double fabs(double x);
double atan2(double y, double x);
