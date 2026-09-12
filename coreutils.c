/**
 * @file coreutils.c
 * @brief A busybox-style toolbox of coreutils / util-linux commands for the
 *        kernel debug console.
 *
 * @ref console_exec (commands.c) tries its handful of "native" built-ins first
 * (ls, cd, start, read, ...) and then hands anything it does not recognise to
 * @ref coreutils_try. Every command below is therefore reachable from the
 * in-kernel console, from the userspace shell (apps/zsh, through the `run`
 * syscall) and over an SSH channel, with identical behaviour.
 *
 * Constraints this file works within:
 *   - The kernel heap is tiny (~140 KiB), so nothing here calls kmalloc for
 *     bulk data. Streaming commands read a file 512 bytes at a time; the few
 *     commands that must see a whole file at once (sort, tac, tail, uniq) copy
 *     it into a fixed line pool, and cp/mv/base64 use one fixed I/O buffer.
 *   - The FAT driver is root-directory-only and has no rename or mkdir, so
 *     mv is copy+delete and there is no mkdir/ln.
 *   - The console has no pipes, no redirection and no exit status, so filters
 *     that classically read stdin (grep, sort, tr, ...) take a file argument.
 */

#include <lib/string.h>
#include <printf.h>
#include <types.h>
#include <vfs.h>
#include <mm.h>
#include <kheap.h>
#include <sched.h>
#include <pit.h>
#include <rtc.h>
#include <keyboard.h>
#include <sha2.h>
#include <io.h>
#include <acpi.h>

#include <net.h>
#include <tcp.h>
#include <dns.h>
#include <ver.h>
#include <log.h>
#include <commands.h>


/* ------------------------------------------------------------------ output -- */

/** @brief Write a NUL-terminated string to the console. */
static void o_str(const char *s)              { for (; *s; s++) putchar_(*s); }
/** @brief Write @p n bytes, NULs included, to the console. */
static void o_buf(const char *s, unsigned n)  { for (unsigned i = 0; i < n; i++) putchar_(s[i]); }
/** @brief Write a newline. */
static void o_nl(void)                        { putchar_('\n'); }

/* ---------------------------------------------------------- string helpers -- */

/** @brief strlen for const strings, without the cast lib/string.h needs. */
static int  s_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
/** @brief String equality. @return Non-zero if @p a and @p b match. */
static int  s_eq(const char *a, const char *b) { return strcmp((char *)a, (char *)b) == 0; }

/**
 * @brief Copy @p s into @p d, always NUL-terminating.
 * @param d   Destination.
 * @param s   Source.
 * @param cap Size of @p d; at most @p cap - 1 characters are copied.
 *
 * Unlike strncpy this never leaves @p d unterminated, which the fixed-size
 * buffers all over this file rely on.
 */
static void s_cpy(char *d, const char *s, int cap) {
    int i = 0;
    for (; s[i] && i < cap - 1; i++) d[i] = s[i];
    d[i] = 0;
}

/** @brief ASCII lowercase. Bytes outside A-Z pass through unchanged. */
static char lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
/** @brief True for the six ASCII whitespace characters isspace() accepts. */
static int  is_ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }
/** @brief True for ASCII '0'-'9'. */
static int  is_digit(char c) { return c >= '0' && c <= '9'; }

/** Signed decimal parse that stops at the first non-digit (unlike lib/atoi). */
static long s_num(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    long v = 0;
    while (is_digit(*s)) v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

/** strstr, optionally case-insensitive. */
static const char *s_find(const char *hay, const char *needle, int fold) {
    if (!*needle) return hay;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n) {
            char a = fold ? lc(*h) : *h;
            char b = fold ? lc(*n) : *n;
            if (a != b) break;
            h++; n++;
        }
        if (!*n) return hay;
    }
    return 0;
}

/* ----------------------------------------------------------- argv splitter -- */

#define CU_MAXARG 32
static char  cu_linebuf[512];
static char *cu_argv[CU_MAXARG];

/**
 * @brief Split a command line into argv on spaces and tabs.
 * @param line The line; it is copied first, so the caller's buffer is untouched.
 * @return The argument count, at most @ref CU_MAXARG.
 *
 * Words are cut in a private copy (@c cu_linebuf) and @c cu_argv points into
 * it, so both stay valid until the next call. There is no quoting and no
 * escaping: the console has no shell grammar, so "a b" is two arguments.
 */
static int cu_split(char *line) {
    s_cpy(cu_linebuf, line, sizeof cu_linebuf);
    int argc = 0;
    char *p = cu_linebuf;
    while (*p && argc < CU_MAXARG) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        cu_argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
    }
    return argc;
}

/* --------------------------------------------------------- file reading ----- */

/**
 * @brief Resolve @p name against the console's cwd and open it for reading.
 * @param name Path as typed, absolute or relative.
 * @return An open handle, or 0 after printing the reason (path too long, or
 *         no such file). A directory counts as "no such file" here.
 */
static file *cu_fopen(const char *name) {
    char path[96];
    if (!console_resolve_path(path, sizeof path, name)) {
        printf("%s: path too long\n", name);
        return 0;
    }
    file *f = vfs_file_open(path, "r");
    if (f->type != FS_FILE) {
        vfs_file_close(f);
        printf("%s: No such file or directory\n", name);
        return 0;
    }
    return f;
}

typedef int (*cu_chunk_fn)(const char *buf, unsigned n, void *ctx);

/** Feed @p f to @p fn in <=512-byte pieces. @return 1 if @p fn asked to stop. */
static int cu_chunks(file *f, cu_chunk_fn fn, void *ctx) {
    unsigned total = f->len, done = 0;
    while (f->eof != 1 && done < total) {
        char cb[512];
        memset(cb, 0, sizeof cb);
        vfs_file_read(f, cb);
        unsigned n = total - done;
        if (n > 512) n = 512;
        if (fn(cb, n, ctx)) return 1;
        done += 512;
    }
    return 0;
}

typedef int (*cu_line_fn)(const char *line, unsigned len, unsigned no, void *ctx);

struct cu_lines {
    char        line[2048];
    unsigned    len;
    unsigned    no;
    cu_line_fn  fn;
    void       *ctx;
};

/**
 * @brief @ref cu_chunks callback that reassembles lines for @ref cu_lines.
 *
 * Drops CR so CRLF files read the same as LF ones, and silently truncates a
 * line longer than @c struct @c cu_lines::line rather than splitting it.
 */
static int cu__line_chunk(const char *buf, unsigned n, void *v) {
    struct cu_lines *L = v;
    for (unsigned i = 0; i < n; i++) {
        char c = buf[i];
        if (c == '\r') continue;
        if (c == '\n') {
            L->line[L->len] = 0;
            if (L->fn(L->line, L->len, ++L->no, L->ctx)) return 1;
            L->len = 0;
        } else if (L->len < sizeof L->line - 1) {
            L->line[L->len++] = c;
        }
    }
    return 0;
}

/**
 * @brief Feed @p f to @p fn one line at a time.
 * @param f   Open file.
 * @param fn  Called per line with (text, length, 1-based number, @p ctx);
 *            a non-zero return stops the scan.
 * @param ctx Passed through to @p fn.
 *
 * A trailing fragment with no final newline is delivered as a last line, but
 * only if @p fn did not already ask to stop.
 */
static void cu_lines(file *f, cu_line_fn fn, void *ctx) {
    struct cu_lines L;
    L.len = 0; L.no = 0; L.fn = fn; L.ctx = ctx;
    if (!cu_chunks(f, cu__line_chunk, &L) && L.len > 0) {
        L.line[L.len] = 0;
        fn(L.line, L.len, ++L.no, ctx);
    }
}

/*
 * Whole-file line pool for sort / tac / tail / uniq. The kernel image sits just
 * below the 4 MiB identity-map ceiling with only a ~100 KiB heap above it, so
 * this pool is deliberately small: a debug-console `sort` is not expected to
 * handle a novel.
 */
#define CU_POOL_LINES 64
#define CU_POOL_W     144
static char cu_pool[CU_POOL_LINES][CU_POOL_W];
static int  cu_pool_n;
static int  cu_pool_over;

/**
 * @brief @ref cu_lines callback that copies lines into @ref cu_pool.
 *
 * Stops the scan and sets @c cu_pool_over once the pool is full; a line wider
 * than @ref CU_POOL_W is truncated rather than dropped.
 */
static int cu__pool_line(const char *line, unsigned len, unsigned no, void *ctx) {
    (void)no; (void)ctx;
    if (cu_pool_n >= CU_POOL_LINES) { cu_pool_over = 1; return 1; }
    unsigned c = len;
    if (c > CU_POOL_W - 1) c = CU_POOL_W - 1;
    memcpy(cu_pool[cu_pool_n], (char *)line, c);
    cu_pool[cu_pool_n][c] = 0;
    cu_pool_n++;
    return 0;
}

/** Load @p name's lines into cu_pool. @return 0 on success, -1 if it won't open. */
static int cu_slurp(const char *name, int reset) {
    if (reset) { cu_pool_n = 0; cu_pool_over = 0; }
    file *f = cu_fopen(name);
    if (!f) return -1;
    cu_lines(f, cu__pool_line, 0);
    vfs_file_close(f);
    return 0;
}

/*
 * cp / mv / base64 / wget need a whole file in RAM at once. There is no fixed
 * buffer for it (no room in BSS); instead kmalloc a right-sized block from the
 * kernel heap and free it straight after. cu_io / cu_io_len point at the live
 * block between cu_slurp_heap() and cu_free_heap().
 */
static char     *cu_io;
static unsigned  cu_io_len;

struct cu_io_fill { unsigned cap; };
/**
 * @brief @ref cu_chunks callback that appends to the @ref cu_io heap buffer.
 *
 * Stops the scan rather than overrunning if the file turns out longer than the
 * block that was sized from @c f->len.
 */
static int cu__io_chunk(const char *b, unsigned n, void *v) {
    struct cu_io_fill *s = v;
    if (cu_io_len + n > s->cap) return 1;
    memcpy(cu_io + cu_io_len, (char *)b, n);
    cu_io_len += n;
    return 0;
}

/** kmalloc a buffer and read @p name into it. @return 0 ok (caller must call
 *  cu_free_heap()), -1 on open failure / out of heap / file too large. */
