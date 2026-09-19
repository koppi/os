/*
 * <stdio.h> for the koppi-os Doom port.
 *
 * Three kinds of stream, all backed by whole-file syscalls rather than a file
 * descriptor with a cursor (the kernel VFS has no seek):
 *
 *   console  - stdout / stderr via the write syscall, stdin via gets
 *   read     - the file is slurped into memory by fopen(), so fseek/ftell/
 *              fread are ordinary pointer arithmetic
 *   write    - output accumulates in a growable buffer and is handed to the
 *              spit syscall in one piece by fclose()
 *
 * Nothing a write stream produces reaches the disk before fclose(), so a
 * program that is killed mid-save loses the file rather than truncating it.
 */
#ifndef DOOM_SHIM_STDIO_H
#define DOOM_SHIM_STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include "printf.h"          /* printf / snprintf / vsnprintf / sprintf */

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

FILE  *fopen(const char *path, const char *mode);
FILE  *freopen(const char *path, const char *mode, FILE *stream);
int    fclose(FILE *);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *);
int    fgetc(FILE *);
int    getc(FILE *);
int    ungetc(int c, FILE *);
char  *fgets(char *s, int size, FILE *);
int    fputc(int c, FILE *);
int    fputs(const char *s, FILE *);
int    putchar(int c);
int    puts(const char *s);
int    fflush(FILE *);
int    feof(FILE *);
int    ferror(FILE *);
void   clearerr(FILE *);
int    fseek(FILE *, long off, int whence);
long   ftell(FILE *);
void   rewind(FILE *);
int    setvbuf(FILE *, char *buf, int mode, size_t size);
int    fprintf(FILE *, const char *fmt, ...);
int    vfprintf(FILE *, const char *fmt, va_list ap);
int    sscanf(const char *str, const char *fmt, ...);
int    remove(const char *path);
int    rename(const char *a, const char *b);

/*
 * Hand a read stream's buffer to the caller and forget about it: how
 * w_file_koppi.c turns an fopen()ed WAD into the memory-mapped image w_wad.c
 * reads lumps straight out of, with no second copy of a 4 MiB file.
 */
void  *__koppi_steal(FILE *, size_t *len);

#endif
