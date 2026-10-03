/*
 * The seekable-FILE half of this port's C library.
 *
 * Built against the real system headers, like the rest of this app (and like
 * apps/hello-qt), so `FILE` is glibc's declared type and every caller sees
 * the standard prototypes; the objects behind those pointers are this file's,
 * never glibc's, and nothing in ChipNomad looks inside one.
 *
 * Same three stream kinds and the same whole-file strategy as
 * apps/doom/shim/stdio.c, for the same reason: the kernel VFS has no seek.
 *
 *   console  stdout/stderr through the write syscall, stdin through gets
 *   read     fopen() slurps the file into memory, so fseek/ftell/fread are
 *            pointer arithmetic
 *   write    output accumulates in a growable buffer that fclose() hands to
 *            the spit syscall in one piece
 *
 * The consequence worth knowing: nothing a write stream produces reaches the
 * disk before fclose(). ChipNomad's savers all close what they open (and the
 * autosave path reopens from scratch each minute), so a project is either the
 * previous version or the new one, never a half-written file -- which is a
 * better failure mode than the real thing has.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ksys.h"

/* printf.c (vendored mpaland printf, built with the SOFT alias config) --
 * declared here rather than including its header, which would #define the
 * standard names over the ones this file defines. */
int vsnprintf_(char *s, size_t count, const char *format, va_list arg);

size_t _write(const void *buf, size_t len) {
    return (size_t) ksys2(SYS_WRITE, (unsigned long) buf, (unsigned long) len);
}

enum { FK_CLOSED = 0, FK_OUT, FK_IN, FK_RD, FK_WR };

#define PATH_CAP 160
#define POOL_MAX 16

struct kfile {
    int    kind;
    char  *buf;     /* FK_RD: the whole file. FK_WR: what has been written. */
    size_t len;     /* bytes of content */
    size_t cap;     /* FK_WR: allocated size of buf */
    size_t pos;     /* cursor */
    int    ungot;   /* pushed-back byte, or -1 */
    int    eof, err;
    char   path[PATH_CAP];  /* FK_WR: where fclose() spits the buffer */
};

static struct kfile f_stdin  = { FK_IN,  0, 0, 0, 0, -1, 0, 0, {0} };
static struct kfile f_stdout = { FK_OUT, 0, 0, 0, 0, -1, 0, 0, {0} };
static struct kfile f_stderr = { FK_OUT, 0, 0, 0, 0, -1, 0, 0, {0} };

FILE *stdin  = (FILE *) &f_stdin;
FILE *stdout = (FILE *) &f_stdout;
FILE *stderr = (FILE *) &f_stderr;

static struct kfile pool[POOL_MAX];

/* stdin is read a console line at a time into this. */
static char line_buf[256];
static int  line_pos, line_len;

#define S(f) ((struct kfile *) (void *) (f))

/* ------------------------------------------------------------------ *
 *  Paths                                                              *
 * ------------------------------------------------------------------ */

/*
 * Make @p in absolute. There is no per-process working directory in this
 * kernel -- the one `cd` maintains belongs to the console -- so a relative
 * path resolves against that, and against the boot RAM disk when the console
 * sits at the root (where a bare "/" names no device).
 */