static int cu_slurp_heap(const char *name) {
    cu_io = 0;
    cu_io_len = 0;
    file *f = cu_fopen(name);
    if (!f) return -1;
    unsigned need = f->len;
    unsigned budget = 0;
    int hs = get_heap_size(), hu = get_used_heap();
    if (hs > hu) budget = (unsigned)(hs - hu);
    budget = budget > 16384 ? budget - 16384 : 0;   /* leave the heap some air */
    if (need + 1 > budget) {
        printf("%s: file too large for the %u KiB kernel heap\n", name, budget / 1024);
        vfs_file_close(f);
        return -1;
    }
    cu_io = kmalloc(need + 1);
    if (!cu_io) { printf("%s: out of memory\n", name); vfs_file_close(f); return -1; }
    struct cu_io_fill s = { need };
    cu_chunks(f, cu__io_chunk, &s);
    cu_io[cu_io_len] = 0;
    vfs_file_close(f);
    return 0;
}

/**
 * @brief Release the buffer @ref cu_slurp_heap allocated.
 *
 * Safe to call when no buffer is live, so an error path can call it blindly.
 */
static void cu_free_heap(void) {
    if (cu_io) { kfree(cu_io); cu_io = 0; }
    cu_io_len = 0;
}

/* =========================================================================
 *  file / text commands
 * ========================================================================= */

/** @brief @ref cu_chunks callback for plain `cat`: copy bytes straight out. */
static int cu__cat_raw(const char *b, unsigned n, void *c) { (void)c; o_buf(b, n); return 0; }

struct catn { unsigned no; };
/**
 * @brief @ref cu_lines callback for `cat -n`.
 *
 * Numbers from its own counter rather than @p no so that several files
 * numbered in one command continue the sequence, as cat(1) does.
 */
static int cu__cat_num(const char *l, unsigned len, unsigned no, void *v) {
    (void)len; (void)no;
    struct catn *c = v;
    printf("%6u\t", ++c->no);
    o_str(l); o_nl();
    return 0;
}

/**
 * @brief cat(1): concatenate files to the console.
 *
 * Accepts -n (number lines) and --. Without -n the bytes are streamed
 * untouched, so binary files come out verbatim.
 */
static void cmd_cat(int argc, char **argv) {
    int number = 0, first = 1;
    struct catn cn = { 0 };
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-n")) { number = 1; continue; }
        if (s_eq(argv[i], "--")) continue;
        first = 0;
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        if (number) cu_lines(f, cu__cat_num, &cn);
        else        cu_chunks(f, cu__cat_raw, 0);
        vfs_file_close(f);
    }
    if (first) printf("usage: %s [-n] FILE...\n", argv[0]);
}

struct headctx { int limit, printed; };
/** @brief @ref cu_lines callback for `head`: print until the limit is hit. */
static int cu__head_line(const char *l, unsigned len, unsigned no, void *v) {
    (void)len; (void)no;
    struct headctx *h = v;
    if (h->printed >= h->limit) return 1;
    o_str(l); o_nl();
    return ++h->printed >= h->limit;
}

/**
 * Pull a "-n N" / "-nN" / "-N" line count out of argv and collect the file
 * operands into @p files. @return the file count; *limit holds the line count.
 */
static int ht_parse(int argc, char **argv, int *limit, char **files) {
    int nf = 0;
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-n") && i + 1 < argc) { *limit = (int)s_num(argv[++i]); continue; }
        if (argv[i][0] == '-' && argv[i][1] == 'n' && is_digit(argv[i][2])) { *limit = (int)s_num(argv[i] + 2); continue; }
        if (argv[i][0] == '-' && is_digit(argv[i][1])) { *limit = (int)s_num(argv[i] + 1); continue; }
        if (argv[i][0] == '-') continue;
        files[nf++] = argv[i];
    }
    return nf;
}

/**
 * @brief head(1): print the first N lines of each file. N defaults to 10.
 *
 * Prints a "==> name <==" banner when given more than one file.
 */
static void cmd_head(int argc, char **argv) {
    int limit = 10;
    char *files[CU_MAXARG];
    int nf = ht_parse(argc, argv, &limit, files);
    if (!nf) { printf("usage: head [-n N] FILE...\n"); return; }
    for (int i = 0; i < nf; i++) {
        file *f = cu_fopen(files[i]);
        if (!f) continue;
        if (nf > 1) printf("%s==> %s <==\n", i ? "\n" : "", files[i]);
        struct headctx h = { limit, 0 };
        cu_lines(f, cu__head_line, &h);
        vfs_file_close(f);
    }
}

/**
 * @brief tail(1): print the last N lines of each file. N defaults to 10.
 *
 * The file has to be buffered to find its end, so this goes through
 * @ref cu_pool and sees only the first @ref CU_POOL_LINES lines of a long
 * file — it says so rather than printing the wrong tail silently.
 */
static void cmd_tail(int argc, char **argv) {
    int limit = 10;
    char *files[CU_MAXARG];
    int nf = ht_parse(argc, argv, &limit, files);
    if (!nf) { printf("usage: tail [-n N] FILE...\n"); return; }
    for (int i = 0; i < nf; i++) {
        if (cu_slurp(files[i], 1) < 0) continue;
        if (nf > 1) printf("%s==> %s <==\n", i ? "\n" : "", files[i]);
        int start = cu_pool_n - limit;
        if (start < 0) start = 0;
        for (int k = start; k < cu_pool_n; k++) { o_str(cu_pool[k]); o_nl(); }
        if (cu_pool_over) printf("tail: %s: only the last of the first %d lines\n", files[i], CU_POOL_LINES);
    }
}

struct wcctx { unsigned l, w, c; int inword; };
/**
 * @brief @ref cu_chunks callback for `wc`: count lines, words and bytes.
 *
 * A word ends at any @ref is_ws byte, and the line count is newline
 * characters, so a file without a final newline counts one line fewer.
 */
static int cu__wc_chunk(const char *b, unsigned n, void *v) {
    struct wcctx *x = v;
    for (unsigned i = 0; i < n; i++) {
        x->c++;
        if (b[i] == '\n') x->l++;
        if (is_ws(b[i])) x->inword = 0;
        else if (!x->inword) { x->inword = 1; x->w++; }
    }
    return 0;
}

/**
 * @brief wc(1): count lines, words and bytes.
 *
 * Accepts -l, -w and -c; -m is treated as -c, there being no multibyte
 * decoding. With no selector all three are printed. A "total" line follows
 * when more than one file was read successfully.
 */
static void cmd_wc(int argc, char **argv) {
    int wl = 0, ww = 0, wc = 0, files = 0;
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-l")) wl = 1;
        else if (s_eq(argv[i], "-w")) ww = 1;
        else if (s_eq(argv[i], "-c") || s_eq(argv[i], "-m")) wc = 1;
        else if (argv[i][0] != '-') files++;
    }
    if (!wl && !ww && !wc) wl = ww = wc = 1;
    if (!files) { printf("usage: %s [-l|-w|-c] FILE...\n", argv[0]); return; }
    struct wcctx tot = { 0, 0, 0, 0 };
    int nfiles = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        nfiles++;
        struct wcctx x = { 0, 0, 0, 0 };
        cu_chunks(f, cu__wc_chunk, &x);
        vfs_file_close(f);
        tot.l += x.l; tot.w += x.w; tot.c += x.c;
        if (wl) printf("%8u", x.l);
        if (ww) printf("%8u", x.w);
        if (wc) printf("%8u", x.c);
        printf(" %s\n", argv[i]);
    }
    if (nfiles > 1) {
        if (wl) printf("%8u", tot.l);
        if (ww) printf("%8u", tot.w);
        if (wc) printf("%8u", tot.c);
        printf(" total\n");
    }
}

struct grepctx {
    const char *pat, *name;
    int fold, invert, showno, count, prefix;
    unsigned hits;
};
/**
 * @brief @ref cu_lines callback for `grep`: match, count and print one line.
 *
 * Always scans to the end of the file, since -c needs the full count.
 */
static int cu__grep_line(const char *l, unsigned len, unsigned no, void *v) {
    (void)len;
    struct grepctx *g = v;
    int m = s_find(l, g->pat, g->fold) != 0;
    if (m == g->invert) return 0;
    g->hits++;
    if (g->count) return 0;
    if (g->prefix) printf("%s:", g->name);
    if (g->showno) printf("%u:", no);
    o_str(l); o_nl();
    return 0;
}

/**
 * @brief grep(1): print lines containing a pattern.
 *
 * Accepts bundled -i, -v, -n and -c. The pattern is a literal substring, not
 * a regular expression, which is also why egrep and fgrep are aliases of this
 * same function. With several files each line is prefixed with its filename.
 */
static void cmd_grep(int argc, char **argv) {
    struct grepctx g = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int ai = 1, files = 0;
    for (; ai < argc && argv[ai][0] == '-' && argv[ai][1]; ai++) {
        for (const char *o = argv[ai] + 1; *o; o++)
            switch (*o) {
                case 'i': g.fold = 1; break;
                case 'v': g.invert = 1; break;
                case 'n': g.showno = 1; break;
                case 'c': g.count = 1; break;
                default: printf("grep: bad option -%c\n", *o); return;
            }
    }
    if (ai >= argc) { printf("usage: grep [-ivnc] PATTERN FILE...\n"); return; }
    g.pat = argv[ai++];
    for (int i = ai; i < argc; i++) files++;
    if (!files) { printf("grep: no input files\n"); return; }
    g.prefix = files > 1;
    for (int i = ai; i < argc; i++) {
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        g.name = argv[i];
        g.hits = 0;
        cu_lines(f, cu__grep_line, &g);
        vfs_file_close(f);
        if (g.count) {
            if (g.prefix) printf("%s:", argv[i]);
            printf("%u\n", g.hits);
        }
    }
}

/**
 * @brief @ref cu_lines callback for `nl`.
 *
 * Empty lines are echoed without consuming a number, matching nl(1)'s default
 * numbering of non-empty lines only.
 */
static int cu__nl_line(const char *l, unsigned len, unsigned no, void *v) {
    unsigned *n = v;
    (void)no;
    if (len) printf("%6u\t%s\n", ++*n, l);
    else o_nl();
    return 0;
}
/**
 * @brief nl(1): number the non-empty lines of each file.
 *
 * The count runs across all the files named in one command.
 */
