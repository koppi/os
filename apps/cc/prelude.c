/*
 * prelude.c - the runtime library cc prepends to every program it compiles
 * (unless -nostdlib).  Written in cc's own accepted subset.  Everything here
 * talks to the kernel through the __syscallN(...) builtins, which cc lowers to
 *   int $0x72   with  eax = call number, ebx/ecx/edx = arguments.
 *
 * Syscalls used:  5 exit(code)   6 fopen(name,mode)   7 fclose(f)
 *                 9 malloc(n)   10 free(p)   11 realloc(p,n)
 *                12 write(buf,len)  13 fread(f,buf512)  16 spit(path,buf,len)
 */

typedef struct {
    char name[32];
    unsigned flags;
    unsigned len;
    unsigned eof;
    unsigned dev;
    unsigned cur;
    unsigned type;
} __CFILE;

/* ---- process ---- */
void exit(int code) { __syscall1(5, code); }

/* ---- memory ---- */
void *malloc(int n)              { return (void *)__syscall1(9, n); }
void  free(void *p)              { __syscall1(10, (int)p); }
void *realloc(void *p, int n)    { return (void *)__syscall2(11, (int)p, n); }

void *memset(void *d, int c, int n) {
    char *p = d;
    int i = 0;
    while (i < n) { p[i] = (char)c; i++; }
    return d;
}
void *memcpy(void *d, void *s, int n) {
    char *a = d; char *b = s;
    int i = 0;
    while (i < n) { a[i] = b[i]; i++; }
    return d;
}
void *memmove(void *d, void *s, int n) {
    char *a = d; char *b = s;
    if (a < b) { int i = 0; while (i < n) { a[i] = b[i]; i++; } }
    else { int i = n; while (i > 0) { i--; a[i] = b[i]; } }
    return d;
}
int memcmp(void *a, void *b, int n) {
    unsigned char *x = a; unsigned char *y = b;
    int i = 0;
    while (i < n) { if (x[i] != y[i]) return x[i] - y[i]; i++; }
    return 0;
}

