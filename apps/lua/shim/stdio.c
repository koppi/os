/*
 * <stdio.h> shim for the koppi-os Lua port.
 *
 * Kinds of stream:
 *   FK_OUT  - the kernel console (stdout / stderr), write syscall
 *   FK_IN   - the console keyboard (stdin), gets syscall, line buffered
 *   FK_VFS  - a file on a mounted FAT volume, fopen/fread/fclose syscalls
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ksys.h"

size_t _write(const void *buf, size_t len) {
    return (size_t) ksys2(SYS_WRITE, (unsigned long) buf, (unsigned long) len);
}

static void sys_gets(char *buf, unsigned size) {
    ksys2(SYS_GETS, (unsigned long) buf, (unsigned long) size);
}

/* Kernel `file` struct (vfs.h) - only len/eof matter here. */
typedef struct {
    char     name[32];
    unsigned flags, len, eof, dev, current_cluster, type;
} kfile_t;

enum { FK_CLOSED = 0, FK_OUT, FK_IN, FK_VFS };

struct __shim_file {
    int       kind;
    kfile_t  *kf;              /* FK_VFS: kernel handle from fopen syscall */
    unsigned  off;             /* FK_VFS: bytes consumed from the file */
    unsigned char buf[512];
    int       bpos, blen;      /* buffer cursor / fill */
    int       ungot;           /* pushed-back byte, or -1 */
    int       eof, err;
};

static FILE f_stdin  = { FK_IN,  0, 0, {0}, 0, 0, -1, 0, 0 };
static FILE f_stdout = { FK_OUT, 0, 0, {0}, 0, 0, -1, 0, 0 };
static FILE f_stderr = { FK_OUT, 0, 0, {0}, 0, 0, -1, 0, 0 };

FILE *stdin  = &f_stdin;
FILE *stdout = &f_stdout;
FILE *stderr = &f_stderr;

static FILE pool[FOPEN_MAX];

/* ---------------- output ---------------- */

void __lua_out(const void *b, size_t n) { _write(b, n); }

void __lua_errf(const char *fmt, ...) {
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int) sizeof tmp) n = sizeof tmp;
    _write(tmp, (size_t) n);
}

void putchar_(char c) { _write(&c, 1); }        /* referenced by printf.c */

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *s) {
    size_t n = size * nmemb;
    if (s && (s->kind == FK_OUT) && n)
        _write(ptr, n);
    return nmemb;
}

int fputc(int c, FILE *s) {
    unsigned char ch = (unsigned char) c;
    if (s && s->kind == FK_OUT) _write(&ch, 1);
    return c;
}

int fputs(const char *str, FILE *s) {
    if (s && s->kind == FK_OUT) _write(str, strlen(str));
    return 0;
}

int vfprintf(FILE *s, const char *fmt, va_list ap) {
    char tmp[512];
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    if (n < 0) return n;
    size_t w = (n > (int) sizeof tmp) ? sizeof tmp : (size_t) n;
    if (s && s->kind == FK_OUT) _write(tmp, w);
    return n;
}

int fprintf(FILE *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(s, fmt, ap);
    va_end(ap);
    return n;
}

int fflush(FILE *s) { (void) s; return 0; }

/* ---------------- input ---------------- */

/* Read one console line into s->buf (stdin). The kernel gets() strips the
 * newline and returns a lone \x04 for Ctrl-D; re-add the '\n' so callers see a
 * terminated line, and map Ctrl-D to EOF. Returns bytes available, 0 at EOF. */
static int refill_line(FILE *s) {
    s->bpos = s->blen = 0;
    sys_gets((char *) s->buf, sizeof s->buf - 1);
    if (s->buf[0] == 4) { s->eof = 1; return 0; }   /* Ctrl-D */
    int n = (int) strlen((char *) s->buf);
    s->buf[n] = '\n';
    s->blen = n + 1;
    return s->blen;
}

/* Read the next 512-byte block of a VFS file (fread syscall wants the kernel
 * handle in ebx and the buffer in ecx). Returns valid bytes, 0 at EOF. */
