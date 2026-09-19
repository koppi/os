/* Minimal <errno.h> for the koppi-os Doom port. Nothing here ever sets errno
 * -- the shim reports failure through return values -- but m_misc.c reads it
 * to tell "not a file" from "no such file", and gets 0 (neither). */
#ifndef DOOM_SHIM_ERRNO_H
#define DOOM_SHIM_ERRNO_H

extern int errno;

#define EDOM    1
#define ERANGE  2
#define EINVAL  3
#define ENOENT  4
#define EISDIR  5
#define EACCES  6
#define EEXIST  7

#endif
