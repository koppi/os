/*
 * <stdio.h> implementation for the koppi-os Doom port. See stdio.h for the
 * three stream kinds and why files are read and written whole.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ksys.h"

size_t _write(const void *buf, size_t len) {
    return (size_t) ksys2(SYS_WRITE, (unsigned long) buf, (unsigned long) len);
}

static void sys_gets(char *buf, unsigned size) {
    ksys2(SYS_GETS, (unsigned long) buf, (unsigned long) size);
}

/* The kernel's `file` handle (vfs.h); only len and type are read here. */
typedef struct {
    char     name[32];
    unsigned flags, len, eof, dev, current_cluster, type;
} kfile_t;

#define KFS_FILE 0

enum { FK_CLOSED = 0, FK_OUT, FK_IN, FK_RD, FK_WR };

struct __shim_file {
    int      kind;
    char    *buf;      /* FK_RD: the whole file. FK_WR: what has been written. */
    size_t   len;      /* bytes of content */
    size_t   cap;      /* FK_WR: allocated size of buf */
    size_t   pos;      /* cursor */
    int      ungot;    /* pushed-back byte, or -1 */
    int      eof, err;
    char     path[96]; /* FK_WR: where fclose() spits the buffer */
};

static FILE f_stdin  = { FK_IN,  0, 0, 0, 0, -1, 0, 0, {0} };
static FILE f_stdout = { FK_OUT, 0, 0, 0, 0, -1, 0, 0, {0} };
static FILE f_stderr = { FK_OUT, 0, 0, 0, 0, -1, 0, 0, {0} };

FILE *stdin  = &f_stdin;
FILE *stdout = &f_stdout;
FILE *stderr = &f_stderr;

static FILE pool[FOPEN_MAX];

/* stdin is read a console line at a time into this. */
static char line_buf[256];
static int  line_pos, line_len;

/* ---------------- output ---------------- */

void putchar_(char c) { _write(&c, 1); }        /* referenced by printf.c */

int putchar(int c) { char ch = (char) c; _write(&ch, 1); return c; }

int puts(const char *s) {
    _write(s, strlen(s));
    _write("\n", 1);
    return 0;
}

/** @brief Make room for @p extra more bytes in a write stream. */
static int wr_reserve(FILE *s, size_t extra) {
    if (s->len + extra <= s->cap)
        return 1;
    size_t want = s->cap ? s->cap : 4096;
    while (want < s->len + extra)
        want *= 2;
    char *nb = realloc(s->buf, want);
    if (!nb) { s->err = 1; return 0; }
    s->buf = nb;
    s->cap = want;
    return 1;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *s) {
    size_t n = size * nmemb;
    if (!s || !n)
        return 0;
    if (s->kind == FK_OUT) {
        _write(ptr, n);
        return nmemb;
    }
    if (s->kind != FK_WR || !wr_reserve(s, n))
        return 0;
    memcpy(s->buf + s->len, ptr, n);
    s->len += n;
    s->pos = s->len;
    return size ? n / size : 0;
}

int fputc(int c, FILE *s) {
    unsigned char ch = (unsigned char) c;
    if (!s) return EOF;
    if (s->kind == FK_OUT) { _write(&ch, 1); return c; }
    return fwrite(&ch, 1, 1, s) == 1 ? c : EOF;
}

int fputs(const char *str, FILE *s) {
    size_t n = strlen(str);
    if (!s) return EOF;
    if (s->kind == FK_OUT) { _write(str, n); return 0; }
    return fwrite(str, 1, n, s) == n ? 0 : EOF;
}