static void cmd_nl(int argc, char **argv) {
    if (argc < 2) { printf("usage: nl FILE...\n"); return; }
    unsigned n = 0;
    for (int i = 1; i < argc; i++) {
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        cu_lines(f, cu__nl_line, &n);
        vfs_file_close(f);
    }
}

/**
 * @brief tac(1): print each file's lines in reverse.
 *
 * Buffered through @ref cu_pool, so a file longer than @ref CU_POOL_LINES
 * lines is reversed only as far as the pool reached.
 */
static void cmd_tac(int argc, char **argv) {
    if (argc < 2) { printf("usage: tac FILE...\n"); return; }
    for (int i = 1; i < argc; i++) {
        if (cu_slurp(argv[i], 1) < 0) continue;
        for (int k = cu_pool_n - 1; k >= 0; k--) { o_str(cu_pool[k]); o_nl(); }
    }
}

/** @brief @ref cu_lines callback for `rev`: print one line back to front. */
static int cu__rev_line(const char *l, unsigned len, unsigned no, void *v) {
    (void)no; (void)v;
    for (int i = (int)len - 1; i >= 0; i--) putchar_(l[i]);
    o_nl();
    return 0;
}
/** @brief rev(1): reverse the characters of every line. Streams, unlike tac. */
static void cmd_rev(int argc, char **argv) {
    if (argc < 2) { printf("usage: rev FILE...\n"); return; }
    for (int i = 1; i < argc; i++) {
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        cu_lines(f, cu__rev_line, 0);
        vfs_file_close(f);
    }
}

/* cut -----------------------------------------------------------------------*/
struct cutctx { int mode_c; char delim; unsigned lo[16], hi[16]; int nr; };

/**
 * @brief Test a field or column number against the parsed -f/-c list.
 * @param c     Parsed cut state.
 * @param field 1-based field or character position.
 * @return Non-zero if it falls in any of the selected ranges.
 */
static int cut_wanted(struct cutctx *c, unsigned field) {
    for (int i = 0; i < c->nr; i++)
        if (field >= c->lo[i] && field <= c->hi[i]) return 1;
    return 0;
}
/**
 * @brief Parse a cut(1) range list such as "1,3-5,7-" into @p c.
 * @param c Receives up to 16 lo/hi pairs.
 * @param s The list.
 *
 * An open upper bound ("3-") becomes a very large hi, and an open lower bound
 * ("-5") starts at 1. Ranges past the sixteenth are dropped.
 */
static void cut_parse_list(struct cutctx *c, const char *s) {
    c->nr = 0;
    while (*s && c->nr < 16) {
        unsigned a = 0, b;
        int hasa = 0;
        while (is_digit(*s)) { a = a * 10 + (*s++ - '0'); hasa = 1; }
        if (*s == '-') {
            s++;
            b = 0;
            int hasb = 0;
            while (is_digit(*s)) { b = b * 10 + (*s++ - '0'); hasb = 1; }
            if (!hasb) b = 0xffffffu;
            if (!hasa) a = 1;
        } else {
            b = a;
        }
        if (hasa || a) { c->lo[c->nr] = a ? a : 1; c->hi[c->nr] = b; c->nr++; }
        if (*s == ',') s++;
        else break;
    }
}
/**
 * @brief @ref cu_lines callback for `cut`, in both -c and -f mode.
 *
 * In field mode the delimiter is re-inserted between the fields that survive.
 * A line with no delimiter at all is emitted as a single field, so it is kept
 * only if field 1 was selected — cut(1)'s behaviour without -s.
 */
static int cu__cut_line(const char *l, unsigned len, unsigned no, void *v) {
    (void)no;
    struct cutctx *c = v;
    if (c->mode_c) {
        for (unsigned i = 0; i < len; i++)
            if (cut_wanted(c, i + 1)) putchar_(l[i]);
        o_nl();
        return 0;
    }
    unsigned field = 1, start = 0, printed = 0;
    for (unsigned i = 0; i <= len; i++) {
        if (i == len || l[i] == c->delim) {
            if (cut_wanted(c, field)) {
                if (printed++) putchar_(c->delim);
                o_buf(l + start, i - start);
            }
            field++;
            start = i + 1;
        }
    }
    o_nl();
    return 0;
}
/**
 * @brief cut(1): select fields (-f) or character columns (-c) from each line.
 *
 * The delimiter (-d) defaults to tab. Both the separated ("-f 1") and joined
 * ("-f1") option forms are accepted.
 */
static void cmd_cut(int argc, char **argv) {
    struct cutctx c = { 0, '\t', {0}, {0}, 0 };
    const char *list = 0;
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-d") && i + 1 < argc) c.delim = argv[++i][0];
        else if (argv[i][0] == '-' && argv[i][1] == 'd') c.delim = argv[i][2];
        else if (s_eq(argv[i], "-f") && i + 1 < argc) list = argv[++i];
        else if (s_eq(argv[i], "-c") && i + 1 < argc) { c.mode_c = 1; list = argv[++i]; }
        else if (argv[i][0] == '-' && argv[i][1] == 'f') list = argv[i] + 2;
        else if (argv[i][0] == '-' && argv[i][1] == 'c') { c.mode_c = 1; list = argv[i] + 2; }
    }
    if (!list) { printf("usage: cut -f LIST [-d C] FILE... | cut -c LIST FILE...\n"); return; }
    cut_parse_list(&c, list);
    int did = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') { if ((s_eq(argv[i], "-d") || s_eq(argv[i], "-f") || s_eq(argv[i], "-c"))) i++; continue; }
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        did = 1;
        cu_lines(f, cu__cut_line, &c);
        vfs_file_close(f);
    }
    if (!did) printf("cut: no input files\n");
}

/* tr ----------------------------------------------------------------------- */
/**
 * @brief Expand a tr(1) set, turning "a-z" ranges into their members.
 * @param s   The set as typed.
 * @param out Receives up to 255 bytes.
 * @return The number of bytes written.
 *
 * Backslash escapes and character classes ([:alpha:] and friends) are not
 * supported; every other byte stands for itself.
 */
static int tr_expand(const char *s, unsigned char *out) {
    int n = 0;
    while (*s && n < 255) {
        if (s[1] == '-' && s[2] && (unsigned char)s[2] >= (unsigned char)s[0]) {
            for (unsigned char c = s[0]; c <= (unsigned char)s[2] && n < 255; c++) out[n++] = c;
            s += 3;
        } else {
            out[n++] = (unsigned char)*s++;
        }
    }
    return n;
}
struct trctx { unsigned char map[256]; int del; unsigned char set1[256]; int n1; };
/**
 * @brief @ref cu_chunks callback for `tr`, deleting or translating bytes.
 *
 * Translation goes through a precomputed 256-entry map, so it costs one
 * lookup per byte; deletion scans the set, which is short in practice.
 */
static int cu__tr_chunk(const char *b, unsigned n, void *v) {
    struct trctx *t = v;
    for (unsigned i = 0; i < n; i++) {
        unsigned char c = (unsigned char)b[i];
        if (t->del) {
            int drop = 0;
            for (int k = 0; k < t->n1; k++) if (t->set1[k] == c) { drop = 1; break; }
            if (!drop) putchar_((char)c);
        } else {
            putchar_((char)t->map[c]);
        }
    }
    return 0;
}
/**
 * @brief tr(1): translate or delete bytes of a file.
 *
 * `tr SET1 SET2 FILE` translates, `tr -d SET1 FILE` deletes. A SET2 shorter
 * than SET1 is padded with its own last character, as tr(1) does. Squeezing
 * (-s) and complementing (-c) are not implemented.
 */
static void cmd_tr(int argc, char **argv) {
    struct trctx t;
    memset(&t, 0, sizeof t);
    int ai = 1;
    if (ai < argc && s_eq(argv[ai], "-d")) { t.del = 1; ai++; }
    if (argc - ai < (t.del ? 2 : 3)) {
        printf("usage: tr [-d] SET1 [SET2] FILE\n");
        return;
    }
    t.n1 = tr_expand(argv[ai], t.set1);
    unsigned char set2[256];
    int n2 = 0;
    const char *fname;
    if (t.del) {
        fname = argv[ai + 1];
    } else {
        n2 = tr_expand(argv[ai + 1], set2);
        fname = argv[ai + 2];
        for (int c = 0; c < 256; c++) t.map[c] = (unsigned char)c;
        for (int k = 0; k < t.n1; k++)
            t.map[t.set1[k]] = n2 ? set2[k < n2 ? k : n2 - 1] : t.set1[k];
    }
    file *f = cu_fopen(fname);
    if (!f) return;
    cu_chunks(f, cu__tr_chunk, &t);
    vfs_file_close(f);
}

/* sort ------------------------------------------------------------------- */
/**
 * @brief Compare two lines for `sort`.
 * @param a       First line.
 * @param b       Second line.
 * @param numeric Non-zero to compare leading integers instead of bytes.
 * @return Negative, zero or positive like strcmp.
 *
 * Numeric mode uses @ref s_num, so a line with no leading number sorts as 0.
 */
static int sort_cmp(const char *a, const char *b, int numeric) {
    if (numeric) {
        long x = s_num(a), y = s_num(b);
        if (x < y) return -1;
        if (x > y) return 1;
        return 0;
    }
    return strcmp((char *)a, (char *)b);
}
/**
 * @brief sort(1): sort the lines of the named files together.
 *
 * Accepts bundled -r (reverse), -n (numeric) and -u (drop adjacent
 * duplicates). Several files are merged into one pool and sorted as a whole.
 * The sort itself is an O(n^2) selection pass, which is fine for a pool
 * capped at @ref CU_POOL_LINES lines, and it says so when input was truncated.
 */
