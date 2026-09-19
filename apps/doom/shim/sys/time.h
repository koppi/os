/* <sys/time.h> for the koppi-os Doom port; i_timer.c has its include
 * commented out and goes through DG_GetTicksMs instead. */
#ifndef DOOM_SHIM_SYS_TIME_H
#define DOOM_SHIM_SYS_TIME_H

struct timeval  { long tv_sec; long tv_usec; };
struct timezone { int tz_minuteswest; int tz_dsttime; };

#endif
