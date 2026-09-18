/*
 * sys_native.c - host (Linux/glibc) implementation of the six primitives
 * cc.c needs.  Used only to build ./cc-native for fast development iteration.
 * The OS build uses prelude.c (self-compiled) or io_os.c instead.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *xalloc(int n) {
    void *p = calloc(1, n > 0 ? (size_t)n : 1);
    if (!p) { fprintf(stderr, "cc: out of memory\n"); exit(1); }
    return p;
}

void *xrealloc(void *p, int n) {
    void *q = realloc(p, n > 0 ? (size_t)n : 1);
    if (!q) { fprintf(stderr, "cc: out of memory\n"); exit(1); }
    return q;
}

char *sys_readfile(char *path, int *plen) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    if ((long)fread(buf, 1, n, f) != n) { fclose(f); return 0; }
    buf[n] = 0;
    fclose(f);
    if (plen) *plen = (int)n;
    return buf;
}

int sys_writefile(char *path, void *buf, int len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int w = (int)fwrite(buf, 1, len, f);
    fclose(f);
    return w == len ? w : -1;
}

void sys_out(char *buf, int len) {
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
}

void sys_exit(int code) { exit(code); }
