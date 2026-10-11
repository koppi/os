/*
 * sched.h -- the one call real Qt6 source (qthread_unix.cpp's
 * QThread::yieldCurrentThread()) needs from it. sched_yield() just hands
 * off to this kernel's own thread_yield() syscall (system_calls.h).
 */
#ifndef LIB_SCHED_H
#define LIB_SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

int sched_yield(void);

#ifdef __cplusplus
}
#endif

#endif
