/* <stdlib.h> for the koppi-os Doom port. */
#ifndef DOOM_SHIM_STDLIB_H
#define DOOM_SHIM_STDLIB_H

#include <stddef.h>

void  *malloc(size_t);
void  *calloc(size_t, size_t);
void  *realloc(void *, size_t);
void   free(void *);

void   abort(void) __attribute__((noreturn));
void   exit(int) __attribute__((noreturn));

/*
 * exit() runs these before it leaves. The engine has its own I_AtExit list,
 * but that one only runs on the paths that reach I_Quit or I_Error, and the
 * full-screen grab has to be undone however the game ends -- so the port
 * registers the teardown here as well (see doomgeneric_koppi.c).
 */
int    atexit(void (*)(void));

int    abs(int);
int    atoi(const char *);
double atof(const char *);
long   strtol(const char *, char **, int);
double strtod(const char *, char **);
char  *getenv(const char *);
int    system(const char *);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif
