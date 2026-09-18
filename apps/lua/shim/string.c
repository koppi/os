/* Standard <string.h> implementations for the koppi-os Lua port. */
#include <string.h>

void *memcpy(void *d, const void *s, size_t n) {
    unsigned char *dp = d;
    const unsigned char *sp = s;
    while (n--) *dp++ = *sp++;
    return d;
}

void *memmove(void *d, const void *s, size_t n) {
    unsigned char *dp = d;
    const unsigned char *sp = s;
    if (dp < sp) {
        while (n--) *dp++ = *sp++;
    } else {
        dp += n; sp += n;
        while (n--) *--dp = *--sp;
    }
    return d;
}

void *memset(void *d, int c, size_t n) {
    unsigned char *dp = d;
    while (n--) *dp++ = (unsigned char) c;
    return d;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = a, *pb = b;
    while (n--) {
        if (*pa != *pb) return (int) *pa - (int) *pb;
        pa++; pb++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    while (n--) {
        if (*p == (unsigned char) c) return (void *) p;
        p++;
    }
    return 0;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t) (p - s);
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int) (unsigned char) *a - (int) (unsigned char) *b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (int) (unsigned char) *a - (int) (unsigned char) *b;
}

int strcoll(const char *a, const char *b) { return strcmp(a, b); }

char *strcpy(char *d, const char *s) {
    char *r = d;
    while ((*d++ = *s++)) ;
    return r;
}

char *strncpy(char *d, const char *s, size_t n) {
    char *r = d;
    while (n && (*d = *s)) { d++; s++; n--; }
    while (n--) *d++ = 0;
    return r;
}

char *strcat(char *d, const char *s) {
    char *r = d;
    while (*d) d++;
    while ((*d++ = *s++)) ;
    return r;
}

char *strchr(const char *s, int c) {
    for (;; s++) {
        if (*s == (char) c) return (char *) s;
        if (!*s) return 0;
    }
}

char *strrchr(const char *s, int c) {
    const char *last = 0;
    for (;; s++) {
        if (*s == (char) c) last = s;
        if (!*s) return (char *) last;
    }
}

size_t strspn(const char *s, const char *set) {
    const char *p = s;
    for (; *p; p++) {
        const char *q = set;
        while (*q && *q != *p) q++;
        if (!*q) break;
    }
    return (size_t) (p - s);
}

size_t strcspn(const char *s, const char *set) {
    const char *p = s;
    for (; *p; p++) {
        const char *q = set;
        while (*q && *q != *p) q++;
        if (*q) break;
    }
    return (size_t) (p - s);
}

char *strpbrk(const char *s, const char *set) {
    for (; *s; s++) {
        const char *q = set;
        while (*q) {
            if (*q == *s) return (char *) s;
            q++;
        }
    }
    return 0;
}

char *strstr(const char *hay, const char *needle) {
    if (!*needle) return (char *) hay;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *) hay;
    }
    return 0;
}

char *strerror(int e) {
    (void) e;
    return (char *) "error";
}