static void cmd_sort(int argc, char **argv) {
    int rev = 0, num = 0, uniq = 0, files = 0, reset = 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            for (const char *o = argv[i] + 1; *o; o++) {
                if (*o == 'r') rev = 1;
                else if (*o == 'n') num = 1;
                else if (*o == 'u') uniq = 1;
            }
        } else {
            if (cu_slurp(argv[i], reset) == 0) reset = 0;
            files++;
        }
    }
    if (!files) { printf("usage: sort [-rnu] FILE...\n"); return; }
    for (int i = 0; i < cu_pool_n; i++)
        for (int j = i + 1; j < cu_pool_n; j++) {
            int c = sort_cmp(cu_pool[i], cu_pool[j], num);
            if (rev) c = -c;
            if (c > 0) {
                char tmp[CU_POOL_W];
                s_cpy(tmp, cu_pool[i], CU_POOL_W);
                s_cpy(cu_pool[i], cu_pool[j], CU_POOL_W);
                s_cpy(cu_pool[j], tmp, CU_POOL_W);
            }
        }
    for (int i = 0; i < cu_pool_n; i++) {
        if (uniq && i && s_eq(cu_pool[i], cu_pool[i - 1])) continue;
        o_str(cu_pool[i]); o_nl();
    }
    if (cu_pool_over) printf("sort: input truncated to %d lines\n", CU_POOL_LINES);
}

/**
 * @brief uniq(1): collapse runs of identical adjacent lines.
 *
 * Accepts -c (prefix each run with its length), -d (only repeated runs) and
 * -u (only runs of one). Like uniq(1) it compares adjacent lines only, so
 * unsorted input keeps its scattered duplicates.
 */
static void cmd_uniq(int argc, char **argv) {
    int count = 0, only_dup = 0, only_uniq = 0;
    const char *fname = 0;
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-c")) count = 1;
        else if (s_eq(argv[i], "-d")) only_dup = 1;
        else if (s_eq(argv[i], "-u")) only_uniq = 1;
        else fname = argv[i];
    }
    if (!fname) { printf("usage: uniq [-cdu] FILE\n"); return; }
    if (cu_slurp(fname, 1) < 0) return;
    int i = 0;
    while (i < cu_pool_n) {
        int j = i + 1;
        while (j < cu_pool_n && s_eq(cu_pool[j], cu_pool[i])) j++;
        unsigned run = (unsigned)(j - i);
        int show = 1;
        if (only_dup && run < 2) show = 0;
        if (only_uniq && run > 1) show = 0;
        if (show) {
            if (count) printf("%7u ", run);
            o_str(cu_pool[i]); o_nl();
        }
        i = j;
    }
}

/* strings --------------------------------------------------------------- */
struct strctx { int min; char cur[256]; int n; };
/**
 * @brief @ref cu_chunks callback for `strings`.
 *
 * Accumulates printable ASCII and flushes on the first non-printable byte.
 * A run crossing a 512-byte chunk boundary is carried in the context, so it
 * is reported whole; the run in progress at end-of-file is flushed by the
 * caller.
 */
static int cu__str_chunk(const char *b, unsigned n, void *v) {
    struct strctx *s = v;
    for (unsigned i = 0; i < n; i++) {
        unsigned char c = (unsigned char)b[i];
        if (c >= 0x20 && c < 0x7f) {
            if (s->n < (int)sizeof s->cur - 1) s->cur[s->n++] = (char)c;
        } else {
            if (s->n >= s->min) { s->cur[s->n] = 0; o_str(s->cur); o_nl(); }
            s->n = 0;
        }
    }
    return 0;
}
/**
 * @brief strings(1): print runs of printable ASCII in a file.
 *
 * The minimum run length comes from "-n N" or "-N" and defaults to 4. Runs
 * longer than 255 characters are truncated to that.
 */
static void cmd_strings(int argc, char **argv) {
    int min = 4, files = 0;
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-n") && i + 1 < argc) { min = (int)s_num(argv[++i]); continue; }
        if (argv[i][0] == '-' && is_digit(argv[i][1])) { min = (int)s_num(argv[i] + 1); continue; }
        if (argv[i][0] == '-') continue;
        files++;
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        struct strctx s = { min, {0}, 0 };
        cu_chunks(f, cu__str_chunk, &s);
        if (s.n >= s.min) { s.cur[s.n] = 0; o_str(s.cur); o_nl(); }
        vfs_file_close(f);
    }
    if (!files) printf("usage: strings [-n N] FILE...\n");
}

/* hexdump / xxd / od --------------------------------------------------- */
struct hexctx { unsigned off; };
/**
 * @brief @ref cu_chunks callback for `hexdump`: emit 16-byte rows.
 *
 * The running offset lives in the context so rows stay numbered across
 * chunks. Because chunks are 512 bytes, a short final chunk is the only one
 * that can produce a partial row.
 */
static int cu__hex_chunk(const char *b, unsigned n, void *v) {
    struct hexctx *h = v;
    for (unsigned i = 0; i < n; i += 16) {
        printf("%08x  ", h->off + i);
        unsigned row = n - i < 16 ? n - i : 16;
        for (unsigned j = 0; j < 16; j++) {
            if (j < row) printf("%02x ", (unsigned char)b[i + j]);
            else o_str("   ");
            if (j == 7) putchar_(' ');
        }
        o_str(" |");
        for (unsigned j = 0; j < row; j++) {
            unsigned char c = (unsigned char)b[i + j];
            putchar_((c >= 0x20 && c < 0x7f) ? (char)c : '.');
        }
        o_str("|\n");
    }
    h->off += n;
    return 0;
}
/**
 * @brief hexdump(1) -C: canonical hex + ASCII dump. Also reached as xxd/od/hd.
 *
 * The output format is fixed; no option selects a different one. Repeated
 * identical rows are not collapsed the way hexdump(1) does with '*'.
 */
static void cmd_hexdump(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s FILE...\n", argv[0]); return; }
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        struct hexctx h = { 0 };
        cu_chunks(f, cu__hex_chunk, &h);
        printf("%08x\n", h.off);
        vfs_file_close(f);
    }
}

/**
 * @brief cmp(1): report the first byte at which two files differ.
 *
 * Reads both in lockstep, 512 bytes at a time, and stops at the first
 * difference. If the shared prefix matches but the lengths differ, it reports
 * which file ended early. There is no exit status for the caller to read, so
 * the answer is the printed line.
 */
static void cmd_cmp(int argc, char **argv) {
    if (argc < 3) { printf("usage: cmp FILE1 FILE2\n"); return; }
    file *a = cu_fopen(argv[1]);
    if (!a) return;
    file *b = cu_fopen(argv[2]);
    if (!b) { vfs_file_close(a); return; }

    unsigned la = a->len, lb = b->len;
    unsigned common = la < lb ? la : lb;
    unsigned off = 0, line = 1;
    int diff = 0;
    char ba[512], bb[512];
    while (off < common && !diff) {
        memset(ba, 0, sizeof ba);
        memset(bb, 0, sizeof bb);
        if (a->eof != 1) vfs_file_read(a, ba);
        if (b->eof != 1) vfs_file_read(b, bb);
        unsigned n = common - off;
        if (n > 512) n = 512;
        for (unsigned i = 0; i < n; i++) {
            if (ba[i] != bb[i]) {
                printf("%s %s differ: byte %u, line %u\n",
                       argv[1], argv[2], off + i + 1, line);
                diff = 1;
                break;
            }
            if (ba[i] == '\n') line++;
        }
        off += n;
    }
    if (!diff && la != lb)
        printf("cmp: EOF on %s after byte %u\n", la < lb ? argv[1] : argv[2], common);
    vfs_file_close(a);
    vfs_file_close(b);
}

/* ------------------------------------------------------------- checksums -- */

/* CRC-32 (IEEE, reflected) for cksum-style output. */
static uint32_t crc32_tab[256];
static int      crc32_ready;
/**
 * @brief Build the reflected CRC-32 (IEEE, polynomial 0xEDB88320) table.
 *
 * Called on the first `cksum`; the table then stays in BSS for later runs.
 */
static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc32_tab[i] = c;
    }
    crc32_ready = 1;
}
struct crcctx { uint32_t crc; unsigned len; };
/** @brief @ref cu_chunks callback for `cksum`: fold bytes into the CRC. */
static int cu__crc_chunk(const char *b, unsigned n, void *v) {
    struct crcctx *x = v;
    for (unsigned i = 0; i < n; i++)
        x->crc = crc32_tab[(x->crc ^ (unsigned char)b[i]) & 0xff] ^ (x->crc >> 8);
    x->len += n;
    return 0;
}
/**
 * @brief cksum-style checksum: print "CRC LENGTH NAME" per file.
 *
 * The CRC is reflected CRC-32 over the file bytes only. POSIX cksum uses a
 * different, unreflected polynomial and also folds in the length, so these
 * numbers do not match GNU coreutils' cksum; they match `crc32`.
 */
static void cmd_cksum(int argc, char **argv) {
    if (!crc32_ready) crc32_init();
    if (argc < 2) { printf("usage: cksum FILE...\n"); return; }
    for (int i = 1; i < argc; i++) {
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        struct crcctx x = { 0xffffffffu, 0 };
        cu_chunks(f, cu__crc_chunk, &x);
        vfs_file_close(f);
        printf("%u %u %s\n", x.crc ^ 0xffffffffu, x.len, argv[i]);
    }
}

/* MD5 (RFC 1321), incremental. */
struct md5 { uint32_t a, b, c, d; uint64_t len; uint8_t buf[64]; unsigned n; };
/** @brief Rotate @p x left by @p c bits. @p c is always 1..31 here. */
static uint32_t md5_rol(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }
/**
 * @brief Mix one 64-byte block into the MD5 state (RFC 1321 section 3.4).
 * @param m State to update.
 * @param p Exactly 64 bytes.
 */
static void md5_block(struct md5 *m, const uint8_t *p) {
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 };
    static const int S[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21 };
    uint32_t M[16];
    for (int i = 0; i < 16; i++)
        M[i] = (uint32_t)p[i * 4] | ((uint32_t)p[i * 4 + 1] << 8) |
               ((uint32_t)p[i * 4 + 2] << 16) | ((uint32_t)p[i * 4 + 3] << 24);
    uint32_t A = m->a, B = m->b, C = m->c, D = m->d;
    for (int i = 0; i < 64; i++) {
        uint32_t F;
        int g;
        if (i < 16)      { F = (B & C) | (~B & D);        g = i; }
        else if (i < 32) { F = (D & B) | (~D & C);        g = (5 * i + 1) & 15; }
        else if (i < 48) { F = B ^ C ^ D;                 g = (3 * i + 5) & 15; }
        else             { F = C ^ (B | ~D);              g = (7 * i) & 15; }
        F += A + K[i] + M[g];
        A = D; D = C; C = B;
        B += md5_rol(F, S[i]);
    }
    m->a += A; m->b += B; m->c += C; m->d += D;
}
/** @brief Set the MD5 state to the RFC 1321 initial chaining values. */
static void md5_init(struct md5 *m) {
    m->a = 0x67452301; m->b = 0xefcdab89; m->c = 0x98badcfe; m->d = 0x10325476;
    m->len = 0; m->n = 0;
}
/** @brief @ref cu_chunks callback for `md5sum`: absorb bytes, 64 at a time. */
static int cu__md5_chunk(const char *b, unsigned n, void *v) {
    struct md5 *m = v;
    m->len += n;
    for (unsigned i = 0; i < n; i++) {
        m->buf[m->n++] = (uint8_t)b[i];
        if (m->n == 64) { md5_block(m, m->buf); m->n = 0; }
    }
    return 0;
}
/**
 * @brief Pad the MD5 state and write the little-endian digest.
 * @param m   State; unusable afterwards without a fresh @ref md5_init.
 * @param out Receives 16 bytes.
 */
