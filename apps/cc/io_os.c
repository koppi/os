/*
 * io_os.c - the six primitives cc.c needs, for the ld-linked bootstrap `cc`
 * (built with the host gcc + the OS shim libc in ../../lib).  The self-hosted
 * build gets these from prelude.c instead.
 */
#include <lib/system_calls.h>

typedef struct {
    char name[32];
    unsigned flags;
    unsigned len;
    unsigned eof;
    unsigned dev;
    unsigned cur;
    unsigned type;
} OSFILE;

void *xalloc(int n) {
    if (n <= 0) n = 1;
    char *p = (char *)syscall3(9, (unsigned)n, 0, 0);
    for (int i = 0; i < n; i++) p[i] = 0;
    return p;
}

void *xrealloc(void *p, int n) {
    return (void *)syscall3(11, (unsigned)p, (unsigned)(n <= 0 ? 1 : n), 0);
}

void sys_out(char *buf, int len) { (void)syscall3(12, (unsigned)buf, (unsigned)len, 0); }

void sys_exit(int code) { (void)syscall3(5, (unsigned)code, 0, 0); }

int sys_writefile(char *path, void *buf, int len) {
    return write_file(path, buf, (unsigned)len);
}

char *sys_readfile(char *path, int *plen) {
    char abs[128];
    if (path[0] != '/') {
        abs[0] = '/';
        int i = 0;
        while (path[i] && i < 126) { abs[i + 1] = path[i]; i++; }
        abs[i + 1] = 0;
        path = abs;
    }
    OSFILE *f = (OSFILE *)syscall3(6, (unsigned)path, (unsigned)"r", 0);
    if (!f) return 0;
    int n = f->len;
    char *out = (char *)syscall3(9, (unsigned)(n + 512 + 1), 0, 0);
    int got = 0;
    while (f->eof == 0 && got < n) {
        (void)syscall3(13, (unsigned)f, (unsigned)(out + got), 0);
        got += 512;
    }
    out[n] = 0;
    (void)syscall3(7, (unsigned)f, 0, 0);
    if (plen) *plen = n;
    return out;
}
