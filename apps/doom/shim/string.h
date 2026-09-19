/* Standard-signature <string.h> for the koppi-os Doom port. */
#ifndef DOOM_SHIM_STRING_H
#define DOOM_SHIM_STRING_H

#include <stddef.h>

void  *memcpy(void *, const void *, size_t);
void  *memmove(void *, const void *, size_t);
void  *memset(void *, int, size_t);
int    memcmp(const void *, const void *, size_t);
void  *memchr(const void *, int, size_t);

size_t strlen(const char *);
int    strcmp(const char *, const char *);
int    strncmp(const char *, const char *, size_t);
int    strcoll(const char *, const char *);
char  *strcpy(char *, const char *);
char  *strncpy(char *, const char *, size_t);
char  *strcat(char *, const char *);
char  *strchr(const char *, int);
char  *strrchr(const char *, int);
char  *strpbrk(const char *, const char *);
size_t strspn(const char *, const char *);
size_t strcspn(const char *, const char *);
char  *strstr(const char *, const char *);
char  *strdup(const char *);
char  *strerror(int);

/* Also declared by <strings.h>, which is where doomtype.h looks for them. */
int    strcasecmp(const char *, const char *);
int    strncasecmp(const char *, const char *, size_t);

#endif