static void md5_final(struct md5 *m, uint8_t out[16]) {
    uint64_t bits = m->len * 8;
    m->buf[m->n++] = 0x80;
    if (m->n > 56) { while (m->n < 64) m->buf[m->n++] = 0; md5_block(m, m->buf); m->n = 0; }
    while (m->n < 56) m->buf[m->n++] = 0;
    for (int i = 0; i < 8; i++) m->buf[56 + i] = (uint8_t)(bits >> (8 * i));
    md5_block(m, m->buf);
    uint32_t v[4] = { m->a, m->b, m->c, m->d };
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) out[i * 4 + j] = (uint8_t)(v[i] >> (8 * j));
}
/** @brief md5sum(1): print the MD5 digest and name of each file. */
static void cmd_md5sum(int argc, char **argv) {
    if (argc < 2) { printf("usage: md5sum FILE...\n"); return; }
    for (int i = 1; i < argc; i++) {
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        struct md5 m;
        md5_init(&m);
        cu_chunks(f, cu__md5_chunk, &m);
        vfs_file_close(f);
        uint8_t d[16];
        md5_final(&m, d);
        for (int k = 0; k < 16; k++) printf("%02x", d[k]);
        printf("  %s\n", argv[i]);
    }
}

struct shactx { sha256_ctx c; };
/** @brief @ref cu_chunks callback for `sha256sum`, feeding sha2.c. */
static int cu__sha_chunk(const char *b, unsigned n, void *v) {
    struct shactx *s = v;
    sha256_update(&s->c, b, n);
    return 0;
}
/** @brief sha256sum(1): print the SHA-256 digest and name of each file. */
static void cmd_sha256sum(int argc, char **argv) {
    if (argc < 2) { printf("usage: sha256sum FILE...\n"); return; }
    for (int i = 1; i < argc; i++) {
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        struct shactx s;
        sha256_init(&s.c);
        cu_chunks(f, cu__sha_chunk, &s);
        vfs_file_close(f);
        uint8_t d[32];
        sha256_final(&s.c, d);
        for (int k = 0; k < 32; k++) printf("%02x", d[k]);
        printf("  %s\n", argv[i]);
    }
}

/* base64 --------------------------------------------------------------- */
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/**
 * @brief Map one base64 character to its 6-bit value.
 * @return 0-63, or -1 for any other byte, including '=' and whitespace.
 */
static int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
/**
 * @brief base64(1): encode, or with -d/--decode decode, a whole file.
 *
 * Needs the file in RAM (@ref cu_slurp_heap), so it is bounded by the kernel
 * heap. Encoding wraps at 76 columns; decoding ignores every character
 * outside the alphabet, so wrapped input and stray padding are fine.
 */
static void cmd_base64(int argc, char **argv) {
    int decode = 0;
    const char *fname = 0;
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-d") || s_eq(argv[i], "--decode")) decode = 1;
        else fname = argv[i];
    }
    if (!fname) { printf("usage: base64 [-d] FILE\n"); return; }
    if (cu_slurp_heap(fname) < 0) return;
    if (!decode) {
        unsigned col = 0;
        for (unsigned i = 0; i < cu_io_len; i += 3) {
            unsigned b0 = (unsigned char)cu_io[i];
            unsigned b1 = i + 1 < cu_io_len ? (unsigned char)cu_io[i + 1] : 0;
            unsigned b2 = i + 2 < cu_io_len ? (unsigned char)cu_io[i + 2] : 0;
            putchar_(B64[b0 >> 2]);
            putchar_(B64[((b0 & 3) << 4) | (b1 >> 4)]);
            putchar_(i + 1 < cu_io_len ? B64[((b1 & 15) << 2) | (b2 >> 6)] : '=');
            putchar_(i + 2 < cu_io_len ? B64[b2 & 63] : '=');
            if ((col += 4) >= 76) { o_nl(); col = 0; }
        }
        if (col) o_nl();
    } else {
        int acc = 0, bits = 0;
        for (unsigned i = 0; i < cu_io_len; i++) {
            int v = b64val(cu_io[i]);
            if (v < 0) continue;
            acc = (acc << 6) | v;
            bits += 6;
            if (bits >= 8) { bits -= 8; putchar_((char)((acc >> bits) & 0xff)); }
        }
    }
    cu_free_heap();
}

/* -------------------------------------------------------- file management -- */

/**
 * @brief cp(1): copy SRC to DST.
 *
 * Reads the whole source into the kernel heap and writes it in one
 * @ref vfs_spit, because the FAT driver has no partial-write path worth
 * streaming into. Only the two-operand form exists — there is no directory
 * destination, since the FAT layer is root-directory-only.
 */
static void cmd_cp(int argc, char **argv) {
    if (argc < 3) { printf("usage: cp SRC DST\n"); return; }
    char dst[96];
    if (!console_resolve_path(dst, sizeof dst, argv[2])) { printf("cp: %s: path too long\n", argv[2]); return; }
    if (cu_slurp_heap(argv[1]) < 0) return;
    if (vfs_spit(dst, cu_io, cu_io_len) < 0)
        printf("cp: cannot write %s\n", argv[2]);
    cu_free_heap();
}

/**
 * @brief mv(1): move SRC to DST, as a copy followed by a delete.
 *
 * The FAT driver has no rename, so this is not atomic: the destination is
 * written first and the source removed only if that succeeded. A failed
 * delete leaves both copies and says so rather than losing the file.
 */
static void cmd_mv(int argc, char **argv) {
    if (argc < 3) { printf("usage: mv SRC DST\n"); return; }
    char dst[96], src[96];
    if (!console_resolve_path(dst, sizeof dst, argv[2])) { printf("mv: %s: path too long\n", argv[2]); return; }
    if (!console_resolve_path(src, sizeof src, argv[1])) { printf("mv: %s: path too long\n", argv[1]); return; }
    if (cu_slurp_heap(argv[1]) < 0) return;
    int ok = vfs_spit(dst, cu_io, cu_io_len) >= 0;
    cu_free_heap();
    if (!ok) { printf("mv: cannot write %s\n", argv[2]); return; }
    if (!vfs_delete(src)) printf("mv: warning: could not remove %s\n", argv[1]);
}

/**
 * @brief basename(1): strip the directory, and optionally a suffix.
 *
 * Prints "/" for a path that is nothing but slashes. Unlike basename(1),
 * trailing slashes are not stripped first, so "a/b/" yields an empty
 * component and therefore "/".
 */
static void cmd_basename(int argc, char **argv) {
    if (argc < 2) { printf("usage: basename NAME [SUFFIX]\n"); return; }
    const char *p = argv[1];
    const char *slash = p;
    for (const char *q = p; *q; q++) if (*q == '/') slash = q + 1;
    char out[128];
    s_cpy(out, slash, sizeof out);
    if (argc > 2) {
        int ol = s_len(out), sl = s_len(argv[2]);
        if (sl < ol && s_eq(out + ol - sl, argv[2])) out[ol - sl] = 0;
    }
    printf("%s\n", out[0] ? out : "/");
}

/**
 * @brief dirname(1): strip the last path component.
 *
 * Trailing slashes are removed first, then the component, then any slashes
 * that separated it. A path with no slash left gives ".".
 */
static void cmd_dirname(int argc, char **argv) {
    if (argc < 2) { printf("usage: dirname NAME\n"); return; }
    char out[128];
    s_cpy(out, argv[1], sizeof out);
    int i = s_len(out);
    while (i > 1 && out[i - 1] == '/') out[--i] = 0;
    while (i > 0 && out[i - 1] != '/') out[--i] = 0;
    while (i > 1 && out[i - 1] == '/') out[--i] = 0;
    printf("%s\n", out[0] ? out : ".");
}

struct dusz { unsigned bytes; };
/** @brief @ref cu_chunks callback for `du`: add the chunk's length. */
static int cu__du_chunk(const char *b, unsigned n, void *v) { (void)b; ((struct dusz *)v)->bytes += n; return 0; }
/**
 * @brief du(1): report the size of each file.
 *
 * Sizes are rounded up to whole KiB, or with -h scaled to K/M with one
 * decimal. This takes files only: there is no directory recursion, the FAT
 * layer having no subdirectories to recurse into.
 */
static void cmd_du(int argc, char **argv) {
    int human = 0, files = 0;
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-h")) { human = 1; continue; }
        if (argv[i][0] == '-') continue;
        files++;
        file *f = cu_fopen(argv[i]);
        if (!f) continue;
        struct dusz d = { 0 };
        cu_chunks(f, cu__du_chunk, &d);
        vfs_file_close(f);
        if (human) {
            if (d.bytes >= 1024 * 1024) printf("%u.%uM\t%s\n", d.bytes >> 20, ((d.bytes >> 10) % 1024) * 10 / 1024, argv[i]);
            else if (d.bytes >= 1024)   printf("%u.%uK\t%s\n", d.bytes >> 10, (d.bytes % 1024) * 10 / 1024, argv[i]);
            else                        printf("%u\t%s\n", d.bytes, argv[i]);
        } else {
            printf("%u\t%s\n", (d.bytes + 1023) / 1024, argv[i]);
        }
    }
    if (!files) printf("usage: du [-h] FILE...\n");
}

/* =========================================================================
 *  system / misc
 * ========================================================================= */

/** @brief pwd(1): print the console's working directory. */
static void cmd_pwd(int argc, char **argv) { (void)argc; (void)argv; printf("%s\n", console_cwd()); }

