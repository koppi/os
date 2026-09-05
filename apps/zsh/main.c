/**
 * @file apps/zsh/main.c
 * @brief A minimalistic zsh-ish shell that runs in user space (ring 3).
 *
 * The shell owns the interactive surface — a line editor with history,
 * tab-completion, aliases, `!` history expansion and `*`/`?` globbing — but it
 * does not re-implement the system commands. Every command that isn't a shell
 * builtin is handed to the kernel's commands.c dispatcher through the `run`
 * syscall (#18), so `ls`, `cd`, `pci`, `ping`, `poweroff`, `start <prog>` ...
 * behave exactly as they do on the in-kernel debug console.
 *
 * Syscalls used:
 *    5  exit(code)            12 write(buf,len)      17 getkey()  -> one key
 *    6  fopen(path,mode)      13 fread(f,buf512)     18 run(line)
 *    7  fclose(f)             14 time()              19 getcwd(buf,n)
 *                                                    20 listdir(path,buf,n)
 *
 * Editor keys (control keys only arrive over a serial console; a PS/2 keyboard
 * gives you the printable set, Backspace, Tab and Enter):
 *    Tab            complete command / file name
 *    ^P / ^N        previous / next history entry
 *    ^U / ^W        kill line / kill word
 *    ^L             clear screen
 *    ^C             abandon the line
 *    ^D  (empty)    leave the shell (back to the kernel console)
 */

typedef unsigned int  u32;
typedef unsigned char  u8;

/* --------------------------------------------------------------- syscalls -- */

static u32 sc3(int n, u32 a, u32 b, u32 c) {
    u32 r;
    __asm__ __volatile__("int $0x72"
                         : "=a"(r)
                         : "0"(n), "b"(a), "c"(b), "d"(c)
                         : "memory");
    return r;
}
#define SC0(n)       sc3((n), 0, 0, 0)
#define SC1(n,a)     sc3((n), (u32)(a), 0, 0)
#define SC2(n,a,b)   sc3((n), (u32)(a), (u32)(b), 0)

static void sys_exit(int code)                     { SC1(5, code); }
static void *sys_fopen(const char *p, const char *m){ return (void *)SC2(6, p, m); }
static void sys_fclose(void *f)                    { SC1(7, f); }
static void sys_out(const char *b, int n)          { SC2(12, b, n); }
static void sys_fread(void *f, char *b)            { SC2(13, f, b); }
static int  sys_getkey(void)                       { return (int)(SC0(17) & 0xFF); }
static void sys_run(const char *line)              { SC1(18, line); }
static int  sys_getcwd(char *b, int n)             { return (int)SC2(19, b, n); }
static int  sys_listdir(const char *p, char *b, int n) {
    return (int)sc3(20, (u32)p, (u32)b, (u32)n);
}
static int  sys_spawn(const char *path, const char *args) {
    return (int)SC2(21, path, args);
}

/** Mirrors the kernel `file` handle (lib/stdio.h FILE). */
typedef struct { char name[32]; u32 flags, len, eof, dev, cur, type; } OSFILE;

/* ---------------------------------------------------------- tiny c library -- */

#define LINE  256          /* max command line */
#define HIST  64           /* history depth */
#define ALN   24           /* alias slots */

static int  slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, const char *s) { while ((*d++ = *s++)) ; }
static void sncpy(char *d, const char *s, int n) {
    int i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}
static void scat(char *d, const char *s) { scpy(d + slen(d), s); }
static int  scmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(u8)*a - (int)(u8)*b;
}
static int  sncmp(const char *a, const char *b, int n) {
    while (n > 0 && *a && *a == *b) { a++; b++; n--; }
    return n ? (int)(u8)*a - (int)(u8)*b : 0;
}
static char *schr(const char *s, int c) {
    for (; *s; s++) if (*s == c) return (char *)s;
    return c ? (char *)0 : (char *)s;
}
static char *srchr(const char *s, int c) {
    char *r = (char *)0;
    for (; *s; s++) if (*s == c) r = (char *)s;
    return r;
}
static int  is_space(int c) { return c == ' ' || c == '\t'; }
static const char *skip_ws(const char *s) { while (is_space(*s)) s++; return s; }