/* ---- strings ---- */
int strlen(char *s) { int n = 0; while (s[n]) n++; return n; }
int strcmp(char *a, char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
int strncmp(char *a, char *b, int n) {
    int i = 0;
    while (i < n) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
        i++;
    }
    return 0;
}
char *strcpy(char *d, char *s) { int i = 0; while (s[i]) { d[i] = s[i]; i++; } d[i] = 0; return d; }
char *strncpy(char *d, char *s, int n) {
    int i = 0;
    while (i < n && s[i]) { d[i] = s[i]; i++; }
    while (i < n) { d[i] = 0; i++; }
    return d;
}
char *strcat(char *d, char *s) { strcpy(d + strlen(d), s); return d; }
char *strchr(char *s, int c) {
    while (*s) { if (*s == (char)c) return s; s++; }
    return (c == 0) ? s : 0;
}
char *strrchr(char *s, int c) {
    char *last = 0;
    while (*s) { if (*s == (char)c) last = s; s++; }
    return last;
}
char *strstr(char *h, char *n) {
    int ln = strlen(n);
    while (*h) { if (strncmp(h, n, ln) == 0) return h; h++; }
    return 0;
}
int atoi(char *s) {
    int n = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return neg ? -n : n;
}
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalnum(int c) { return isdigit(c) || isalpha(c); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* ---- raw console output ---- */
int _write(void *buf, int len) { __syscall2(12, (int)buf, len); return len; }

int putchar(int c) { char b; b = (char)c; __syscall2(12, (int)&b, 1); return c; }
int puts(char *s) { _write(s, strlen(s)); putchar('\n'); return 0; }

/* ---- printf family (varargs via the i386 stack) ---- */
int __fmt(char *out, char *fmt, int *ap) {
    int n = 0;
    while (*fmt) {
        if (*fmt != '%') { out[n++] = *fmt++; continue; }
        fmt++;
        int width = 0, zero = 0;
        if (*fmt == '0') { zero = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        if (*fmt == 'l') fmt++;
        char c = *fmt++;
        if (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X' || c == 'p') {
            unsigned v = (unsigned)*ap++;
            int base = (c == 'x' || c == 'X' || c == 'p') ? 16 : 10;
            int neg = 0;
            char tmp[16]; int t = 0;
            if ((c == 'd' || c == 'i') && (int)v < 0) { neg = 1; v = (unsigned)(-(int)v); }
            if (c == 'p') { out[n++] = '0'; out[n++] = 'x'; }
            if (v == 0) tmp[t++] = '0';
            while (v) {
                int d = v % base;
                tmp[t++] = (d < 10) ? ('0' + d) : ((c == 'X' ? 'A' : 'a') + d - 10);
                v = v / base;
            }
            int len = t + (neg ? 1 : 0);
            while (len < width) { out[n++] = zero ? '0' : ' '; len++; }
            if (neg) out[n++] = '-';
            while (t) out[n++] = tmp[--t];
        } else if (c == 's') {
            char *s = (char *)*ap++;
            if (!s) s = "(null)";
            int len = strlen(s);
            while (len < width) { out[n++] = ' '; width--; }
            while (*s) out[n++] = *s++;
        } else if (c == 'c') {
            out[n++] = (char)*ap++;
        } else if (c == '%') {
            out[n++] = '%';
        } else {
            out[n++] = '%';
            if (c) out[n++] = c;
        }
    }
    out[n] = 0;
    return n;
}

int printf(char *fmt, ...) {
    int *ap = ((int *)&fmt) + 1;
    char buf[2048];
    int n = __fmt(buf, fmt, ap);
    _write(buf, n);
    return n;
}
int sprintf(char *out, char *fmt, ...) {
    int *ap = ((int *)&fmt) + 1;
    return __fmt(out, fmt, ap);
}
int snprintf(char *out, int sz, char *fmt, ...) {
    int *ap = ((int *)&fmt) + 1;
    (void)sz;
    return __fmt(out, fmt, ap);
}
void fprintf(void *stream, char *fmt, ...) {
    int *ap = ((int *)&fmt) + 1;
    char buf[2048];
    int n = __fmt(buf, fmt, ap);
    (void)stream;
    _write(buf, n);
}

/* ---- file IO ---- */
/* the fopen syscall wants a leading-slash device path ("/rd/x"); accept both */
void *fopen(char *name, char *mode) {
    if (name[0] == '/')
        return (void *)__syscall2(6, (int)name, (int)mode);
    char p[128];
    int i = 0;
    p[0] = '/';
    while (name[i] && i < 126) { p[i + 1] = name[i]; i++; }
    p[i + 1] = 0;
    return (void *)__syscall2(6, (int)p, (int)mode);
}
void  fclose(void *f) { __syscall1(7, (int)f); }
int   fgetc(void *f) { (void)f; return -1; }

/* read the whole file at 'path' into a fresh buffer; *plen gets its length. */
char *sys_readfile(char *path, int *plen) {
    __CFILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = f->len;
    /* +512 slack: the fread syscall always writes a full 512-byte block, so the
       last (partial) block over-reads into this padding rather than off the end.
       Reading straight into the destination avoids a large stack scratch buffer,
       which on this OS can sit close under the kernel stack. */
    char *buf = malloc(n + 512 + 1);
    int got = 0;
    while (f->eof == 0 && got < n) {
        __syscall2(13, (int)f, (int)(buf + got));
        got += 512;
    }
    buf[n] = 0;
    fclose(f);
    if (plen) *plen = n;
    return buf;
}

int sys_writefile(char *path, void *buf, int len) {
    return (int)__syscall3(16, (int)path, (int)buf, len);
}
void sys_out(char *buf, int len) { __syscall2(12, (int)buf, len); }
void sys_exit(int code) { __syscall1(5, code); }

void *xalloc(int n) {
    if (n <= 0) n = 1;
    char *p = malloc(n);
    int i = 0;
    while (i < n) { p[i] = 0; i++; }
    return p;
}
void *xrealloc(void *p, int n) { return realloc(p, n <= 0 ? 1 : n); }