/**
 * @brief Decode one character, resolving a backslash escape if there is one.
 * @param s Cursor; advanced past whatever was consumed.
 * @return The decoded character.
 *
 * Understands `\n` `\t` `\r` `\a` `\b` `\f` `\v` `\0` and `\\`. An unknown
 * escape is left alone: the cursor is rewound and a literal backslash
 * returned, so `\q` prints as typed. Shared by `echo -e` and `printf`.
 */
static char cu_unescape(const char **s) {
    char c = *(*s)++;
    if (c != '\\' || !**s) return c;
    char e = *(*s)++;
    switch (e) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case 'a': return 7;
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        case '0': return 0;
        case '\\': return '\\';
        default: (*s)--; return '\\';
    }
}

/**
 * @brief echo(1): print the arguments separated by single spaces.
 *
 * Accepts -n (no trailing newline), -e and -E (escape processing on/off).
 * With -e, `\c` stops output immediately, as in echo(1). The first argument
 * that is not a recognised option ends option parsing, so "echo -x" prints
 * "-x".
 */
static void cmd_echo(int argc, char **argv) {
    int nl = 1, esc = 0, start = 1;
    while (start < argc && argv[start][0] == '-' && argv[start][1]) {
        int ok = 1;
        for (const char *o = argv[start] + 1; *o; o++) {
            if (*o == 'n') nl = 0;
            else if (*o == 'e') esc = 1;
            else if (*o == 'E') esc = 0;
            else ok = 0;
        }
        if (!ok) break;
        start++;
    }
    for (int i = start; i < argc; i++) {
        if (i > start) putchar_(' ');
        if (!esc) { o_str(argv[i]); continue; }
        const char *p = argv[i];
        while (*p) {
            if (*p == '\\' && p[1] == 'c') return;
            putchar_(cu_unescape(&p));
        }
    }
    if (nl) o_nl();
}

/**
 * @brief printf(1): render the arguments through a format string.
 *
 * Understands `%s`, `%d`/`%i`, `%u`, `%x`, `%X`, `%o`, `%c` and `%%`, plus the
 * backslash escapes @ref cu_unescape handles. Field widths and precisions are
 * parsed off and discarded rather than applied, and an unrecognised conversion
 * is echoed as typed. A conversion with no argument left consumes the empty
 * string, and the format is re-applied while arguments remain, like the real
 * utility.
 */
static void cmd_printf(int argc, char **argv) {
    if (argc < 2) { printf("usage: printf FORMAT [ARG...]\n"); return; }
    int ai = 2;
    do {
        const char *f = argv[1];
        while (*f) {
            if (*f == '\\') { putchar_(cu_unescape(&f)); continue; }
            if (*f != '%') { putchar_(*f++); continue; }
            f++;
            if (*f == '%' || *f == 0) { putchar_('%'); if (*f) f++; continue; }
            /* skip any flags/width/precision we don't implement */
            while (*f && !((*f >= 'a' && *f <= 'z') || (*f >= 'A' && *f <= 'Z'))) f++;
            char conv = *f ? *f++ : 's';
            const char *arg = ai < argc ? argv[ai++] : "";
            switch (conv) {
                case 's': printf("%s", arg); break;
                case 'd': case 'i': printf("%ld", s_num(arg)); break;
                case 'u': printf("%lu", (unsigned long)s_num(arg)); break;
                case 'x': printf("%lx", (unsigned long)s_num(arg)); break;
                case 'X': printf("%lX", (unsigned long)s_num(arg)); break;
                case 'o': printf("%lo", (unsigned long)s_num(arg)); break;
                case 'c': putchar_(arg[0]); break;
                default:  putchar_('%'); putchar_(conv); break;
            }
        }
    } while (ai < argc);
}

/** @brief true(1). The console has no exit status, so this does nothing. */
static void cmd_true(int argc, char **argv)  { (void)argc; (void)argv; }
/** @brief false(1). Indistinguishable from @ref cmd_true without a status. */
static void cmd_false(int argc, char **argv) { (void)argc; (void)argv; }

/**
 * @brief seq(1): print a sequence of integers.
 *
 * Takes LAST, FIRST LAST, or FIRST INCR LAST. Integers only — there is no
 * floating point in the kernel. A zero increment is refused, and the run is
 * capped at 100000 lines so a typo cannot wedge the console.
 */
static void cmd_seq(int argc, char **argv) {
    long first = 1, incr = 1, last;
    if (argc == 2)      { last = s_num(argv[1]); }
    else if (argc == 3) { first = s_num(argv[1]); last = s_num(argv[2]); }
    else if (argc >= 4) { first = s_num(argv[1]); incr = s_num(argv[2]); last = s_num(argv[3]); }
    else { printf("usage: seq [FIRST [INCR]] LAST\n"); return; }
    if (incr == 0) { printf("seq: increment must not be zero\n"); return; }
    long guard = 0;
    for (long v = first; incr > 0 ? v <= last : v >= last; v += incr) {
        printf("%ld\n", v);
        if (++guard > 100000) { printf("seq: stopped at 100000 lines\n"); break; }
    }
}

/**
 * @brief Yield the CPU until @p ms milliseconds of PIT time have passed.
 *
 * Yields rather than spinning, so other threads keep running; the wait is
 * therefore at least @p ms, not exactly.
 */
static void cu_delay_ms(unsigned ms) {
    unsigned start = pit_ms();
    while (pit_ms() - start < ms) sched_yield();
}

/**
 * @brief sleep(1): wait for a number of seconds.
 *
 * A fractional part is accepted and parsed to millisecond resolution
 * ("sleep 0.25"); suffixes such as "1m" are not.
 */
static void cmd_sleep(int argc, char **argv) {
    if (argc < 2) { printf("usage: sleep SECONDS\n"); return; }
    const char *s = argv[1];
    unsigned secs = 0;
    while (is_digit(*s)) secs = secs * 10 + (*s++ - '0');
    unsigned ms = secs * 1000;
    if (*s == '.') {
        s++;
        unsigned mul = 100;
        while (is_digit(*s) && mul) { ms += (*s++ - '0') * mul; mul /= 10; }
    }
    cu_delay_ms(ms);
}

/**
 * @brief usleep(1): wait for a number of microseconds.
 *
 * The PIT gives millisecond resolution, so anything under 1000 microseconds
 * waits one millisecond.
 */
static void cmd_usleep(int argc, char **argv) {
    if (argc < 2) { printf("usage: usleep MICROSECONDS\n"); return; }
    unsigned us = (unsigned)s_num(argv[1]);
    cu_delay_ms(us < 1000 ? 1 : us / 1000);
}

/**
 * @brief yes(1): repeat a line ("y" by default) until interrupted.
 *
 * Stops on any keystroke, and in any case after 100000 lines — an SSH or
 * script caller has no keyboard to interrupt with and there is no job
 * control. The keystroke that stopped it is consumed.
 */
static void cmd_yes(int argc, char **argv) {
    char msg[128];
    if (argc > 1) {
        msg[0] = 0;
        for (int i = 1; i < argc; i++) {
            if (i > 1) { int n = s_len(msg); if (n < 126) { msg[n] = ' '; msg[n + 1] = 0; } }
            int n = s_len(msg);
            s_cpy(msg + n, argv[i], (int)sizeof msg - n);
        }
    } else {
        s_cpy(msg, "y", sizeof msg);
    }
    /* Stop on any keystroke; also cap it, since an SSH/exec caller has no
     * keyboard to interrupt with and there is no job control. */
    for (unsigned i = 0; i < 100000u && !keyboard_get_lastkey(); i++) {
        o_str(msg);
        o_nl();
    }
    keyboard_invalidate_lastkey();
}

/**
 * @brief expr(1): evaluate a small arithmetic or string expression.
 *
 * Supports `A OP B` for `+` `-` `*` `/` `%` `<` `<=` `>` `>=` `=` `==` `!=`,
 * plus the `length`, `substr` and `index` forms. A leading backslash on the
 * operator is tolerated so a shell-escaped `\*` works. Division or modulo
 * by zero yields 0 rather than faulting. The result is printed; there is no
 * exit status.
 */
static void cmd_expr(int argc, char **argv) {
    if (argc == 4 && s_eq(argv[1], "index")) {
        int pos = 0;
        for (int i = 0; argv[2][i]; i++)
            if (strchr(argv[3], argv[2][i])) { pos = i + 1; break; }
        printf("%d\n", pos);
        return;
    }
    if (argc == 3 && s_eq(argv[1], "length")) { printf("%d\n", s_len(argv[2])); return; }
    if (argc == 5 && s_eq(argv[1], "substr")) {
        int pos = (int)s_num(argv[3]), len = (int)s_num(argv[4]);
        int sl = s_len(argv[2]);
        if (pos < 1) pos = 1;
        for (int i = pos - 1; i < sl && i < pos - 1 + len; i++) putchar_(argv[2][i]);
        o_nl();
        return;
    }
    if (argc == 4) {
        long a = s_num(argv[1]), b = s_num(argv[3]);
        const char *op = argv[2];
        if (op[0] == '\\' && op[1]) op++;      /* tolerate a shell-escaped \* */
        long r = 0;
        int str = 0;
        if (s_eq(op, "+")) r = a + b;
        else if (s_eq(op, "-")) r = a - b;
        else if (s_eq(op, "*")) r = a * b;
        else if (s_eq(op, "/")) r = b ? a / b : 0;
        else if (s_eq(op, "%")) r = b ? a % b : 0;
        else if (s_eq(op, "<")) r = a < b;
        else if (s_eq(op, "<=")) r = a <= b;
        else if (s_eq(op, ">")) r = a > b;
        else if (s_eq(op, ">=")) r = a >= b;
        else if (s_eq(op, "=") || s_eq(op, "==")) r = s_eq(argv[1], argv[3]);
        else if (s_eq(op, "!=")) r = !s_eq(argv[1], argv[3]);
        else { str = 1; }
        if (str) printf("expr: unknown operator '%s'\n", op);
        else printf("%ld\n", r);
        return;
    }
    printf("usage: expr A OP B | expr length S | expr substr S P L | expr index S C\n");
}

/**
 * @brief factor(1): print the prime factorisation of each number.
 *
 * Trial division up to the square root, on unsigned long, so a large prime
 * takes a visible moment.
 */