/* ------------------------------------------------------------------ output -- */

static void w(const char *s)   { sys_out(s, slen(s)); }
static void wc(char c)         { sys_out(&c, 1); }
static void wu(u32 v) {
    char t[12];
    int i = 0;
    if (!v) { wc('0'); return; }
    while (v) { t[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) wc(t[--i]);
}

/* ------------------------------------------------------------------- state -- */

static char g_prompt[96];
static char g_cwd[64];
static char g_oldcwd[64];
static int  g_done;

static char hist[HIST][LINE];
static int  hist_len;

static char al_name[ALN][24];
static char al_val[ALN][192];
static int  al_n;

static char ld_buf[4096];   /* scratch for listdir output */

static const char *g_builtins[] = {
    "cd", "pwd", "exit", "logout", "quit", "help", "history",
    "alias", "unalias", "echo", "which", "clear", "source", "true", "false", 0
};
/*
 * Command-name hints for tab-completion and `which`. NOT used for dispatch:
 * run_segment() routes anything that is not a shell builtin and not an on-disk
 * program through the `run` syscall, so the kernel console's own dispatcher
 * (native built-ins + the coreutils toolbox in coreutils.c) stays the single
 * source of truth and this list can drift without breaking anything.
 */
static const char *g_verbs[] = {
    "mem", "ps", "cpus", "ls", "read", "touch", "rm", "write", "sum", "beep",
    "pci", "net", "nfs", "ssh", "ping", "dns", "http", "ntpdate", "poweroff",
    "reboot", "start",
    "cat", "cp", "mv", "grep", "egrep", "fgrep", "head", "tail", "wc", "sort",
    "uniq", "cut", "tr", "nl", "tac", "rev", "od", "hexdump", "xxd", "hd",
    "seq", "yes", "factor", "expr", "cal", "date", "sleep", "usleep", "env",
    "printenv", "printf", "id", "whoami", "groups", "logname", "hostname",
    "uname", "arch", "nproc", "free", "du", "uptime", "sync", "mount", "umount",
    "wget", "base64", "md5sum", "sha256sum", "cksum", "cmp", "basename",
    "dirname", "strings", "reset", "halt", 0
};

static int is_verb(const char *c) {
    for (int i = 0; g_verbs[i]; i++)
        if (scmp(g_verbs[i], c) == 0) return 1;
    return 0;
}

/** @return 1 if @p name is a leaf of directory @p dir. */
static int listdir_has(const char *dir, const char *name) {
    if (sys_listdir(dir, ld_buf, sizeof ld_buf) <= 0) return 0;
    char *p = ld_buf;
    while (*p) {
        char *e = schr(p, '\n');
        int L = e ? (int)(e - p) : slen(p);
        if (L > 0 && L < 63) {
            char nm[64];
            sncpy(nm, p, L);
            if (scmp(nm, name) == 0) return 1;
        }
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

/** @return the directory holding program @p cmd (cwd or /hda), or NULL. */
static const char *prog_dir(const char *cmd) {
    if (g_cwd[0] && scmp(g_cwd, "/") != 0 && listdir_has(g_cwd, cmd)) return g_cwd;
    if (listdir_has("/hda", cmd)) return "/hda";
    return 0;
}

static void build_prompt(void) {
    scpy(g_prompt, "os ");
    scat(g_prompt, g_cwd[0] ? g_cwd : "/");
    scat(g_prompt, " % ");
}

/* --------------------------------------------------------------- history --- */

static void hist_add(const char *l) {
    l = skip_ws(l);
    if (!l[0]) return;
    if (hist_len && scmp(hist[hist_len - 1], l) == 0) return;
    if (hist_len == HIST) {
        for (int i = 1; i < HIST; i++) scpy(hist[i - 1], hist[i]);
        hist_len--;
    }
    sncpy(hist[hist_len++], l, LINE - 1);
}

static int digits(const char *s) { int n = 0; while (s[n] >= '0' && s[n] <= '9') n++; return n; }
static int atou(const char *s)   { int v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); return v; }

/** Expand a leading `!` event. Returns 1 if @p in was a bang expression. */
static int expand_bang(const char *in, char *out) {
    if (in[0] != '!' || in[1] == 0) return 0;
    const char *p = in + 1;
    const char *base = 0;
    int consumed;

    if (p[0] == '!') {
        consumed = 1;
        if (hist_len) base = hist[hist_len - 1];
    } else if (p[0] == '-' && digits(p + 1) > 0) {
        int d = digits(p + 1), k = atou(p + 1);
        consumed = 1 + d;
        if (k >= 1 && k <= hist_len) base = hist[hist_len - k];
    } else if (digits(p) > 0) {
        int d = digits(p), k = atou(p);
        consumed = d;
        if (k >= 1 && k <= hist_len) base = hist[k - 1];
    } else {
        int d = 0;
        while (p[d] && !is_space(p[d])) d++;
        consumed = d;
        for (int i = hist_len - 1; i >= 0; i--)
            if (sncmp(hist[i], p, d) == 0) { base = hist[i]; break; }
    }
    if (!base) { w("zsh: no such history event\n"); out[0] = 0; return 1; }
    scpy(out, base);
    scat(out, in + 1 + consumed);
    return 1;
}

/* ---------------------------------------------------------------- aliases -- */

static int alias_find(const char *name) {
    for (int i = 0; i < al_n; i++)
        if (scmp(al_name[i], name) == 0) return i;
    return -1;
}
static void alias_set(const char *name, const char *val) {
    int i = alias_find(name);
    if (i < 0) {
        if (al_n == ALN) { w("alias: table full\n"); return; }
        i = al_n++;
        sncpy(al_name[i], name, 23);
    }
    sncpy(al_val[i], val, 191);
}
static void apply_alias(char *seg) {
    char first[64];
    int i = 0;
    while (seg[i] && !is_space(seg[i]) && i < 63) { first[i] = seg[i]; i++; }
    first[i] = 0;
    int a = alias_find(first);
    if (a < 0) return;
    char tmp[LINE];
    sncpy(tmp, al_val[a], LINE - 1);
    scat(tmp, seg + i);          /* keep the original arguments */
    sncpy(seg, tmp, LINE - 1);
}

static void do_alias(const char *a) {
    a = skip_ws(a);
    if (!a[0]) {
        for (int i = 0; i < al_n; i++) {
            w("alias "); w(al_name[i]); w("='"); w(al_val[i]); w("'\n");
        }
        return;
    }
    char name[24];
    int i = 0;
    while (a[i] && a[i] != '=' && !is_space(a[i]) && i < 23) { name[i] = a[i]; i++; }
    name[i] = 0;
    if (a[i] != '=') {
        int k = alias_find(name);
        if (k < 0) { w("alias: "); w(name); w(": not found\n"); return; }
        w("alias "); w(name); w("='"); w(al_val[k]); w("'\n");
        return;
    }
    const char *v = a + i + 1;
    char val[192];
    int j = 0;
    if (*v == '\'' || *v == '"') {
        char q = *v++;
        while (*v && *v != q && j < 191) val[j++] = *v++;
    } else {
        while (*v && j < 191) val[j++] = *v++;
    }
    val[j] = 0;
    alias_set(name, val);
}

static void do_unalias(const char *a) {
    char name[24];
    int i = 0;
    a = skip_ws(a);
    while (a[i] && !is_space(a[i]) && i < 23) { name[i] = a[i]; i++; }
    name[i] = 0;
    int k = alias_find(name);
    if (k < 0) { w("unalias: "); w(name); w(": not found\n"); return; }
    for (int m = k + 1; m < al_n; m++) {
        scpy(al_name[m - 1], al_name[m]);
        scpy(al_val[m - 1], al_val[m]);
    }
    al_n--;
}

/* --------------------------------------------------------------- globbing -- */

static int fnmatch(const char *p, const char *s) {
    for (;;) {
        if (*p == '*') {
            p++;
            if (!*p) return 1;
            for (; *s; s++) if (fnmatch(p, s)) return 1;
            return fnmatch(p, s);
        }
        if (!*s) return *p == 0;
        if (*p != '?' && *p != *s) return 0;
        p++; s++;
    }
}

static const char *ls_base(void) {
    return (g_cwd[0] && scmp(g_cwd, "/") != 0) ? g_cwd : "/hda";
}

static void glob_expand(char *seg) {
    if (!schr(seg, '*') && !schr(seg, '?')) return;
    if (sys_listdir(ls_base(), ld_buf, sizeof ld_buf) <= 0) return;

    char out[LINE];
    int ol = 0;
    const char *p = seg;
    while (*p) {
        while (*p == ' ') { if (ol < LINE - 1) out[ol++] = ' '; p++; }
        if (!*p) break;
        char tok[128];
        int tl = 0;
        while (*p && *p != ' ' && tl < 127) tok[tl++] = *p++;
        tok[tl] = 0;

        if ((schr(tok, '*') || schr(tok, '?')) && !schr(tok, '/')) {
            int matched = 0;
            char *q = ld_buf;
            while (*q) {
                char *e = schr(q, '\n');
                int L = e ? (int)(e - q) : slen(q);
                if (L > 0 && L < 63) {
                    char nm[64];
                    sncpy(nm, q, L);
                    if (fnmatch(tok, nm)) {
                        if (matched && ol < LINE - 1) out[ol++] = ' ';
                        for (int k = 0; nm[k] && ol < LINE - 1; k++) out[ol++] = nm[k];
                        matched = 1;
                    }
                }
                if (!e) break;
                q = e + 1;
            }
            if (!matched)
                for (int k = 0; tok[k] && ol < LINE - 1; k++) out[ol++] = tok[k];
        } else {
            for (int k = 0; tok[k] && ol < LINE - 1; k++) out[ol++] = tok[k];
        }
    }
    out[ol] = 0;
    sncpy(seg, out, LINE - 1);
}

/* ------------------------------------------------------------------ cd ------ */

/** Descend to absolute path @p abs ("" / "/" == root), one component at a time
 *  (the kernel `cd` only walks a single level per call). */
static void cd_path(const char *abs) {
    sys_run("cd");
    const char *p = abs;
    while (*p == '/') p++;
    while (*p) {
        char comp[40], cmd[48];
        int i = 0;
        while (*p && *p != '/' && i < 39) comp[i++] = *p++;
        comp[i] = 0;
        while (*p == '/') p++;
        if (comp[0]) { scpy(cmd, "cd "); scat(cmd, comp); sys_run(cmd); }
    }
}

static void parent_of(const char *path, char *out) {
    scpy(out, path);
    char *sl = srchr(out + (out[0] == '/' ? 1 : 0), '/');
    if (sl) *sl = 0;
    else out[0] = 0;                 /* single component -> root */
}

static void do_cd(const char *arg) {
    arg = skip_ws(arg);
    char target[64];

    if (!arg[0] || scmp(arg, "~") == 0)        target[0] = 0;         /* root */
    else if (scmp(arg, "-") == 0)              scpy(target, g_oldcwd);
    else if (scmp(arg, ".") == 0)              scpy(target, g_cwd);
    else if (scmp(arg, "..") == 0)             parent_of(g_cwd, target);
    else if (arg[0] == '/')                    sncpy(target, arg, 63);
    else if (arg[0] == '~')                    sncpy(target, arg + 1, 63);
    else {
        const char *base = (g_cwd[0] && scmp(g_cwd, "/") != 0) ? g_cwd : "";
        scpy(target, base);
        scat(target, "/");
        { char t[48]; int i = 0; while (arg[i] && !is_space(arg[i]) && i < 47) { t[i] = arg[i]; i++; } t[i] = 0; scat(target, t); }
    }

    char prev[64];
    scpy(prev, g_cwd);
    cd_path(target);
    sys_getcwd(g_cwd, sizeof g_cwd);
    if (scmp(prev, g_cwd) != 0) scpy(g_oldcwd, prev);
    build_prompt();
}

/* --------------------------------------------------------------- builtins -- */

static void do_echo(const char *a) {
    int nl = 1;
    if (sncmp(a, "-n", 2) == 0 && (a[2] == 0 || is_space(a[2]))) {
        nl = 0;
        a = skip_ws(a + 2);
    }
    w(a);
    if (nl) wc('\n');
}

static void do_history(const char *a) {
    a = skip_ws(a);
    if (sncmp(a, "-c", 2) == 0) { hist_len = 0; return; }
    for (int i = 0; i < hist_len; i++) {
        w("  ");
        wu((u32)(i + 1));
        w("  ");
        w(hist[i]);
        wc('\n');
    }
}

static void do_which(const char *a) {
    char n[64];
    int i = 0;
    a = skip_ws(a);
    while (a[i] && !is_space(a[i]) && i < 63) { n[i] = a[i]; i++; }
    n[i] = 0;
    if (!n[0]) return;

    int k = alias_find(n);
    if (k >= 0) { w(n); w(": aliased to '"); w(al_val[k]); w("'\n"); return; }
    for (int m = 0; g_builtins[m]; m++)
        if (scmp(g_builtins[m], n) == 0) { w(n); w(": shell builtin\n"); return; }

    const char *base = prog_dir(n);
    if (base) { w(n); w(": "); w(base); w("/"); w(n); wc('\n'); return; }

    if (is_verb(n)) { w(n); w(": console command\n"); return; }
    w(n); w(": not found\n");
}

static void do_help(void) {
    w("zsh builtins:\n"
      "  cd [dir|-|~]   pwd   echo [-n]   clear   history [-c]\n"
      "  alias [name=val]   unalias name   which name   source FILE   exit\n"
      "  !!  !n  !prefix history expansion   TAB completion   ^P/^N  ^U/^W/^L\n"
      "\nkernel commands (run in ring 0 via the `run` syscall):\n");
    sys_run("help");
}

/* --------------------------------------------------------- run a file ------ */

static void exec_line(const char *line);   /* fwd */

static void source_path(const char *path) {
    OSFILE *f = (OSFILE *)sys_fopen(path, "r");
    if (!f) return;

    static char fb[4096 + 520];
    u32 total = f->len;
    if (total > 4096) total = 4096;
    u32 got = 0;
    while (!f->eof && got < total) { sys_fread(f, fb + got); got += 512; }
    sys_fclose(f);
    fb[total] = 0;

    char *p = fb;
    while (*p) {
        char *e = schr(p, '\n');
        if (e) *e = 0;
        char *cr = schr(p, '\r');
        if (cr) *cr = 0;
        if (p[0]) { char lb[LINE]; sncpy(lb, p, LINE - 1); exec_line(lb); }
        if (!e) break;
        p = e + 1;
    }
}

static void source_arg(const char *a) {
    char n[80];
    int i = 0;
    a = skip_ws(a);
    while (a[i] && !is_space(a[i]) && i < 79) { n[i] = a[i]; i++; }
    n[i] = 0;
    if (!n[0]) { w("source: filename required\n"); return; }
    if (n[0] == '/') { source_path(n); return; }
    char full[112];
    const char *base = (g_cwd[0] && scmp(g_cwd, "/") != 0) ? g_cwd : "/hda";
    scpy(full, base);
    scat(full, "/");
    scat(full, n);
    source_path(full);
}

/* Prefix the current directory onto bare argument tokens that name a file in
 * it, so `lua t.lua` opens /hda/t.lua the way `cat t.lua` already would. */
static void qualify_args(char *args) {
    const char *base = (g_cwd[0] && scmp(g_cwd, "/") != 0) ? g_cwd : 0;
    if (!args[0] || !base) return;
    if (sys_listdir(base, ld_buf, sizeof ld_buf) <= 0) return;

    char out[LINE];
    int ol = 0;
    const char *p = args;
    while (*p) {
        while (*p == ' ') { if (ol < LINE - 1) out[ol++] = ' '; p++; }
        char tok[96];
        int tl = 0;
        while (*p && *p != ' ' && tl < 95) tok[tl++] = *p++;
        tok[tl] = 0;
        if (!tok[0]) continue;

        int isfile = 0;
        if (tok[0] != '-' && !schr(tok, '/')) {
            char *q = ld_buf;
            while (*q) {
                char *e = schr(q, '\n');
                int L = e ? (int)(e - q) : slen(q);
                if (L > 0 && L < 63) {
                    char nm[64];
                    sncpy(nm, q, L);
                    if (scmp(nm, tok) == 0) { isfile = 1; break; }
                }
                if (!e) break;
                q = e + 1;
            }
        }
        if (isfile) {
            for (const char *b = base; *b && ol < LINE - 1;) out[ol++] = *b++;
            if (ol < LINE - 1) out[ol++] = '/';
        }
        for (int i = 0; i < tl && ol < LINE - 1; i++) out[ol++] = tok[i];
    }
    out[ol] = 0;
    sncpy(args, out, LINE - 1);
}

/* ------------------------------------------------------- command dispatch -- */

static void run_segment(const char *seg_in) {
    char seg[LINE];
    sncpy(seg, skip_ws(seg_in), LINE - 1);
    if (!seg[0] || seg[0] == '#') return;

    if (seg[0] == '!') {
        char ex[LINE];
        if (expand_bang(seg, ex)) {
            if (!ex[0]) return;
            w(ex); wc('\n');
            sncpy(seg, ex, LINE - 1);
        }
    }
    apply_alias(seg);
    glob_expand(seg);

    char cmd[64];
    int i = 0;
    while (seg[i] && !is_space(seg[i]) && i < 63) { cmd[i] = seg[i]; i++; }
    cmd[i] = 0;
    const char *args = skip_ws(seg + i);

    if (!cmd[0]) return;
    if (scmp(cmd, "exit") == 0 || scmp(cmd, "logout") == 0 || scmp(cmd, "quit") == 0) {
        g_done = 1; return;
    }
    if (scmp(cmd, "cd") == 0)       { do_cd(args); return; }
    if (scmp(cmd, "pwd") == 0)      { w(g_cwd[0] ? g_cwd : "/"); wc('\n'); return; }
    if (scmp(cmd, "echo") == 0)     { do_echo(args); return; }
    if (scmp(cmd, "clear") == 0)    { for (int k = 0; k < 40; k++) wc('\n'); return; }
    if (scmp(cmd, "history") == 0)  { do_history(args); return; }
    if (scmp(cmd, "alias") == 0)    { do_alias(args); return; }
    if (scmp(cmd, "unalias") == 0)  { do_unalias(args); return; }
    if (scmp(cmd, "which") == 0)    { do_which(args); return; }
    if (scmp(cmd, "help") == 0)     { do_help(); return; }
    if (scmp(cmd, "source") == 0 || scmp(cmd, ".") == 0) { source_arg(args); return; }
    if (scmp(cmd, ":") == 0) return;

    /*
     * Everything else is either a program on disk or a command for the kernel
     * console dispatcher (native built-ins + the coreutils toolbox). Spawn it
     * only when it is an explicit `start`, a path, or the name of a file in the
     * current directory / on /hda; otherwise hand the whole line to `run`.
     */
    int is_start = (scmp(cmd, "start") == 0);
    const char *rest = is_start ? skip_ws(args) : seg;
    char prog[80];
    int k = 0;
    while (rest[k] && !is_space(rest[k]) && k < 79) { prog[k] = rest[k]; k++; }
    prog[k] = 0;

    if (!is_start && prog[0] != '/' && !prog_dir(prog)) {
        sys_run(seg);
        return;
    }
    if (!prog[0]) { w("start: missing program name\n"); return; }

    char pargs[LINE];
    sncpy(pargs, skip_ws(rest + k), LINE - 1);
    qualify_args(pargs);

    char full[112];
    if (prog[0] == '/') {
        scpy(full, prog);
    } else {
        const char *base = prog_dir(prog);
        if (!base) base = (g_cwd[0] && scmp(g_cwd, "/") != 0) ? g_cwd : "/hda";
        scpy(full, base);
        scat(full, "/");
        scat(full, prog);
    }
    sys_spawn(full, pargs);
}

static void exec_line(const char *line) {
    char work[LINE];
    sncpy(work, line, LINE - 1);
    char *p = work;
    for (;;) {
        char *semi = schr(p, ';');
        if (semi) *semi = 0;
        run_segment(p);
        if (g_done) return;
        if (!semi) break;
        p = semi + 1;
    }
}

/* --------------------------------------------------------- line editor ----- */

static int  g_plen;         /* rendered prompt length */

static void redraw(const char *buf, int len, int *drawn) {
    wc('\r');
    w(g_prompt);
    w(buf);
    int now = g_plen + len;
    if (*drawn > now) {
        int pad = *drawn - now;
        for (int i = 0; i < pad; i++) wc(' ');
        for (int i = 0; i < pad; i++) wc('\b');
    }
    *drawn = now;
}

/** Longest common prefix of @p nc candidate names. */
static int common_prefix(char cand[][64], int nc, char *out) {
    scpy(out, cand[0]);
    for (int i = 1; i < nc; i++) {
        int j = 0;
        while (out[j] && cand[i][j] && out[j] == cand[i][j]) j++;
        out[j] = 0;
    }
    return slen(out);
}

static void complete(char *buf, int *plen, int *drawn) {
    int len = *plen;
    int ts = len;
    while (ts > 0 && buf[ts - 1] != ' ') ts--;
    int is_cmd = (ts == 0);

    char tok[128];
    sncpy(tok, buf + ts, sizeof tok - 1);

    char dir[64], pfx[64];
    char *sl = srchr(tok, '/');
    if (sl) { int dl = (int)(sl - tok); sncpy(dir, tok, dl < 63 ? dl : 63); scpy(pfx, sl + 1); }
    else    { dir[0] = 0; scpy(pfx, tok); }
    int pl = slen(pfx);

    static char cand[64][64];
    int nc = 0;

    if (is_cmd && !sl) {
        for (int i = 0; g_builtins[i] && nc < 64; i++)
            if (sncmp(g_builtins[i], pfx, pl) == 0) scpy(cand[nc++], g_builtins[i]);
        for (int i = 0; g_verbs[i] && nc < 64; i++)
            if (sncmp(g_verbs[i], pfx, pl) == 0) scpy(cand[nc++], g_verbs[i]);
    }

    char lspath[96];
    if (dir[0] == '/')                        scpy(lspath, dir);
    else if (dir[0]) { scpy(lspath, ls_base()); scat(lspath, "/"); scat(lspath, dir); }
    else                                     scpy(lspath, g_cwd[0] ? g_cwd : "/");

    if (scmp(lspath, "/") == 0) {
        const char *roots[] = { "fda", "hda", "nfs" };
        for (int i = 0; i < 3 && nc < 64; i++)
            if (sncmp(roots[i], pfx, pl) == 0) scpy(cand[nc++], roots[i]);
    } else {
        sys_listdir(lspath, ld_buf, sizeof ld_buf);
        char *p = ld_buf;
        while (*p && nc < 64) {
            char *e = schr(p, '\n');
            int L = e ? (int)(e - p) : slen(p);
            if (L > 0 && L < 63) {
                char nm[64];
                sncpy(nm, p, L);
                if (sncmp(nm, pfx, pl) == 0) scpy(cand[nc++], nm);
            }
            if (!e) break;
            p = e + 1;
        }
    }

    if (nc == 0) return;

    char lcp[64];
    int ll = common_prefix(cand, nc, lcp);
    if (ll > pl) {
        const char *add = lcp + pl;
        while (*add && len < LINE - 1) { buf[len++] = *add; wc(*add); (*drawn)++; add++; }
        buf[len] = 0;
        if (nc == 1 && len < LINE - 1) { buf[len++] = ' '; buf[len] = 0; wc(' '); (*drawn)++; }
        *plen = len;
        return;
    }
    if (nc > 1) {
        wc('\n');
        for (int i = 0; i < nc; i++) { w(cand[i]); w("  "); }
        wc('\n');
        w(g_prompt);
        w(buf);
        *drawn = g_plen + len;
    }
    *plen = len;
}

/** Read one line. Returns the length, or -1 for end-of-input (^D on empty). */
static int readline(char *buf) {
    int len = 0, drawn;
    int hpos = hist_len;
    char saved[LINE];
    saved[0] = 0;

    w(g_prompt);
    g_plen = slen(g_prompt);
    drawn = g_plen;
    buf[0] = 0;

    for (;;) {
        int c = sys_getkey();

        if (c == '\n' || c == '\r') { wc('\n'); buf[len] = 0; return len; }
        if (c == 4) { if (len == 0) { wc('\n'); return -1; } continue; }
        if (c == 3) { buf[0] = 0; w("^C\n"); return 0; }

        if (c == 8 || c == 127) {
            if (len > 0) { len--; buf[len] = 0; w("\b \b"); drawn--; }
            continue;
        }
        if (c == 21) { len = 0; buf[0] = 0; redraw(buf, len, &drawn); continue; }
        if (c == 23) {
            while (len > 0 && buf[len - 1] == ' ') len--;
            while (len > 0 && buf[len - 1] != ' ') len--;
            buf[len] = 0;
            redraw(buf, len, &drawn);
            continue;
        }
        if (c == 12) {
            for (int i = 0; i < 40; i++) wc('\n');
            w(g_prompt); w(buf);
            drawn = g_plen + len;
            continue;
        }
        if (c == 16) {                       /* ^P */
            if (hpos > 0) {
                if (hpos == hist_len) sncpy(saved, buf, LINE - 1);
                hpos--;
                scpy(buf, hist[hpos]);
                len = slen(buf);
                redraw(buf, len, &drawn);
            }
            continue;
        }
        if (c == 14) {                       /* ^N */
            if (hpos < hist_len) {
                hpos++;
                scpy(buf, hpos == hist_len ? saved : hist[hpos]);
                len = slen(buf);
                redraw(buf, len, &drawn);
            }
            continue;
        }
        if (c == '\t') { complete(buf, &len, &drawn); continue; }

        if (c >= 32 && c < 127 && len < LINE - 1) {
            buf[len++] = (char)c;
            buf[len] = 0;
            wc((char)c);
            drawn++;
        }
    }
}

/* -------------------------------------------------------------------- main -- */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    sys_getcwd(g_cwd, sizeof g_cwd);
    scpy(g_oldcwd, g_cwd[0] ? g_cwd : "/");
    build_prompt();

    w("\nos zsh - minimalistic shell.  'help' for commands, ^D to leave.\n\n");

    alias_set("ll", "ls");
    alias_set("la", "ls");
    alias_set("h", "history");
    source_path("/hda/zshrc");

    static char line[LINE];
    while (!g_done) {
        int r = readline(line);
        if (r < 0) break;               /* ^D */
        if (r == 0) continue;
        hist_add(line);
        exec_line(line);
    }

    w("\nos zsh: back to the kernel console.\n");
    sys_exit(0);
    return 0;
}
