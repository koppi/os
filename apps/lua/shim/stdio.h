/*
 * Minimal <stdio.h> for the koppi-os Lua port.
 *
 * stdout / stderr write to the kernel console (write syscall); stdin reads a
 * line at a time (gets syscall). Regular files are backed by the VFS through
 * the fopen / fread / fclose syscalls, 512 bytes at a time.
 */
#ifndef LUA_SHIM_STDIO_H
#define LUA_SHIM_STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include "printf.h"          /* snprintf / vsnprintf / sprintf / vsprintf */

typedef struct __shim_file FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF (-1)
#define BUFSIZ 512
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IONBF 2
#define _IOLBF 1
#define _IOFBF 0
#define L_tmpnam 32
#define FOPEN_MAX 16

FILE *fopen(const char *path, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int   fclose(FILE *);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *);
int   fgetc(FILE *);
int   getc(FILE *);
int   ungetc(int c, FILE *);
char *fgets(char *s, int size, FILE *);
int   fputc(int c, FILE *);
int   fputs(const char *s, FILE *);
int   fflush(FILE *);
int   feof(FILE *);
int   ferror(FILE *);
void  clearerr(FILE *);
int   fseek(FILE *, long off, int whence);
long  ftell(FILE *);
int   setvbuf(FILE *, char *buf, int mode, size_t size);
int   fprintf(FILE *, const char *fmt, ...);
int   vfprintf(FILE *, const char *fmt, va_list ap);
int   remove(const char *path);
int   rename(const char *a, const char *b);
FILE *tmpfile(void);
char *tmpnam(char *s);

/* Fast paths used by the luaconf output macros. */
void  __lua_out(const void *buf, size_t len);
void  __lua_errf(const char *fmt, ...);

#endif
