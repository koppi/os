/* Minimal <stdlib.h> for the koppi-os Lua port. */
#ifndef LUA_SHIM_STDLIB_H
#define LUA_SHIM_STDLIB_H

#include <stddef.h>

void  *malloc(size_t);
void  *realloc(void *, size_t);
void   free(void *);

void   abort(void) __attribute__((noreturn));
void   exit(int) __attribute__((noreturn));

int    abs(int);
double strtod(const char *, char **);
long   strtol(const char *, char **, int);
char  *getenv(const char *);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 0x7fffffff

#endif