static const char *abspath(const char *in, char *out, size_t n) {
    if (in[0] == '/')
        return in;

    char cwd[80];
    unsigned got = (unsigned) ksys2(SYS_GETCWD, (unsigned long) cwd,
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

/* ------------------------------------------------------------------ *
 *  Output                                                             *
 * ------------------------------------------------------------------ */

void putchar_(char c) { _write(&c, 1); }     /* printf.c's output hook */

int putchar(int c) { char ch = (char) c; _write(&ch, 1); return c; }

int puts(const char *s) {
    _write(s, strlen(s));
    _write("\n", 1);
    return 0;
}

/** Make room for @p extra more bytes in a write stream. */
static int wr_reserve(struct kfile *s, size_t extra) {
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

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f) {
    struct kfile *s = S(f);
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

int fputc(int c, FILE *f) {
    struct kfile *s = S(f);
    unsigned char ch = (unsigned char) c;
    if (!s) return EOF;
    if (s->kind == FK_OUT) { _write(&ch, 1); return c; }
    return fwrite(&ch, 1, 1, f) == 1 ? c : EOF;
}

int fputs(const char *str, FILE *f) {
    struct kfile *s = S(f);
    size_t n = strlen(str);
    if (!s) return EOF;
    if (s->kind == FK_OUT) { _write(str, n); return 0; }
    return fwrite(str, 1, n, f) == n ? 0 : EOF;
}

/*
 * ChipNomad writes its project files a line at a time, and the longest single
 * fprintf in the format is a pitch-table or wavetable row; 1 KiB covers every
 * one of them with room to spare. Output longer than that is truncated rather
 * than written half-formatted, and the return value still reports the length
 * the format wanted, as the standard requires.
 */
int vfprintf(FILE *f, const char *fmt, va_list ap) {
    struct kfile *s = S(f);
    char tmp[1024];
    int n = vsnprintf_(tmp, sizeof tmp, fmt, ap);
    if (n < 0) return n;
    size_t w = ((size_t) n >= sizeof tmp) ? sizeof tmp - 1 : (size_t) n;
    if (s && s->kind == FK_OUT)
        _write(tmp, w);
    else if (s)
        fwrite(tmp, 1, w, f);
    return n;
}

int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int fflush(FILE *f) { (void) f; return 0; }

/* ------------------------------------------------------------------ *
 *  Input                                                              *
 * ------------------------------------------------------------------ */

/* The kernel gets() strips the newline and returns a lone \x04 for Ctrl-D;
 * re-add the '\n' and map Ctrl-D to EOF. */
static int refill_line(struct kfile *s) {
    line_pos = line_len = 0;
    ksys2(SYS_GETS, (unsigned long) line_buf, (unsigned long) sizeof line_buf - 1);
    if (line_buf[0] == 4) { s->eof = 1; return 0; }
    int n = (int) strlen(line_buf);
    line_buf[n] = '\n';
    line_len = n + 1;
    return line_len;
}

int fgetc(FILE *f) {
    struct kfile *s = S(f);
    if (!s) return EOF;
    if (s->ungot >= 0) { int c = s->ungot; s->ungot = -1; return c; }
    if (s->kind == FK_IN) {
        if (line_pos >= line_len && refill_line(s) <= 0)
            return EOF;
        return (unsigned char) line_buf[line_pos++];
    }
    if (s->kind != FK_RD || s->pos >= s->len) {
        s->eof = 1;
        return EOF;
    }
    return (unsigned char) s->buf[s->pos++];
}

int getc(FILE *f) { return fgetc(f); }

int ungetc(int c, FILE *f) {
    struct kfile *s = S(f);
    if (!s || c == EOF) return EOF;
    s->ungot = (unsigned char) c;
    s->eof = 0;
    return c;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f) {
    struct kfile *s = S(f);
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

    unsigned char *out = (unsigned char *) ptr;
    size_t done = 0;
    while (done < want) {
        int c = fgetc(f);
        if (c == EOF) break;
        out[done++] = (unsigned char) c;
    }
    return size ? done / size : 0;
}

char *fgets(char *dst, int size, FILE *f) {
    if (size <= 0) return 0;
    int i = 0;
    while (i < size - 1) {
        int c = fgetc(f);
        if (c == EOF) break;
        dst[i++] = (char) c;
        if (c == '\n') break;
    }
    if (i == 0) return 0;
    dst[i] = 0;
    return dst;
}

int feof(FILE *f)   { return S(f) ? S(f)->eof : 1; }
int ferror(FILE *f) { return S(f) ? S(f)->err : 0; }
void clearerr(FILE *f) { if (S(f)) { S(f)->eof = 0; S(f)->err = 0; } }

/* ------------------------------------------------------------------ *
 *  Open / close                                                       *
 * ------------------------------------------------------------------ */

static struct kfile *pool_get(void) {
    for (int i = 0; i < POOL_MAX; i++) {
        if (pool[i].kind == FK_CLOSED) {
            struct kfile *s = &pool[i];
            memset(s, 0, sizeof *s);
            s->ungot = -1;
            return s;
        }
    }
    return 0;
}

/* Read the whole file at @p path. Returns the buffer (NUL-terminated for text
 * callers) and writes its length to @p out_len, or NULL. */
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
    char *buf = (char *) malloc(len + 512 + 1);
    if (!buf) {
        ksys1(SYS_FCLOSE, (unsigned long) kf);
        return 0;
    }

    for (size_t off = 0; off < len; off += 512)
        ksys2(SYS_FREAD, (unsigned long) kf, (unsigned long) (buf + off));
    ksys1(SYS_FCLOSE, (unsigned long) kf);

    buf[len] = 0;
    *out_len = len;
    return buf;
}

FILE *fopen(const char *path, const char *mode) {
    char norm[PATH_CAP];
    const char *p = abspath(path, norm, sizeof norm);
    int writing = mode && (mode[0] == 'w' || mode[0] == 'a');

    struct kfile *s = pool_get();
    if (!s) {
        errno = EMFILE;
        return 0;
    }

    if (writing) {
        size_t i = 0;
        while (p[i] && i + 1 < sizeof s->path) { s->path[i] = p[i]; i++; }
        s->path[i] = 0;
        s->kind = FK_WR;
        /* "a" keeps what is already there; there is no append syscall, so the
         * existing contents become the start of the write buffer. */
        if (mode[0] == 'a') {
            size_t len = 0;
            char *old = slurp(p, &len);
            if (old) {
                s->buf = old;
                s->len = s->cap = len;
                s->pos = len;
            }
        }
        return (FILE *) s;
    }

    size_t len = 0;
    char *buf = slurp(p, &len);
    if (!buf) {
        s->kind = FK_CLOSED;
        errno = ENOENT;
        return 0;
    }
    s->kind = FK_RD;
    s->buf  = buf;
    s->len  = len;
    return (FILE *) s;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    if (!path && stream == stdin) return stdin;
    if (stream) fclose(stream);
    return path ? fopen(path, mode) : 0;
}

int fclose(FILE *f) {
    struct kfile *s = S(f);
    if (!s || s->kind == FK_CLOSED)
        return EOF;
    if (f == stdin || f == stdout || f == stderr)
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

/* ------------------------------------------------------------------ *
 *  Seeking                                                            *
 * ------------------------------------------------------------------ */

int fseek(FILE *f, long off, int whence) {
    struct kfile *s = S(f);
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

long ftell(FILE *f) {
    struct kfile *s = S(f);
    if (!s) return -1;
    if (s->kind == FK_RD || s->kind == FK_WR) return (long) s->pos;
    return -1;
}

void rewind(FILE *f) { fseek(f, 0, SEEK_SET); }

int setvbuf(FILE *f, char *b, int m, size_t n) {
    (void) f; (void) b; (void) m; (void) n; return 0;
}

/* ------------------------------------------------------------------ *
 *  remove / rename / mkdir                                            *
 * ------------------------------------------------------------------ */

/*
 * There is no unlink or rename syscall, but both operations exist in the
 * kernel console's command set and the `run` syscall is the door ring 3 uses
 * to reach it (apps/zsh runs every coreutil this way), so borrow them rather
 * than grow the ABI -- the same trade apps/doom/shim/stdio.c makes.
 */
int remove(const char *p) {
    char norm[PATH_CAP], line[PATH_CAP + 32];
    if (snprintf(line, sizeof line, "rm %s",
                 abspath(p, norm, sizeof norm)) >= (int) sizeof line)
        return -1;
    ksys1(SYS_RUN, (unsigned long) line);
    return 0;
}

int rename(const char *a, const char *b) {
    /* abspath() may return its argument unchanged, so resolve into two
     * separate buffers before formatting. */
    char na[PATH_CAP], nb[PATH_CAP], line[2 * PATH_CAP + 32];
    const char *pa = abspath(a, na, sizeof na);
    const char *pb = abspath(b, nb, sizeof nb);
    if (snprintf(line, sizeof line, "mv %s %s", pa, pb) >= (int) sizeof line)
        return -1;
    ksys1(SYS_RUN, (unsigned long) line);
    return 0;
}

/*
 * Honest failure. Directories are the one filesystem concept this kernel does
 * not have on the write side: the FAT driver's operation vector (vfs.h) has
 * touch and delete but no mkdir, and fat_listdir() only ever walks a volume's
 * root. ChipNomad's file browser calls this from its "create folder" screen,
 * which reports the failure to the user rather than pretending.
 */
int mkdir(const char *path, mode_t mode) {
    (void) path; (void) mode;
    errno = ENOSYS;
    return -1;
}
