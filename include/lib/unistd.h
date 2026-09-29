/*
 *  Copyright 2016 Davide Pianca
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef UNISTD_H
#define UNISTD_H

#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Unimplemented in the kernel: always returns -1. */
pid_t fork();
void exit(int code);
pid_t wait(int *x);
//pid_t wait(pid_t proc, int *x, int code);
pid_t getpid();
pid_t getppid();

/* sysconf() -- only the one query real Qt6 source (qthread_unix.cpp's
 * QThread::idealThreadCount(), on the "rest: Solaris, AIX, Tru64" fallback
 * path real Qt takes for any OS without its own #ifdef branch) needs. No
 * syscall exposes this kernel's real booted-CPU count to userspace yet, so
 * this reports 1 -- the same conservative fallback real Qt itself uses on
 * platforms without a CPU-count facility (see qthread_unix.cpp's Integrity
 * branch). */
#define _SC_NPROCESSORS_ONLN 1
long sysconf(int name);

#ifdef __cplusplus
}
#endif

#endif