static int vfs_block(FILE *s) {
    ksys2(SYS_FREAD, (unsigned long) s->kf, (unsigned long) s->buf);
    unsigned remain = s->kf->len - s->off;
    int got = remain < 512 ? (int) remain : 512;
    s->off += (unsigned) got;
    s->blen = got;
    s->bpos = 0;
    return got;
}

int fgetc(FILE *s) {
    if (!s) return EOF;
    if (s->ungot >= 0) { int c = s->ungot; s->ungot = -1; return c; }
    if (s->bpos >= s->blen) {
        if (s->kind == FK_VFS) {
            if (s->off >= (s->kf ? s->kf->len : 0)) { s->eof = 1; return EOF; }
            if (vfs_block(s) <= 0) { s->eof = 1; return EOF; }
        } else if (refill_line(s) <= 0) {
            return EOF;
        }
    }
    return s->buf[s->bpos++];
}

int getc(FILE *s) { return fgetc(s); }

int ungetc(int c, FILE *s) {
    if (!s || c == EOF) return EOF;
    s->ungot = (unsigned char) c;
    s->eof = 0;
    return c;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *s) {
    size_t want = size * nmemb, done = 0;
    unsigned char *out = ptr;
    while (done < want) {
        int c = fgetc(s);
        if (c == EOF) break;
        out[done++] = (unsigned char) c;
    }
    return size ? done / size : 0;
}

char *fgets(char *dst, int size, FILE *s) {
    if (size <= 0) return 0;
    int i = 0;
    while (i < size - 1) {
        int c = fgetc(s);
        if (c == EOF) break;
        dst[i++] = (char) c;
        if (c == '\n') break;
    }
    if (i == 0) return 0;
    dst[i] = 0;
    return dst;
}

int feof(FILE *s)   { return s ? s->eof : 1; }
int ferror(FILE *s) { return s ? s->err : 0; }
void clearerr(FILE *s) { if (s) { s->eof = 0; s->err = 0; } }

/* ---------------- open / close ---------------- */

FILE *fopen(const char *path, const char *mode) {
    (void) mode;                     /* read-only for now */
    char norm[128];
    if (path[0] != '/') {
        norm[0] = '/';
        strncpy(norm + 1, path, sizeof norm - 2);
        norm[sizeof norm - 1] = 0;
        path = norm;
    }

    kfile_t *kf = (kfile_t *) ksys2(SYS_FOPEN, (unsigned long) path,
                                    (unsigned long) "r");
    if (!kf || kf->type != 0 /* FS_FILE */)
        return 0;

    for (int i = 0; i < FOPEN_MAX; i++) {
        if (pool[i].kind == FK_CLOSED) {
            FILE *s = &pool[i];
            memset(s, 0, sizeof *s);
            s->kind = FK_VFS;
            s->kf = kf;
            s->ungot = -1;
            return s;
        }
    }
    return 0;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    if (!path && stream == stdin) return stdin;       /* the case Lua uses */
    if (stream && stream->kind == FK_VFS) fclose(stream);
    return path ? fopen(path, mode) : 0;
}

int fclose(FILE *s) {
    if (!s) return EOF;
    if (s->kind == FK_VFS && s->kf)
        ksys1(SYS_FCLOSE, (unsigned long) s->kf);
    s->kind = FK_CLOSED;
    s->kf = 0;
    return 0;
}

int fseek(FILE *s, long off, int whence) { (void) s; (void) off; (void) whence; return -1; }
long ftell(FILE *s) { return s && s->kind == FK_VFS ? (long) s->off : -1; }
int setvbuf(FILE *s, char *b, int m, size_t n) { (void) s; (void) b; (void) m; (void) n; return 0; }

int remove(const char *p) { (void) p; return -1; }
int rename(const char *a, const char *b) { (void) a; (void) b; return -1; }
FILE *tmpfile(void) { return 0; }
char *tmpnam(char *s) { (void) s; return 0; }
