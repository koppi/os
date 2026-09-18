/* Minimal <errno.h> for the koppi-os Lua port. */
#ifndef LUA_SHIM_ERRNO_H
#define LUA_SHIM_ERRNO_H

extern int errno;

#define EDOM   1
#define ERANGE 2
#define EINVAL 3

#endif