int vfprintf(FILE *s, const char *fmt, va_list ap) {
    char tmp[512];
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    if (n < 0) return n;
    size_t w = (n > (int) sizeof tmp) ? sizeof tmp : (size_t) n;
    if (s && s->kind == FK_OUT)
        _write(tmp, w);
    else if (s)
        fwrite(tmp, 1, w, s);
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

/* ---------------- paths ---------------- */

/*
 * Make @p in absolute. The kernel VFS has no per-process working directory --
 * the one `cd` maintains belongs to the console -- so a relative path is
 * resolved against that, and against the boot RAM disk when the console is at
 * the root. Doom builds most of its paths from an IWAD directory and so
 * arrives here already absolute.
 */
static const char *abspath(const char *in, char *out, size_t n) {
    if (in[0] == '/')
        return in;

    char cwd[80];
    unsigned got = (unsigned) ksys2(19 /* getcwd */, (unsigned long) cwd,
                                    (unsigned long) sizeof cwd);
    const char *base = (got > 1 && cwd[0] == '/') ? cwd : "/rd";

    size_t i = 0;
    while (base[i] && i + 2 < n) { out[i] = base[i]; i++; }
    if (i && out[i - 1] == '/') i--;          /* no "//" in the middle */
    out[i++] = '/';
    size_t j = 0;
    while (in[j] && i + 1 < n) out[i++] = in[j++];
    out[i] = 0;
    return out;
}

/* ---------------- input ---------------- */

/* Read one console line (stdin). The kernel gets() strips the newline and
 * returns a lone \x04 for Ctrl-D; re-add the '\n' and map Ctrl-D to EOF. */
static int refill_line(FILE *s) {
    line_pos = line_len = 0;
    sys_gets(line_buf, sizeof line_buf - 1);
    if (line_buf[0] == 4) { s->eof = 1; return 0; }
    int n = (int) strlen(line_buf);
    line_buf[n] = '\n';
    line_len = n + 1;
    return line_len;
}

int fgetc(FILE *s) {
    if (!s) return EOF;
    if (s->ungot >= 0) { int c = s->ungot; s->ungot = -1; return c; }
    if (s->kind == FK_IN) {
        if (line_pos >= line_len && refill_line(s) <= 0)
            return EOF;
        return (unsigned char) line_buf[line_pos++];
    }
    if (s->kind != FK_RD || s->pos >= s->len) {
        if (s) s->eof = 1;
        return EOF;
    }
    return (unsigned char) s->buf[s->pos++];
}

int getc(FILE *s) { return fgetc(s); }

int ungetc(int c, FILE *s) {
    if (!s || c == EOF) return EOF;
    s->ungot = (unsigned char) c;
    s->eof = 0;
    return c;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *s) {
    size_t want = size * nmemb;
    if (!s || !want)
        return 0;

    if (s->kind == FK_RD && s->ungot < 0) {     /* the common case: bulk copy */
        size_t avail = s->len - s->pos;
        if (want > avail) { want = avail; s->eof = 1; }
        memcpy(ptr, s->buf + s->pos, want);
        s->pos += want;
        return size ? want / size : 0;
    }

    unsigned char *out = ptr;
    size_t done = 0;
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

static FILE *pool_get(void) {
    for (int i = 0; i < FOPEN_MAX; i++) {
        if (pool[i].kind == FK_CLOSED) {
            FILE *s = &pool[i];
            memset(s, 0, sizeof *s);
            s->ungot = -1;
            return s;
        }
    }
    return 0;
}

/* Read the whole file at @p path. Returns the buffer (NUL-terminated for the
 * benefit of text callers) and writes its length to @p out_len, or NULL. */
static char *slurp(const char *path, size_t *out_len) {
    kfile_t *kf = (kfile_t *) ksys2(SYS_FOPEN, (unsigned long) path,
                                    (unsigned long) "r");
    if (!kf)
        return 0;
    if (kf->type != KFS_FILE) {
        ksys1(SYS_FCLOSE, (unsigned long) kf);
        return 0;
    }

    size_t len = kf->len;
    /* One spare block: the kernel fread always hands back a whole 512-byte
     * sector, so the last one can overrun a buffer sized to the file. */
    char *buf = malloc(len + 512 + 1);
    if (!buf) {
        ksys1(SYS_FCLOSE, (unsigned long) kf);
        return 0;
    }

    /* kf->len is the authority on the size; the cluster chain is walked by
     * the kernel one sector per call and the spare block above absorbs the
     * overrun of the last, partial one. */
    for (size_t off = 0; off < len; off += 512)
        ksys2(SYS_FREAD, (unsigned long) kf, (unsigned long) (buf + off));
    ksys1(SYS_FCLOSE, (unsigned long) kf);

    buf[len] = 0;
    *out_len = len;
    return buf;
}

FILE *fopen(const char *path, const char *mode) {
    char norm[128];
    const char *p = abspath(path, norm, sizeof norm);
    int writing = mode && (mode[0] == 'w' || mode[0] == 'a');

    FILE *s = pool_get();
    if (!s)
        return 0;

    if (writing) {
        size_t i = 0;
        while (p[i] && i + 1 < sizeof s->path) { s->path[i] = p[i]; i++; }
        s->path[i] = 0;
        s->kind = FK_WR;
        return s;
    }

    size_t len = 0;
    char *buf = slurp(p, &len);
    if (!buf) {
        s->kind = FK_CLOSED;
        return 0;
    }
    s->kind = FK_RD;
    s->buf  = buf;
    s->len  = len;
    return s;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    if (!path && stream == stdin) return stdin;
    if (stream) fclose(stream);
    return path ? fopen(path, mode) : 0;
}

int fclose(FILE *s) {
    if (!s || s->kind == FK_CLOSED)
        return EOF;
    if (s == stdin || s == stdout || s == stderr)
        return 0;        /* closing these would silently mute the console */
    int r = 0;
    if (s->kind == FK_WR) {
        if ((int) ksys3(SYS_SPIT, (unsigned long) s->path,
                        (unsigned long) (s->buf ? s->buf : ""),
                        (unsigned long) s->len) < 0)
            r = EOF;
    }
    free(s->buf);
    s->buf = 0;
    s->kind = FK_CLOSED;
    return r;
}

void *__koppi_steal(FILE *s, size_t *len) {
    if (!s || s->kind != FK_RD)
        return 0;
    void *b = s->buf;
    *len = s->len;
    s->buf = 0;
    s->kind = FK_CLOSED;
    return b;
}

/* ---------------- seeking ---------------- */

int fseek(FILE *s, long off, int whence) {
    if (!s || (s->kind != FK_RD && s->kind != FK_WR))
        return -1;
    long base = (whence == SEEK_CUR) ? (long) s->pos
              : (whence == SEEK_END) ? (long) s->len : 0;
    long np = base + off;
    if (np < 0 || np > (long) s->len)
        return -1;
    s->pos = (size_t) np;
    s->ungot = -1;
    s->eof = 0;
    return 0;
}

long ftell(FILE *s) {
    if (!s) return -1;
    if (s->kind == FK_RD || s->kind == FK_WR) return (long) s->pos;
    return -1;
}

void rewind(FILE *s) { fseek(s, 0, SEEK_SET); }

int setvbuf(FILE *s, char *b, int m, size_t n) {
    (void) s; (void) b; (void) m; (void) n; return 0;
}

/* ---------------- remove / rename ---------------- */

/*
 * There is no unlink or rename syscall. Both operations do exist in the
 * kernel console's command set, though, and the `run` syscall is exactly the
 * door ring 3 uses to reach it (the shell runs every coreutil this way), so
 * borrow them rather than grow the ABI for two calls Doom makes once each,
 * when a save game replaces its predecessor.
 */
static int run_cmd(const char *line) {
    ksys1(18 /* run */, (unsigned long) line);
    return 0;
}

int remove(const char *p) {
    char norm[128], line[160];
    if (snprintf(line, sizeof line, "rm %s",
                 abspath(p, norm, sizeof norm)) >= (int) sizeof line)
        return -1;
    return run_cmd(line);
}

int rename(const char *a, const char *b) {
    /* abspath() may return its argument unchanged, so resolve into two
     * separate buffers before formatting. */
    char na[128], nb[128], line[288];
    const char *pa = abspath(a, na, sizeof na);
    const char *pb = abspath(b, nb, sizeof nb);
    if (snprintf(line, sizeof line, "mv %s %s", pa, pb) >= (int) sizeof line)
        return -1;
    return run_cmd(line);
}

/* ---------------- sscanf ---------------- */

/*
 * Doom uses sscanf for exactly one job: parsing an integer that may be
 * written in decimal, hex or octal (M_StrToInt, and the config reader). This
 * handles the conversions those call sites use -- %d %i %u %x %o %c %s and a
 * literal prefix -- and returns the number of items assigned, which is what
 * they test. It is not a general sscanf and does not pretend to be.
 */
static int scan_int(const char **sp, int base, int is_signed, long *out) {
    const char *s = *sp;
    int neg = 0;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')
        && isxdigit((unsigned char) s[2])) {
        s += 2;
        base = 16;
    } else if (base == 0) {
        base = (s[0] == '0' && isdigit((unsigned char) s[1])) ? 8 : 10;
    }

    const char *start = s;
    long v = 0;
    for (;;) {
        int c = (unsigned char) *s, d;
        if (isdigit(c))       d = c - '0';
        else if (isalpha(c))  d = (c | 32) - 'a' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        s++;
    }
    if (s == start)
        return 0;

    *sp = s;
    *out = (is_signed && neg) ? -v : v;
    return 1;
}

int sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    const char *s = str;
    int assigned = 0;

    for (const char *f = fmt; *f; f++) {
        if (isspace((unsigned char) *f)) {
            while (isspace((unsigned char) *s)) s++;
            continue;
        }
        if (*f != '%') {
            if (*s != *f) goto done;
            s++;
            continue;
        }

        f++;
        int suppress = 0;
        if (*f == '*') { suppress = 1; f++; }
        while (isdigit((unsigned char) *f)) f++;            /* field width */
        while (*f == 'l' || *f == 'h' || *f == 'z') f++;    /* length mods  */

        if (*f != 'c')
            while (isspace((unsigned char) *s)) s++;

        long v = 0;
        switch (*f) {
        case 'd': case 'u': case 'i': case 'x': case 'X': case 'o': {
            int base = (*f == 'd' || *f == 'u') ? 10
                     : (*f == 'o') ? 8
                     : (*f == 'i') ? 0 : 16;
            if (!scan_int(&s, base, *f != 'u' && *f != 'x' && *f != 'X', &v))
                goto done;
            if (!suppress) { *va_arg(ap, int *) = (int) v; assigned++; }
            break;
        }
        case 'c': {
            if (!*s) goto done;
            if (!suppress) { *va_arg(ap, char *) = *s; assigned++; }
            s++;
            break;
        }
        case 's': {
            if (!*s) goto done;
            char *out = suppress ? 0 : va_arg(ap, char *);
            while (*s && !isspace((unsigned char) *s)) {
                if (out) *out++ = *s;
                s++;
            }
            if (out) { *out = 0; assigned++; }
            break;
        }
        case '%':
            if (*s != '%') goto done;
            s++;
            break;
        default:
            goto done;
        }
    }

done:
    va_end(ap);
    return assigned;
}
