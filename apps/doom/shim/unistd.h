/* <unistd.h> for the koppi-os Doom port. Nothing in the live code paths uses
 * the POSIX process or descriptor calls; usleep is the one real entry. */
#ifndef DOOM_SHIM_UNISTD_H
#define DOOM_SHIM_UNISTD_H

#include <stddef.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int usleep(unsigned usec);
int isatty(int fd);
int fileno(void *stream);

#endif