static void cmd_factor(int argc, char **argv) {
    if (argc < 2) { printf("usage: factor NUMBER...\n"); return; }
    for (int i = 1; i < argc; i++) {
        unsigned long n = (unsigned long)s_num(argv[i]);
        printf("%lu:", n);
        unsigned long m = n;
        for (unsigned long d = 2; d * d <= m; d++)
            while (m % d == 0) { printf(" %lu", d); m /= d; }
        if (m > 1) printf(" %lu", m);
        o_nl();
    }
}

/**
 * @brief Day of the week for a Gregorian date, by Sakamoto's method.
 * @param y Year.
 * @param m Month, 1-12.
 * @param d Day of month.
 * @return 0 for Sunday through 6 for Saturday.
 */
static int dow(int y, int m, int d) {
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
/**
 * @brief Read today's date from the RTC.
 * @param y Receives the year.
 * @param m Receives the month, 1-12.
 * @param d Receives the day of month.
 *
 * Parses the fixed-width string @c unix_to_str() renders
 * ("Ddd YYYY-MM-DD HH:MM:SS UTC") by offset, so it depends on that layout.
 */
static void cur_ymd(int *y, int *m, int *d) {
    char buf[40];
    unix_to_str(rtc_now_unix(), buf, sizeof buf);   /* "Ddd YYYY-MM-DD HH:MM:SS UTC" */
    *y = (int)s_num(buf + 4);
    *m = (int)s_num(buf + 9);
    *d = (int)s_num(buf + 12);
}
/**
 * @brief cal(1): print a month or a whole year.
 *
 * With no argument, the current month; with one, that whole year; with two,
 * that month of that year. Leap years follow the Gregorian rule. The months
 * of a year print one under the other rather than three across.
 */
static void cmd_cal(int argc, char **argv) {
    int y, m, d;
    cur_ymd(&y, &m, &d);
    if (argc == 2) { m = 0; y = (int)s_num(argv[1]); }
    else if (argc >= 3) { m = (int)s_num(argv[1]); y = (int)s_num(argv[2]); }
    static const char *mn[] = { "January", "February", "March", "April", "May", "June",
                                "July", "August", "September", "October", "November", "December" };
    static const int mdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 0 || m > 12) m = 0;
    int m0 = m ? m : 1, m1 = m ? m : 12;
    for (int mm = m0; mm <= m1; mm++) {
        int dim = mdays[mm - 1];
        if (mm == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) dim = 29;
        printf("   %s %d\nSu Mo Tu We Th Fr Sa\n", mn[mm - 1], y);
        int col = dow(y, mm, 1);
        for (int i = 0; i < col; i++) o_str("   ");
        for (int day = 1; day <= dim; day++) {
            printf("%2d ", day);
            if (++col % 7 == 0) o_nl();
        }
        if (col % 7) o_nl();
        if (mm < m1) o_nl();
    }
}

/**
 * @brief uname(1): print system information.
 *
 * Accepts bundled -s -n -r -v -m -o and -a. The release is the kernel version
 * from ver.h and the "version" field is the build timestamp. With no option,
 * -s is assumed.
 */
static void cmd_uname(int argc, char **argv) {
    const char *sys = "os", *node = "os", *mach = "i686";
    char rel[24];
    snprintf(rel, sizeof rel, "%u.%u", os_ver.maj, os_ver.min);
    const char *ver = __DATE__ " " __TIME__;
    int a = 0, S = 0, N = 0, R = 0, M = 0, V = 0, O = 0;
    for (int i = 1; i < argc; i++)
        for (const char *o = argv[i] + (argv[i][0] == '-'); *o; o++)
            switch (*o) {
                case 'a': a = 1; break;
                case 's': S = 1; break;
                case 'n': N = 1; break;
                case 'r': R = 1; break;
                case 'm': M = 1; break;
                case 'v': V = 1; break;
                case 'o': O = 1; break;
            }
    if (a) { printf("%s %s %s %s %s %s\n", sys, node, rel, ver, mach, sys); return; }
    if (!S && !N && !R && !M && !V && !O) S = 1;
    int first = 1;
    #define UF(cond, val) do { if (cond) { printf("%s%s", first ? "" : " ", val); first = 0; } } while (0)
    UF(S, sys); UF(N, node); UF(R, rel); UF(V, ver); UF(M, mach); UF(O, sys);
    #undef UF
    o_nl();
}

static char cu_hostname[32] = "os";
/**
 * @brief hostname(1): print the hostname, or set it when given an argument.
 *
 * The name lives only in this file's static buffer; nothing else in the
 * kernel, the network stack included, reads it.
 */
static void cmd_hostname(int argc, char **argv) {
    if (argc > 1) s_cpy(cu_hostname, argv[1], sizeof cu_hostname);
    else printf("%s\n", cu_hostname);
}
/** @brief arch(1): the architecture is always i686. */
static void cmd_arch(int argc, char **argv)   { (void)argc; (void)argv; printf("i686\n"); }
/** @brief whoami(1). There are no users; everything runs as root. */
static void cmd_whoami(int argc, char **argv) { (void)argc; (void)argv; printf("root\n"); }
/** @brief id(1). Fixed output: there is no credential system. */
static void cmd_id(int argc, char **argv)     { (void)argc; (void)argv; printf("uid=0(root) gid=0(root) groups=0(root)\n"); }
/** @brief groups(1). Fixed output, as with @ref cmd_id. */
static void cmd_groups(int argc, char **argv) { (void)argc; (void)argv; printf("root\n"); }
/** @brief logname(1). Fixed output, as with @ref cmd_id. */
static void cmd_logname(int argc, char **argv){ (void)argc; (void)argv; printf("root\n"); }
/** @brief nproc(1): CPUs the ACPI tables reported, whether or not smp.c
 *         managed to start them. */
static void cmd_nproc(int argc, char **argv)  { (void)argc; (void)argv; printf("%d\n", acpi_cpu_count()); }

/**
 * @brief uptime(1): time since boot as "up [D days, ]HH:MM:SS".
 *
 * Measured from the PIT tick count, so it is wall time since the timer came
 * up rather than since reset. No load averages: nothing tracks them.
 */
static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    unsigned s = pit_ms() / 1000;
    unsigned dd = s / 86400, hh = (s / 3600) % 24, mm = (s / 60) % 60, ss = s % 60;
    printf("up ");
    if (dd) printf("%u day%s, ", dd, dd == 1 ? "" : "s");
    printf("%02u:%02u:%02u\n", hh, mm, ss);
}

/**
 * @brief free(1): physical frames and kernel heap, used and free.
 *
 * Accepts -b, -k (the default) and -m. "Mem:" comes from the PMM frame
 * bitmap in 4 KiB units; "Heap:" is the kmalloc arena, which is a small
 * fraction of it.
 */
static void cmd_free(int argc, char **argv) {
    unsigned div = 1024;
    const char *unit = "KiB";
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-b")) { div = 1; unit = "bytes"; }
        else if (s_eq(argv[i], "-m")) { div = 1024 * 1024; unit = "MiB"; }
        else if (s_eq(argv[i], "-k")) { div = 1024; unit = "KiB"; }
    }
    unsigned total = get_max_blocks() * 4096u;
    unsigned used  = get_used_blocks() * 4096u;
    unsigned hs = (unsigned)get_heap_size(), hu = (unsigned)get_used_heap();
    printf("%-8s %12s %12s %12s\n", unit, "total", "used", "free");
    printf("%-8s %12u %12u %12u\n", "Mem:", total / div, used / div, (total - used) / div);
    printf("%-8s %12u %12u %12u\n", "Heap:", hs / div, hu / div, (hs - hu) / div);
}

/**
 * @brief clear(1) / reset(1): scroll the screen clear with blank lines.
 *
 * No escape sequence and no cursor move: the console is not a terminal
 * emulator, and this works the same over SSH.
 */
static void cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < 40; i++) o_nl();
}

/**
 * @brief env(1) / printenv(1): print a fixed, synthetic environment.
 *
 * There is no environment to inherit, so the table is built here; only PWD
 * and HOSTNAME vary. Invoked as printenv with a name, prints that one value.
 */
static void cmd_env(int argc, char **argv) {
    const char *only = 0;
    int printenv = s_eq(argv[0], "printenv");
    if (printenv && argc > 1) only = argv[1];
    struct { const char *k, *v; } e[] = {
        { "USER", "root" }, { "LOGNAME", "root" }, { "HOME", "/" },
        { "SHELL", "/rd/zsh" }, { "PWD", console_cwd() }, { "TERM", "os" },
        { "PATH", "/rd:/hda:/fda" }, { "HOSTNAME", cu_hostname }, { "OSTYPE", "os" },
    };
    for (unsigned i = 0; i < sizeof e / sizeof e[0]; i++) {
        if (only) { if (s_eq(e[i].k, only)) printf("%s\n", e[i].v); }
        else printf("%s=%s\n", e[i].k, e[i].v);
    }
}

/** @brief sync(1). A no-op: writes reach the driver before the command
 *         returns, so there is nothing buffered to flush. */
static void cmd_sync(int argc, char **argv) { (void)argc; (void)argv; }

/**
 * @brief mount(1): list the mounted volumes, or mount the named device.
 *
 * The device must already be registered by its driver; this only asks the VFS
 * to parse and expose its filesystem.
 */
static void cmd_mount(int argc, char **argv) {
    if (argc < 2) { vfs_ls(); return; }
    vfs_mount(argv[1]);
    printf("mounted %s\n", argv[1]);
}
/**
 * @brief umount(1): drop a device's filesystem from the mount table.
 *
 * Reports success unconditionally — @ref vfs_unmount ignores a name that is
 * not mounted.
 */
static void cmd_umount(int argc, char **argv) {
    if (argc < 2) { printf("usage: umount DEVICE\n"); return; }
    vfs_unmount(argv[1]);
    printf("unmounted %s\n", argv[1]);
}

/**
 * @brief halt(1): stop the machine.
 *
 * Uses the QEMU debug-exit port, so on real hardware it prints the message
 * and returns to the prompt instead of powering off.
 */
static void cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("System halted.\n");
    exit_qemu(0);
}

/**
 * @brief time(1): run a console command and report how long it took.
 *
 * The operands are re-joined into one line and handed to @ref console_exec,
 * so the command can be any console built-in, not only one from this file.
 * Only real time is reported; nothing accounts user and system time.
 */
