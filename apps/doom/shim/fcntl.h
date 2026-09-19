/* <fcntl.h> for the koppi-os Doom port: included by two files that only use
 * open() inside #ifdef ORIGCODE, so the flags are all that has to exist. */
#ifndef DOOM_SHIM_FCNTL_H
#define DOOM_SHIM_FCNTL_H

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0100
#define O_TRUNC  01000
#define O_APPEND 02000

#endif