static void cmd_time(int argc, char **argv) {
    if (argc < 2) { printf("usage: time COMMAND [ARGS...]\n"); return; }
    char sub[256];
    sub[0] = 0;
    for (int i = 1; i < argc; i++) {
        int n = s_len(sub);
        if (i > 1 && n < 254) { sub[n] = ' '; sub[n + 1] = 0; n++; }
        s_cpy(sub + n, argv[i], (int)sizeof sub - n);
    }
    unsigned t0 = pit_ms();
    console_exec(sub);
    unsigned dt = pit_ms() - t0;
    printf("\nreal\t%um%u.%03us\n", dt / 60000, (dt / 1000) % 60, dt % 1000);
}

/**
 * @brief date(1): print the RTC time, optionally through a +FORMAT string.
 *
 * Understands `%s` `%Y` `%m` `%d` `%H` `%M` `%S` and `%%`. The time is UTC,
 * and there is no -s: setting the RTC is not supported.
 */
static void cmd_date(int argc, char **argv) {
    uint32_t now = rtc_now_unix();
    char buf[40];
    unix_to_str(now, buf, sizeof buf);
    const char *fmt = 0;
    for (int i = 1; i < argc; i++)
        if (argv[i][0] == '+') fmt = argv[i] + 1;
    if (!fmt) { printf("%s\n", buf); return; }
    int Y = (int)s_num(buf + 4), M = (int)s_num(buf + 9), D = (int)s_num(buf + 12);
    int hh = (int)s_num(buf + 15), mm = (int)s_num(buf + 18), ss = (int)s_num(buf + 21);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { putchar_(*p); continue; }
        switch (*++p) {
            case 's': printf("%u", now); break;
            case 'Y': printf("%04d", Y); break;
            case 'm': printf("%02d", M); break;
            case 'd': printf("%02d", D); break;
            case 'H': printf("%02d", hh); break;
            case 'M': printf("%02d", mm); break;
            case 'S': printf("%02d", ss); break;
            case '%': putchar_('%'); break;
            case 0: p--; break;
            default: putchar_('%'); putchar_(*p); break;
        }
    }
    o_nl();
}

/* wget ---------------------------------------------------------------------- */
#define WG_MAX (48 * 1024)
static char     wg_host[96];
static char     wg_path[160];
static char     wg_outfile[64];
static uint32_t wg_ip;
static char    *wg_buf;
static unsigned wg_len;

/**
 * @brief The body of `wget`, run on the network stack's own thread.
 * @return 0 on success, -1 after printing the reason.
 *
 * Issues one HTTP/1.0 GET with "Connection: close", drops everything up to
 * the first CRLFCRLF and keeps the body, up to @ref WG_MAX bytes. Its
 * parameters come from the wg_* statics because @c net_exec() takes a
 * no-argument function. Chunked transfer encoding is not decoded, so a
 * server that ignores HTTP/1.0 and chunks anyway yields a file with the
 * chunk framing still in it.
 */
static int wg_task(void) {
    int h = tcp_connect(wg_ip, 80);
    if (h < 0) { printk("wget: connect failed\n"); return -1; }
    char req[400];
    int rn = snprintf(req, sizeof req,
                      "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: os-wget\r\n"
                      "Connection: close\r\n\r\n", wg_path, wg_host);
    if (tcp_send(h, req, rn) != rn) { tcp_close(h); printk("wget: send failed\n"); return -1; }

    wg_len = 0;
    int hdr_done = 0, match = 0;                 /* match: bytes of "\r\n\r\n" seen */
    static const char eoh[4] = { '\r', '\n', '\r', '\n' };
    for (;;) {
        char rb[1024];
        int r = tcp_recv(h, rb, sizeof rb);
        if (r <= 0) break;
        for (int i = 0; i < r; i++) {
            char c = rb[i];
            if (!hdr_done) {
                match = (c == eoh[match]) ? match + 1 : (c == '\r' ? 1 : 0);
                if (match == 4) hdr_done = 1;
                continue;
            }
            if (wg_len < WG_MAX) wg_buf[wg_len++] = c;
        }
        if (wg_len >= WG_MAX) break;
    }
    tcp_close(h);
    if (vfs_spit(wg_outfile, wg_buf, wg_len) < 0) { printk("wget: cannot write %s\n", wg_outfile); return -1; }
    printk("wget: %u bytes -> %s\n", wg_len, wg_outfile);
    return 0;
}

/**
 * @brief wget(1): fetch an HTTP URL into a file.
 *
 * Accepts "-O FILE"; otherwise the name is the last path component, or
 * index.html for a bare host. Parses only http:// URLs, with no port, no user
 * info and no redirect following. Resolves the host, allocates the
 * @ref WG_MAX transfer buffer, then runs @ref wg_task on the network thread.
 */
static void cmd_wget(int argc, char **argv) {
    const char *url = 0, *out = 0;
    for (int i = 1; i < argc; i++) {
        if (s_eq(argv[i], "-O") && i + 1 < argc) out = argv[++i];
        else if (argv[i][0] != '-') url = argv[i];
    }
    if (!url) { printf("usage: wget URL [-O FILE]\n"); return; }
    if (!net_is_up()) { printf("wget: network is down\n"); return; }

    const char *h = url;
    if (s_find(h, "http://", 0) == h) h += 7;
    char hosttok[160];
    s_cpy(hosttok, h, sizeof hosttok);
    char *slash = strchr(hosttok, '/');
    if (slash) { s_cpy(wg_path, slash, sizeof wg_path); *slash = 0; }
    else s_cpy(wg_path, "/", sizeof wg_path);
    s_cpy(wg_host, hosttok, sizeof wg_host);

    if (out) s_cpy(wg_outfile, out, sizeof wg_outfile);
    else {
        const char *base = wg_path;
        for (const char *q = wg_path; *q; q++) if (*q == '/') base = q + 1;
        char nm[64];
        s_cpy(nm, (base && *base) ? base : "index.html", sizeof nm);
        if (!console_resolve_path(wg_outfile, sizeof wg_outfile, nm)) { printf("wget: name too long\n"); return; }
    }

    uint32_t a[2];
    if (dns_resolve(wg_host, a, 1) < 1) { printf("wget: cannot resolve %s\n", wg_host); return; }
    wg_ip = a[0];

    wg_buf = kmalloc(WG_MAX);
    if (!wg_buf) { printf("wget: out of memory\n"); return; }
    net_exec(wg_task);
    kfree(wg_buf);
    wg_buf = 0;
}

/* =========================================================================
 *  dispatch
 * ========================================================================= */

typedef void (*cu_fn)(int argc, char **argv);
struct cu_cmd { const char *name; cu_fn fn; };

static const struct cu_cmd cu_table[] = {
    { "cat", cmd_cat }, { "head", cmd_head }, { "tail", cmd_tail }, { "wc", cmd_wc },
    { "grep", cmd_grep }, { "egrep", cmd_grep }, { "fgrep", cmd_grep },
    { "nl", cmd_nl }, { "tac", cmd_tac }, { "rev", cmd_rev }, { "cut", cmd_cut },
    { "tr", cmd_tr }, { "sort", cmd_sort }, { "uniq", cmd_uniq }, { "strings", cmd_strings },
    { "hexdump", cmd_hexdump }, { "xxd", cmd_hexdump }, { "od", cmd_hexdump }, { "hd", cmd_hexdump },
    { "cmp", cmd_cmp }, { "cksum", cmd_cksum }, { "md5sum", cmd_md5sum }, { "sha256sum", cmd_sha256sum },
    { "base64", cmd_base64 },
    { "cp", cmd_cp }, { "mv", cmd_mv }, { "basename", cmd_basename }, { "dirname", cmd_dirname },
    { "du", cmd_du },
    { "pwd", cmd_pwd }, { "echo", cmd_echo }, { "printf", cmd_printf },
    { "true", cmd_true }, { "false", cmd_false }, { "seq", cmd_seq },
    { "sleep", cmd_sleep }, { "usleep", cmd_usleep }, { "yes", cmd_yes },
    { "expr", cmd_expr }, { "factor", cmd_factor }, { "cal", cmd_cal },
    { "uname", cmd_uname }, { "arch", cmd_arch }, { "hostname", cmd_hostname },
    { "whoami", cmd_whoami }, { "id", cmd_id }, { "groups", cmd_groups }, { "logname", cmd_logname },
    { "nproc", cmd_nproc }, { "uptime", cmd_uptime }, { "free", cmd_free },
    { "clear", cmd_clear }, { "reset", cmd_clear }, { "env", cmd_env }, { "printenv", cmd_env },
    { "sync", cmd_sync }, { "mount", cmd_mount }, { "umount", cmd_umount },
    { "halt", cmd_halt }, { "time", cmd_time }, { "date", cmd_date }, { "wget", cmd_wget },
};

/**
 * @brief Run @p line if its first word names a command in this file.
 * @param line The command line, leading whitespace tolerated.
 * @return 1 if a command matched and ran, 0 if the verb is not ours.
 *
 * @ref console_exec calls this after its own built-ins have had a chance, so
 * a native built-in of the same name wins. A verb longer than 23 characters
 * is truncated before the lookup and therefore cannot match.
 */
int coreutils_try(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    char verb[24];
    int i = 0;
    while (line[i] && line[i] != ' ' && line[i] != '\t' && i < (int)sizeof verb - 1) {
        verb[i] = line[i];
        i++;
    }
    verb[i] = 0;
    if (!verb[0]) return 0;

    for (unsigned k = 0; k < sizeof cu_table / sizeof cu_table[0]; k++) {
        if (s_eq(verb, cu_table[k].name)) {
            int argc = cu_split(line);
            cu_table[k].fn(argc, cu_argv);
            return 1;
        }
    }
    return 0;
}

/** @brief Print the command list for the console's `help`. */
void coreutils_help(void) {
    printf(
        "\nfile/text: cat head tail wc grep nl tac rev cut tr sort uniq strings\n"
        "           hexdump/xxd/od cmp cksum md5sum sha256sum base64 cp mv du\n"
        "           basename dirname\n"
        "system:    pwd echo printf true false seq sleep usleep yes expr factor cal\n"
        "           uname arch hostname whoami id groups logname nproc uptime free\n"
        "           clear env printenv sync mount umount halt time date wget\n");
}
