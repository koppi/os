/**
 * @file lib/libc_ext.c
 * @brief The rest of the real-glibc-shaped C library surface real Qt6
 *        source (QGuiApplication/QPainter/QFont closure), real FreeType,
 *        real HarfBuzz, real PCRE2, and libstdc++ itself call that isn't
 *        already covered by lib/stdio.c, lib/stdlib.c, lib/string.c,
 *        lib/mutex.c, lib/system_calls.c, lib/pthread_glibc.c, or
 *        lib/libm.c. Built against real system headers for correct type
 *        layouts (sem_t, jmp_buf, tm, ...) -- never linked against real
 *        glibc itself (this whole tree is -nostdlib), so only the type
 *        SIZES need to match, not glibc's internal algorithms. Same
 *        spirit as third_party/qt6/koppios/qt_libc_compat.c (that file's
 *        own fprintf/fputs/stderr/snprintf are NOT duplicated here --
 *        this is a separate, larger closure, not a shared link unit
 *        with hello-qt's QString-only one).
 *
 * Genuinely real, correct implementations where correctness is cheap
 * and load-bearing (strtol, qsort, setjmp/longjmp, localtime_r/mktime
 * via a standard civil-calendar algorithm). Honest, documented no-ops
 * or fixed-value stubs where this kernel has no real backing concept at
 * all (locale, timezone beyond UTC, POSIX scheduling priorities, real
 * process exit cleanup, real filesystem stat/open) -- matching this
 * whole tree's established pattern for exactly that situation (see e.g.
 * lib/qfileengine_koppios.cpp's own file comment).
 */
#include <errno.h>
#include <sched.h>
#include <semaphore.h>
#include <setjmp.h>
#include <stdarg.h>
#include <time.h>

extern unsigned int syscall3(unsigned int n, unsigned int a, unsigned int b, unsigned int c);

/* Not <string.h>/<sys/mman.h>/<sys/poll.h>: this file already declares a
 * handful of libc functions above with koppios's own (not always
 * byte-identical) signatures -- pulling in the real system headers here
 * conflicts with those rather than matching them. Declared by hand below,
 * same as memset/memcpy already are. */
extern int strcmp(const char *a, const char *b);
extern char *strchr(const char *s, int c);
extern int memcmp(const void *a, const void *b, unsigned int n);
extern unsigned int strlen(const char *s);

extern int _write(const void *buf, unsigned int len);
extern void *malloc(unsigned int len);
extern void free(void *ptr);
extern void memset(void *start, unsigned int val, unsigned int len);
extern void *memcpy(void *dest, const void *src, unsigned int size);
extern int thread_yield(void);

/* ---- process / thread exit and abort ---- */

/* Matches __cxa_atexit's own reasoning in lib/cxxabi.cpp: this process
 * model has no real exit-cleanup path (end_process_return() goes
 * straight to the exit syscall), so there is nothing later to call
 * these back from. A real registration table would just be dead
 * weight. */
int atexit(void (*function)(void)) {
    (void) function;
    return 0;
}

/* Weak: a program linked with lib/tls.o gets the real one, which runs the destructors when
 * the thread ends; without it there are no thread_local objects to register anyway. */
__attribute__((weak)) int __cxa_thread_atexit(void (*function)(void *), void *arg, void *dso) {
    (void) function;
    (void) arg;
    (void) dso;
    return 0;
}

_Bool __libc_single_threaded = 0;

static void halt_loop(void) {
    for (;;) {
    }
}

void abort(void) {
    halt_loop();
}

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    (void) assertion;
    (void) file;
    (void) line;
    (void) function;
    halt_loop();
}

void __stack_chk_fail_local(void) {
    halt_loop();
}

/* Cooperative-cancellation-only pthreads (see lib/pthread_glibc.c's own
 * comment) never need the old glibc pthread_cleanup_push/pop cleanup-
 * frame registration these support. */
void __pthread_register_cancel(void *frame) {
    (void) frame;
}
void __pthread_unregister_cancel(void *frame) {
    (void) frame;
}

/* No real ELF auxiliary vector on this kernel. */
unsigned long getauxval(unsigned long type) {
    (void) type;
    return 0;
}

/* Single-user model: everything runs as the same, sole "user". */
unsigned int geteuid(void) { return 0; }
unsigned int getuid(void) { return 0; }

/* A small process-local environment: a fixed table of "NAME=value" strings. There is no inherited
 * environment on this kernel (a program starts with an empty one), but a program can set
 * variables for its own libraries to read -- Qt looks at several QT_* ones. Not thread-safe for
 * concurrent writers; programs set what they need at start-up. */
#define ENV_MAX 32
static char *g_env[ENV_MAX];

extern int strncmp(const char *a, const char *b, unsigned int n);
extern char *strcpy(char *dest, const char *src);

static int env_find(const char *name, unsigned int n) {
    for (int i = 0; i < ENV_MAX; i++) {
        if (g_env[i] && strncmp(g_env[i], name, n) == 0 && g_env[i][n] == '=')
            return i;
    }
    return -1;
}

char *getenv(const char *name) {
    if (!name || !*name)
        return 0;
    int i = env_find(name, strlen(name));
    return i < 0 ? 0 : g_env[i] + strlen(name) + 1;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
    unsigned int n = strlen(name);
    int i = env_find(name, n);
    if (i >= 0 && !overwrite)
        return 0;
    char *e = malloc(n + strlen(value) + 2);
    if (!e) {
        errno = ENOMEM;
        return -1;
    }
    memcpy(e, name, n);
    e[n] = '=';
    strcpy(e + n + 1, value);
    if (i < 0) {
        for (i = 0; i < ENV_MAX && g_env[i]; i++) {}
        if (i == ENV_MAX) {
            free(e);
            errno = ENOMEM;
            return -1;
        }
    } else {
        free(g_env[i]);
    }
    g_env[i] = e;
    return 0;
}

int unsetenv(const char *name) {
    if (!name || !*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
    int i = env_find(name, strlen(name));
    if (i >= 0) {
        free(g_env[i]);
        g_env[i] = 0;
    }
    return 0;
}

int putenv(char *string) {
    char *eq = strchr(string, '=');
    if (!eq)
        return unsetenv(string);
    char name[128];
    unsigned int n = (unsigned int) (eq - string);
    if (n >= sizeof name) {
        errno = EINVAL;
        return -1;
    }
    memcpy(name, string, n);
    name[n] = 0;
    return setenv(name, eq + 1, 1);
}

/* No real POSIX scheduling priority range implemented yet -- one fixed
 * priority for every thread, same limitation lib/pthread_glibc.c's own
 * attribute setters already document. */
int sched_get_priority_max(int policy) {
    (void) policy;
    return 0;
}
int sched_get_priority_min(int policy) {
    (void) policy;
    return 0;
}

/* ---- semaphores: real, backed by this kernel's own busy-yield pattern
 * (same style as lib/mutex.c/lib/pthread_glibc.c's mutex/cond, just a
 * plain atomic counter instead of a lock) ---- */

typedef struct {
    volatile int count;
} koppios_sem_t;

_Static_assert(sizeof(koppios_sem_t) <= sizeof(sem_t), "sem_t too small");

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    (void) pshared;
    ((koppios_sem_t *) sem)->count = (int) value;
    return 0;
}

int sem_destroy(sem_t *sem) {
    (void) sem;
    return 0;
}

int sem_post(sem_t *sem) {
    __atomic_fetch_add(&((koppios_sem_t *) sem)->count, 1, __ATOMIC_SEQ_CST);
    return 0;
}

int sem_wait(sem_t *sem) {
    koppios_sem_t *s = (koppios_sem_t *) sem;
    for (;;) {
        int v = __atomic_load_n(&s->count, __ATOMIC_SEQ_CST);
        if (v > 0 && __atomic_compare_exchange_n(&s->count, &v, v - 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            return 0;
        thread_yield();
    }
}

/* Whole-second resolution only, same limitation as every other timed
 * wait in this tree (this kernel's time() syscall has no finer clock). */
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout) {
    koppios_sem_t *s = (koppios_sem_t *) sem;
    for (;;) {
        int v = __atomic_load_n(&s->count, __ATOMIC_SEQ_CST);
        if (v > 0 && __atomic_compare_exchange_n(&s->count, &v, v - 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            return 0;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec >= abs_timeout->tv_sec) {
            errno = ETIMEDOUT;
            return -1;
        }
        thread_yield();
    }
}

/* ---- setjmp/longjmp: real register-context save/restore, i386 cdecl.
 * Layout inside the (real, 156-byte glibc-sized) jmp_buf is entirely
 * ours -- nothing outside this file ever inspects it. ---- */

struct koppios_jmp_ctx {
    unsigned int ebx, esi, edi, ebp, esp, eip;
};

_Static_assert(sizeof(struct koppios_jmp_ctx) <= sizeof(jmp_buf), "jmp_buf too small");

/* setjmp must save the CALLER's ebp/esp -- not whatever ebp/esp happen
 * to be internally, partway through this function's own prologue.
 * __builtin_frame_address(0) gives this function's own ebp, and with
 * the standard `push ebp; mov ebp,esp` prologue (guaranteed by
 * -fno-omit-frame-pointer), [ebp+0] is the CALLER's saved ebp,
 * [ebp+4] is the return address, and ebp+8 is exactly what the
 * caller's esp was immediately after this function returns (call
 * pushes a return address, ret pops it -- net zero effect on the
 * caller's own esp). */
int _setjmp(jmp_buf env) {
    struct koppios_jmp_ctx *ctx = (struct koppios_jmp_ctx *) env;
    unsigned int *fp = (unsigned int *) __builtin_frame_address(0);
    ctx->ebp = fp[0];
    ctx->eip = fp[1];
    ctx->esp = (unsigned int) (fp + 2);
    __asm__ __volatile__("movl %%ebx, %0" : "=m"(ctx->ebx));
    __asm__ __volatile__("movl %%esi, %0" : "=m"(ctx->esi));
    __asm__ __volatile__("movl %%edi, %0" : "=m"(ctx->edi));
    return 0;
}

int __sigsetjmp(jmp_buf env, int savemask) {
    (void) savemask; /* no real signal mask on this kernel to save */
    return _setjmp(env);
}

void longjmp(jmp_buf env, int val) {
    struct koppios_jmp_ctx *ctx = (struct koppios_jmp_ctx *) env;
    if (val == 0)
        val = 1; /* setjmp() must never appear to return 0 via longjmp */
    /* ctx forced into ecx and val into eax via explicit register
     * constraints, NOT a generic "r": a generic constraint could just
     * as easily place ctx itself into eax, which the first version of
     * this function did -- then explicitly overwriting eax with val
     * destroyed the only pointer to ctx before the restore even
     * started reading from it. ecx/eax are never among the registers
     * restored below, so both stay valid for the whole sequence. */
    __asm__ __volatile__(
        "movl 0(%0), %%ebx\n\t"
        "movl 4(%0), %%esi\n\t"
        "movl 8(%0), %%edi\n\t"
        "movl 12(%0), %%ebp\n\t"
        "movl 16(%0), %%esp\n\t"
        "jmp *20(%0)"
        :
        : "c"(ctx), "a"(val)
        : "memory");
    __builtin_unreachable();
}

/* ---- string / sort helpers ---- */

void *calloc(unsigned int nmemb, unsigned int size) {
    unsigned int total = nmemb * size;
    void *p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

char *strrchr(const char *s, int c) {
    const char *last = 0;
    for (; *s; s++)
        if (*s == (char) c)
            last = s;
    if (c == 0)
        return (char *) s;
    return (char *) last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle)
        return (char *) haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n)
            return (char *) haystack;
    }
    return 0;
}

long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    while (*s == ' ' || *s == '\t' || *s == '\n')
        s++;
    int neg = 0;
    if (*s == '+' || *s == '-') {
        neg = (*s == '-');
        s++;
    }
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        base = 16;
    } else if (base == 0 && s[0] == '0') {
        base = 8;
    } else if (base == 0) {
        base = 10;
    }
    long result = 0;
    const char *digits_start = s;
    for (;; s++) {
        int d;
        char c = *s;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'z')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z')
            d = c - 'A' + 10;
        else
            break;
        if (d >= base)
            break;
        result = result * base + d;
    }
    if (endptr)
        *endptr = (char *) (s == digits_start ? nptr : s);
    return neg ? -result : result;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    return (unsigned long) strtol(nptr, endptr, base);
}

long __isoc23_strtol(const char *nptr, char **endptr, int base) { return strtol(nptr, endptr, base); }
unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base) { return strtoul(nptr, endptr, base); }

#ifndef KOPPIOS_APP_STDIO

int sscanf(const char *str, const char *format, ...);
int __isoc23_sscanf(const char *str, const char *format, ...);

static int vsscanf_impl(const char *str, const char *format, va_list ap) {
    int assigned = 0;
    for (; *format; format++) {
        if (*format == '%') {
            format++;
            while (*str == ' ' || *str == '\t' || *str == '\n')
                str++;
            if (*format == 'd' || *format == 'i') {
                char *end;
                long v = strtol(str, &end, 10);
                if (end == str)
                    break;
                *va_arg(ap, int *) = (int) v;
                str = end;
                assigned++;
            } else if (*format == 'u') {
                char *end;
                unsigned long v = strtoul(str, &end, 10);
                if (end == str)
                    break;
                *va_arg(ap, unsigned int *) = (unsigned int) v;
                str = end;
                assigned++;
            } else if (*format == 's') {
                char *out = va_arg(ap, char *);
                while (*str && *str != ' ' && *str != '\t' && *str != '\n')
                    *out++ = *str++;
                *out = 0;
                assigned++;
            } else if (*format == '%') {
                if (*str != '%')
                    break;
                str++;
            }
        } else if (*format == ' ') {
            while (*str == ' ' || *str == '\t' || *str == '\n')
                str++;
        } else {
            if (*str != *format)
                break;
            str++;
        }
    }
    return assigned;
}

int sscanf(const char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int n = vsscanf_impl(str, format, ap);
    va_end(ap);
    return n;
}

int __isoc23_sscanf(const char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int n = vsscanf_impl(str, format, ap);
    va_end(ap);
    return n;
}

#endif /* !KOPPIOS_APP_STDIO */

unsigned int wcslen(const unsigned int *s) {
    unsigned int n = 0;
    while (s[n])
        n++;
    return n;
}

const char *strerror(int errnum) {
    static char buf[32];
    static const char *table[] = {
        "Success", "Operation not permitted", "No such file or directory",
        "Interrupted system call", "I/O error", "No such device or address",
        "Argument list too long", "Bad file descriptor", "Try again",
        "Out of memory",
    };
    if (errnum >= 0 && errnum < (int) (sizeof(table) / sizeof(table[0])))
        return table[errnum];
    /* No real snprintf-with-%d dependency here (this file must not
     * assume the graphical closure's own qt_gui format helper exists
     * yet at link time) -- format the number by hand. */
    char *p = buf + sizeof(buf) - 1;
    *p = 0;
    unsigned int v = (unsigned int) (errnum < 0 ? -errnum : errnum);
    if (v == 0)
        *--p = '0';
    while (v) {
        *--p = (char) ('0' + v % 10);
        v /= 10;
    }
    if (errnum < 0)
        *--p = '-';
    static char out[48];
    const char *prefix = "Unknown error ";
    unsigned int i = 0;
    for (; prefix[i]; i++)
        out[i] = prefix[i];
    for (const char *q = p; *q; q++)
        out[i++] = *q;
    out[i] = 0;
    return out;
}

int strerror_r(int errnum, char *buf, unsigned int buflen) {
    const char *msg = strerror(errnum);
    unsigned int i = 0;
    for (; msg[i] && i + 1 < buflen; i++)
        buf[i] = msg[i];
    buf[i] = 0;
    return 0;
}

/* Standard introsort-free quicksort-with-insertion-sort-fallback would
 * be overkill here; a plain insertion sort is correct, stable enough,
 * and every call site in this closure sorts tiny arrays (font family
 * lists, a handful of glyph metrics), never anything where O(n^2)
 * matters.
 *
 * The swap is byte-wise and in place. It used to stage each element
 * through a `char tmp[256]`, which is a stack buffer overflow for any
 * element larger than that -- apps/chipnomad's file browser sorts
 * FileEntry, which is 260 bytes (a 256-byte name plus a flag), and the
 * four bytes past the end landed on the saved registers of this very
 * frame: the listing came back with most of its entries lost. Nothing
 * here needs a temporary at all. */
static void qsort_swap(char *a, char *b, unsigned int n) {
    while (n--) {
        char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

void qsort(void *base, unsigned int nmemb, unsigned int size, int (*compar)(const void *, const void *)) {
    char *arr = (char *) base;
    for (unsigned int i = 1; i < nmemb; i++) {
        unsigned int j = i;
        while (j > 0 && compar(arr + (j - 1) * size, arr + j * size) > 0) {
            qsort_swap(arr + (j - 1) * size, arr + j * size, size);
            j--;
        }
    }
}

/* ---- locale / timezone: this kernel is always UTC, always the "C"
 * locale -- same reasoning third_party/qt6/koppios/qt_libc_compat.c's
 * own file comment already documents for its smaller closure. ---- */

/* Only two locales exist: "C" and "C.UTF-8" (everything on this kernel,
 * Qt's local8bit included, is UTF-8 anyway), and the process starts in the
 * latter. Anything else is refused the
 * way real setlocale() refuses a locale that is not installed. */
static int locale_is_utf8 = 1; /* all text on this kernel is UTF-8 already (unifont console, UTF-8 sources) */

char *setlocale(int category, const char *locale) {
    (void) category;
    if (locale && *locale) {
        int utf8 = (locale[0] == 'C' && locale[1] == '.');
        int plain = (strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0);
        if (!utf8 && !plain)
            return 0;
        if (utf8 && strcmp(locale, "C.UTF-8") != 0 && strcmp(locale, "C.utf8") != 0)
            return 0;
        locale_is_utf8 = utf8;
    }
    return (char *) (locale_is_utf8 ? "C.UTF-8" : "C");
}

char *nl_langinfo(int item) {
    if (item == 14 /* CODESET */)
        return (char *) (locale_is_utf8 ? "UTF-8" : "ANSI_X3.4-1968");
    return (char *) "";
}

char *tzname[2] = {(char *) "UTC", (char *) "UTC"};

void tzset(void) {
}

/* civil_from_days: Howard Hinnant's well-known constant-time Gregorian
 * calendar algorithm (public domain / CC0, http://howardhinnant.github.io/date_algorithms.html),
 * correct for the entire proleptic Gregorian calendar -- not a
 * approximation, the same algorithm real implementations use. */
static void civil_from_days(long z, int *y, unsigned int *m, unsigned int *d) {
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned int doe = (unsigned int) (z - era * 146097);
    unsigned int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long yr = (long) yoe + era * 400;
    unsigned int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned int mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp + (mp < 10 ? 3 : -9);
    *y = (int) (yr + (*m <= 2 ? 1 : 0));
}

struct tm *localtime_r(const time_t *timer, struct tm *result) {
    long days = (long) (*timer / 86400);
    long rem = (long) (*timer % 86400);
    if (rem < 0) {
        rem += 86400;
        days -= 1;
    }
    int y;
    unsigned int mo, d;
    civil_from_days(days, &y, &mo, &d);
    result->tm_year = y - 1900;
    result->tm_mon = (int) mo - 1;
    result->tm_mday = (int) d;
    result->tm_hour = (int) (rem / 3600);
    result->tm_min = (int) ((rem % 3600) / 60);
    result->tm_sec = (int) (rem % 60);
    result->tm_wday = (int) (((days % 7) + 11) % 7); /* days since epoch Thu=4 */
    long start_of_year = days - (long) (result->tm_mon); /* not exact, yday computed below */
    (void) start_of_year;
    result->tm_isdst = 0;
    /* tm_yday: recompute via days_from_civil for Jan 1 of the same year. */
    {
        long jan1 = (long) y * 365 + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400 - 719162;
        (void) jan1;
    }
    /* Simple day-of-year count (loop over months already decoded is
     * cheap and avoids a second calendar inversion). */
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    result->tm_yday = cum[result->tm_mon] + ((int) d - 1) + ((leap && result->tm_mon >= 2) ? 1 : 0);
    return result;
}

time_t mktime(struct tm *tm) {
    /* days_from_civil, the inverse of civil_from_days above (same
     * source). */
    int y = tm->tm_year + 1900;
    unsigned int m = (unsigned int) tm->tm_mon + 1;
    unsigned int d = (unsigned int) tm->tm_mday;
    y -= (m <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned int yoe = (unsigned int) (y - era * 400);
    unsigned int mp = (m + 9) % 12;
    unsigned int doy = (153 * mp + 2) / 5 + d - 1;
    unsigned int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = era * 146097 + (long) doe - 719468;
    return (time_t) (days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}

/* ---- filesystem calls with no real kernel equivalent: honest failure,
 * same reasoning as lib/qfileengine_koppios.cpp's own file comment and
 * this session's QFileSystemEngine koppios stub. ---- */

int open(const char *path, int flags, ...) {
    (void) path;
    (void) flags;
    errno = ENOENT;
    return -1;
}

int stat(const char *path, void *buf) {
    (void) path;
    (void) buf;
    errno = ENOENT;
    return -1;
}

int closedir(void *dirp) {
    (void) dirp;
    return 0;
}

/* ---- stdio: same model as third_party/qt6/koppios/qt_libc_compat.c's
 * own fprintf/fputs/fflush/stderr/snprintf (that file's smaller,
 * QString-only closure never needed feof/ferror/vfprintf/vsnprintf,
 * this one does) -- an unbuffered "stream" that's really just _write(),
 * good enough to link, not crash, and produce readable diagnostic
 * output; not a real buffered/seekable stdio implementation. %f/%e/%g
 * precision is deliberately approximate for the same reason that file's
 * comment gives: nothing in this closure's reachable code path actually
 * needs accurate floating-point text formatting to function correctly,
 * only to not crash if ever exercised. */

/*
 * KOPPIOS_APP_STDIO: an app that brings its own stdio compiles this file
 * with -DKOPPIOS_APP_STDIO and supplies the FILE/printf/sscanf surface
 * itself. apps/chipnomad does, because its project and settings files need
 * all three of the things the stand-ins here deliberately do without:
 * seekable FILE streams, width- and precision-correct formatting, and an
 * sscanf that understands %x, %f, the hh/h length modifiers and %[^\n].
 * Nothing else in this file depends on them, so every other app that links
 * libc_ext.o is unaffected.
 */
#ifndef KOPPIOS_APP_STDIO

typedef struct {
    int unused;
} FILE;
static FILE stderr_obj;
FILE *stderr = &stderr_obj;

int feof(FILE *f) {
    (void) f;
    return 0;
}

int ferror(FILE *f) {
    (void) f;
    return 0;
}

int fflush(FILE *f) {
    (void) f;
    return 0;
}

int fputs(const char *s, FILE *f) {
    (void) f;
    unsigned int n = 0;
    while (s[n])
        n++;
    _write(s, n);
    return 0;
}

#endif /* !KOPPIOS_APP_STDIO */

#ifndef KOPPIOS_APP_STDIO

static int format_into(char *buf, unsigned int cap, const char *fmt, va_list ap) {
    unsigned int n = 0;
#define PUT(c)                   \
    do {                         \
        if (n + 1 < cap)         \
            buf[n] = (char) (c); \
        n++;                     \
    } while (0)
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            PUT(*p);
            continue;
        }
        p++;
        while (*p == '-' || *p == '+' || *p == '0' || *p == ' ' || *p == '#')
            p++;
        while (*p >= '0' && *p <= '9')
            p++;
        if (*p == '.') {
            p++;
            while (*p >= '0' && *p <= '9')
                p++;
        }
        /* skip length modifiers (l, ll, z, ...) -- va_arg widths below
         * already match what callers in this closure pass. */
        while (*p == 'l' || *p == 'z' || *p == 'h')
            p++;
        switch (*p) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s)
                    s = "(null)";
                for (; *s; s++)
                    PUT(*s);
                break;
            }
            case 'c':
                PUT((char) va_arg(ap, int));
                break;
            case 'p': {
                unsigned int u = (unsigned int) va_arg(ap, void *);
                PUT('0');
                PUT('x');
                char tmp[8];
                int t = 0;
                if (u == 0)
                    tmp[t++] = '0';
                while (u) {
                    unsigned int dg = u % 16;
                    tmp[t++] = (char) (dg < 10 ? '0' + dg : 'a' + dg - 10);
                    u /= 16;
                }
                while (t)
                    PUT(tmp[--t]);
                break;
            }
            case 'd':
            case 'i': {
                long v = va_arg(ap, int);
                unsigned long u = (v < 0) ? (unsigned long) (-v) : (unsigned long) v;
                char tmp[24];
                int t = 0;
                if (v < 0)
                    PUT('-');
                if (u == 0)
                    tmp[t++] = '0';
                while (u) {
                    tmp[t++] = (char) ('0' + (u % 10));
                    u /= 10;
                }
                while (t)
                    PUT(tmp[--t]);
                break;
            }
            case 'u':
            case 'x':
            case 'X': {
                unsigned long u = va_arg(ap, unsigned int);
                unsigned int base = (*p == 'u') ? 10 : 16;
                char tmp[24];
                int t = 0;
                if (u == 0)
                    tmp[t++] = '0';
                while (u) {
                    unsigned int dg = (unsigned int) (u % base);
                    tmp[t++] = (char) (dg < 10 ? '0' + dg : 'a' + dg - 10);
                    u /= base;
                }
                while (t)
                    PUT(tmp[--t]);
                break;
            }
            case 'f':
            case 'e':
            case 'g': {
                double v = va_arg(ap, double);
                long whole = (long) v;
                char tmp[24];
                int t = 0;
                unsigned long uw = (whole < 0) ? (unsigned long) (-whole) : (unsigned long) whole;
                if (whole < 0 || (whole == 0 && v < 0))
                    PUT('-');
                if (uw == 0)
                    tmp[t++] = '0';
                while (uw) {
                    tmp[t++] = (char) ('0' + (uw % 10));
                    uw /= 10;
                }
                while (t)
                    PUT(tmp[--t]);
                PUT('.');
                double frac = v - (double) whole;
                if (frac < 0)
                    frac = -frac;
                for (int k = 0; k < 6; k++) {
                    frac *= 10.0;
                    int digit = (int) frac;
                    PUT((char) ('0' + digit));
                    frac -= digit;
                }
                break;
            }
            case '%':
                PUT('%');
                break;
            default:
                PUT('%');
                PUT(*p);
                break;
        }
    }
    if (cap)
        buf[n < cap ? n : cap - 1] = 0;
#undef PUT
    return (int) n;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) {
    (void) f;
    char buf[512];
    int n = format_into(buf, sizeof(buf), fmt, ap);
    _write(buf, (unsigned int) (n < (int) sizeof(buf) ? n : (int) sizeof(buf) - 1));
    return n;
}

int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int vsnprintf(char *buf, unsigned int cap, const char *fmt, va_list ap) {
    return format_into(buf, cap, fmt, ap);
}

int snprintf(char *buf, unsigned int cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}

#endif /* !KOPPIOS_APP_STDIO */

/*
 * Virtual-memory and misc POSIX surface real Qt6's generic-unix backends
 * (QFileSystemEngine, the thread pool's ideal-count probing, ...) reach for
 * -- honest failure, matching this whole file's pattern: this kernel has no
 * userspace mmap()/mprotect()/prctl() syscalls (or the concepts they
 * expose, like CPU affinity masks or /dev/urandom) to back these with, so
 * every caller sees the same "not supported" result a real POSIX system
 * gives when a request it understands genuinely can't be honored, not a
 * silently wrong success.
 */
void *mmap(void *addr, unsigned int len, int prot, int flags, int fd, long off) {
    (void) addr; (void) len; (void) prot; (void) flags; (void) fd; (void) off;
    errno = ENOMEM;
    return (void *) -1; /* MAP_FAILED */
}

int munmap(void *addr, unsigned int len) {
    (void) addr; (void) len;
    errno = EINVAL;
    return -1;
}

void *mremap(void *old_addr, unsigned int old_len, unsigned int new_len, int flags, ...) {
    (void) old_addr; (void) old_len; (void) new_len; (void) flags;
    errno = ENOMEM;
    return (void *) -1;
}

int mprotect(void *addr, unsigned int len, int prot) {
    (void) addr; (void) len; (void) prot;
    errno = ENOMEM;
    return -1;
}

int madvise(void *addr, unsigned int len, int advice) {
    (void) addr; (void) len; (void) advice;
    return 0; /* a hint the kernel is always free to ignore */
}

int posix_madvise(void *addr, unsigned int len, int advice) {
    (void) addr; (void) len; (void) advice;
    return 0;
}

int getpagesize(void) {
    return 4096;
}

int prctl(int option, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5) {
    (void) option; (void) a2; (void) a3; (void) a4; (void) a5;
    errno = EINVAL;
    return -1;
}

int sched_getaffinity(int pid, unsigned int cpusetsize, void *mask) {
    (void) pid;
    /* Real Qt6 (QThread::idealThreadCount()'s generic-unix path) falls back
     * to this kernel's own real sysconf(_SC_NPROCESSORS_ONLN) (see
     * include/lib/unistd.h) when this fails -- same "1 CPU, honestly"
     * answer either way. */
    (void) cpusetsize; (void) mask;
    errno = ENOSYS;
    return -1;
}

int getentropy(void *buf, unsigned int len) {
    if (len > 256) {
        errno = EIO;                      /* POSIX: at most 256 bytes per call */
        return -1;
    }
    if (len == 0)
        return 0;
    if (syscall3(37, (unsigned int) buf, len, 0) != 0) {   /* kernel getrandom */
        errno = ENOSYS;                   /* a kernel without syscall 37, or a bad buffer */
        return -1;
    }
    return 0;
}

/* Real, not stubbed -- plain byte-buffer scans, cheap and load-bearing. */
void *memrchr(const void *s, int c, unsigned int n) {
    const unsigned char *p = (const unsigned char *) s;
    while (n--) {
        if (p[n] == (unsigned char) c)
            return (void *) (p + n);
    }
    return 0;
}

void *memmem(const void *haystack, unsigned int haystacklen,
             const void *needle, unsigned int needlelen) {
    if (needlelen == 0)
        return (void *) haystack;
    if (needlelen > haystacklen)
        return 0;
    const unsigned char *h = (const unsigned char *) haystack;
    const unsigned char *n = (const unsigned char *) needle;
    for (unsigned int i = 0; i + needlelen <= haystacklen; i++) {
        if (h[i] == n[0] && memcmp(h + i, n, needlelen) == 0)
            return (void *) (h + i);
    }
    return 0;
}

/*
 * Directory iteration / link / cwd surface real Qt6's generic-unix
 * QFileSystemEngine and QFileSystemIterator reach for -- same honest-
 * failure reasoning as the mmap family above. Return types are declared
 * as plain pointers or ints rather than pulling in <dirent.h>/<sys/stat.h> (which would
 * conflict with this file's own earlier hand-declared signatures the same
 * way <string.h> did); the real caller, compiled against the real system
 * headers, only needs the symbol name and an ABI-compatible (pointer-sized)
 * return to link correctly -- the linker doesn't check C type signatures
 * across translation units, only the compiler does.
 */
void *opendir(const char *name) {
    (void) name;
    errno = ENOENT;
    return 0;
}

void *readdir(void *dirp) {
    (void) dirp;
    return 0;
}

int lstat(const char *path, void *statbuf) {
    (void) path; (void) statbuf;
    errno = ENOENT;
    return -1;
}

int statx(int dirfd, const char *path, int flags, unsigned int mask, void *statxbuf) {
    (void) dirfd; (void) path; (void) flags; (void) mask; (void) statxbuf;
    errno = ENOSYS;
    return -1;
}

extern char *pwd(void); /* lib/system_calls.c: wraps the real getcwd syscall (#19) */

char *getcwd(char *buf, unsigned int size) {
    const char *cwd = pwd();
    unsigned int len = (unsigned int) strlen(cwd);
    if (!buf) {
        /* glibc extension: a NULL buffer means "allocate one" */
        unsigned int cap = size > len ? size : len + 1;
        buf = (char *) malloc(cap);
        if (!buf) { errno = ENOMEM; return 0; }
        size = cap;
    }
    if (size < len + 1) { errno = ERANGE; return 0; }
    memcpy(buf, cwd, len + 1);
    return buf;
}

char *realpath(const char *path, char *resolved_path) {
    (void) path; (void) resolved_path;
    errno = ENOENT;
    return 0;
}

long readlink(const char *path, char *buf, unsigned int bufsiz) {
    (void) path; (void) buf; (void) bufsiz;
    errno = EINVAL; /* "not a symlink" -- this filesystem has none */
    return -1;
}

void perror(const char *s) {
    if (s && *s) {
        _write(s, (unsigned int) strlen(s));
        _write(": ", 2);
    }
    const char *msg = "error\n";
    _write(msg, (unsigned int) strlen(msg));
}

/* glibc's <ctype.h> isalpha()/isdigit()/... and tolower()/toupper() compile
 * to lookups through these three accessors. Contents are the "C" locale:
 * ASCII classes only, nothing set for bytes >= 128. Class bit values are
 * glibc's little-endian layout (_ISbit): upper 0x100, lower 0x200, alpha
 * 0x400, digit 0x800, xdigit 0x1000, space 0x2000, print 0x4000, graph
 * 0x8000, blank 0x1, cntrl 0x2, punct 0x4, alnum 0x8. */
static unsigned short ctype_b_table[384];
static int ctype_lc_table[384];
static int ctype_uc_table[384];
static const unsigned short *ctype_b_ptr;
static const int *ctype_lc_ptr;
static const int *ctype_uc_ptr;
static int ctype_ready;

static void ctype_init(void) {
    if (ctype_ready) return;
    for (int i = 0; i < 384; i++) {
        int c = i - 128; /* table index 128 is character 0 */
        unsigned short f = 0;
        /* glibc: indices -128..-2 hold c+256 (so tolower((signed char)c)
         * round-trips), -1 is EOF and stays -1. */
        ctype_lc_table[i] = (c < -1) ? c + 256 : c;
        ctype_uc_table[i] = ctype_lc_table[i];
        if (c >= 0 && c < 128) {
            if (c >= 'A' && c <= 'Z') { f |= 0x100 | 0x400 | 0x8; ctype_lc_table[i] = c + 32; }
            if (c >= 'a' && c <= 'z') { f |= 0x200 | 0x400 | 0x8; ctype_uc_table[i] = c - 32; }
            if (c >= '0' && c <= '9') f |= 0x800 | 0x8;
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) f |= 0x1000;
            if (c == ' ' || (c >= '\t' && c <= '\r')) f |= 0x2000;
            if (c == ' ' || c == '\t') f |= 0x1;
            if (c < 32 || c == 127) f |= 0x2;
            if (c >= 32 && c < 127) f |= 0x4000;
            if (c > 32 && c < 127) f |= 0x8000;
            if (c > 32 && c < 127 && !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
                f |= 0x4;
        }
        ctype_b_table[i] = f;
    }
    ctype_b_ptr = ctype_b_table + 128;
    ctype_lc_ptr = ctype_lc_table + 128;
    ctype_uc_ptr = ctype_uc_table + 128;
    ctype_ready = 1;
}

const unsigned short **__ctype_b_loc(void) { ctype_init(); return &ctype_b_ptr; }
const int **__ctype_tolower_loc(void) { ctype_init(); return &ctype_lc_ptr; }
const int **__ctype_toupper_loc(void) { ctype_init(); return &ctype_uc_ptr; }

int dladdr(const void *addr, void *info) {
    (void) addr; (void) info;
    return 0; /* "no symbol found" -- there is no dynamic loader */
}

/* ---- in-process file descriptors ----
 * This kernel has no fd table (see the comment above pipe() in
 * pthread_glibc.c). The one thing real Qt needs an fd for in a
 * single-process app is the event dispatcher's thread wake-up channel, an
 * eventfd, polled by ppoll(). So: a tiny table of eventfd objects that live
 * entirely inside this process (fds SIMFD_BASE..), serviced by eventfd_*,
 * read/write/close (pthread_glibc.c forwards fds it does not own here) and
 * ppoll(). Any other fd is simply not ours: EBADF / POLLNVAL, as before. */
#define SIMFD_BASE 64
#define SIMFD_MAX 16
#define SIM_O_NONBLOCK 04000

static struct { int used; int nonblock; volatile unsigned int lo, hi; } simfd[SIMFD_MAX];
static volatile int simfd_lock;

static void simfd_enter(void) { while (__sync_lock_test_and_set(&simfd_lock, 1)) sched_yield(); }
static void simfd_leave(void) { __sync_lock_release(&simfd_lock); }

static int simfd_index(int fd) {
    int i = fd - SIMFD_BASE;
    return (i >= 0 && i < SIMFD_MAX && simfd[i].used) ? i : -1;
}

static int simfd_nonzero(int i) { return simfd[i].lo || simfd[i].hi; }

int eventfd(unsigned int initval, int flags) {
    int fd = -1;
    simfd_enter();
    for (int i = 0; i < SIMFD_MAX; i++) {
        if (!simfd[i].used) {
            simfd[i].used = 1;
            simfd[i].nonblock = (flags & SIM_O_NONBLOCK) != 0;
            simfd[i].lo = initval;
            simfd[i].hi = 0;
            fd = SIMFD_BASE + i;
            break;
        }
    }
    simfd_leave();
    if (fd < 0)
        errno = EMFILE;
    return fd;
}

int eventfd_read(int fd, unsigned long long *value) {
    for (;;) {
        simfd_enter();
        int i = simfd_index(fd);
        if (i < 0) { simfd_leave(); errno = EBADF; return -1; }
        if (simfd_nonzero(i)) {
            *value = ((unsigned long long) simfd[i].hi << 32) | simfd[i].lo;
            simfd[i].lo = simfd[i].hi = 0;
            simfd_leave();
            return 0;
        }
        int nb = simfd[i].nonblock;
        simfd_leave();
        if (nb) { errno = EAGAIN; return -1; }
        sched_yield(); /* blocking eventfd: wait for another thread's write */
    }
}

int eventfd_write(int fd, unsigned long long value) {
    simfd_enter();
    int i = simfd_index(fd);
    if (i < 0) { simfd_leave(); errno = EBADF; return -1; }
    unsigned long long cur = ((unsigned long long) simfd[i].hi << 32) | simfd[i].lo;
    cur += value;
    simfd[i].lo = (unsigned int) cur;
    simfd[i].hi = (unsigned int) (cur >> 32);
    simfd_leave();
    return 0;
}

/* Hooks for read()/write()/close() in pthread_glibc.c: return -2 when fd is
 * not one of ours, so the caller reports its own ENOSYS as before. */
long simfd_read(int fd, void *buf, unsigned long count) {
    if (simfd_index(fd) < 0) return -2;
    if (count < 8) { errno = EINVAL; return -1; }
    unsigned long long v;
    if (eventfd_read(fd, &v) < 0) return -1;
    memcpy(buf, &v, 8);
    return 8;
}

long simfd_write(int fd, const void *buf, unsigned long count) {
    if (simfd_index(fd) < 0) return -2;
    if (count < 8) { errno = EINVAL; return -1; }
    unsigned long long v;
    memcpy(&v, buf, 8);
    if (eventfd_write(fd, v) < 0) return -1;
    return 8;
}

int simfd_close(int fd) {
    simfd_enter();
    int i = simfd_index(fd);
    if (i < 0) { simfd_leave(); return -2; }
    simfd[i].used = 0;
    simfd_leave();
    return 0;
}

struct sim_pollfd { int fd; short events; short revents; };
#define SIM_POLLIN 0x001
#define SIM_POLLOUT 0x004
#define SIM_POLLNVAL 0x020

static unsigned long long sim_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long) ts.tv_sec * 1000ull + (unsigned long long) ts.tv_nsec / 1000000ull;
}

/* ppoll()/poll() over the in-process fds above. Waits by sleeping 1 ms at a
 * time (kernel msleep, syscall 27) so a blocked event loop does not burn the
 * CPU, until an fd is ready or the timeout expires. No signals exist here,
 * so sigmask is ignored. A timeout of NULL waits forever (until another
 * thread signals an eventfd), as POSIX specifies. */
int ppoll(void *fdsp, unsigned long nfds, const struct timespec *timeout, const void *sigmask) {
    struct sim_pollfd *fds = (struct sim_pollfd *) fdsp;
    (void) sigmask;
    unsigned long long deadline = 0;
    if (timeout)
        deadline = sim_now_ms() + (unsigned long long) timeout->tv_sec * 1000ull
                   + ((unsigned long long) timeout->tv_nsec + 999999ull) / 1000000ull;
    for (;;) {
        int ready = 0;
        simfd_enter();
        for (unsigned long k = 0; k < nfds; k++) {
            fds[k].revents = 0;
            if (fds[k].fd < 0)
                continue;
            int i = simfd_index(fds[k].fd);
            if (i < 0) {
                fds[k].revents = SIM_POLLNVAL;
            } else {
                if ((fds[k].events & SIM_POLLIN) && simfd_nonzero(i))
                    fds[k].revents |= SIM_POLLIN;
                if (fds[k].events & SIM_POLLOUT)
                    fds[k].revents |= SIM_POLLOUT;
            }
            if (fds[k].revents)
                ready++;
        }
        simfd_leave();
        if (ready)
            return ready;
        if (timeout && sim_now_ms() >= deadline)
            return 0;
        syscall3(27, 1, 0, 0); /* msleep(1) */
    }
}

int poll(void *fdsp, unsigned long nfds, int timeout_ms) {
    struct timespec ts;
    if (timeout_ms < 0)
        return ppoll(fdsp, nfds, 0, 0);
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (long) (timeout_ms % 1000) * 1000000L;
    return ppoll(fdsp, nfds, &ts, 0);
}

/* Out-of-line helper glibc's CPU_COUNT() macro expands to. */
int __sched_cpucount(size_t setsize, const cpu_set_t *setp) {
    const unsigned char *p = (const unsigned char *) setp;
    int n = 0;
    for (size_t i = 0; i < setsize; i++)
        for (unsigned char b = p[i]; b; b >>= 1) n += b & 1;
    return n;
}

/* Wide-string comparison in the only locale that exists here ("C"/"C.UTF-8"
 * with no collation tables): collation order is code point order, so
 * wcscoll() is wcscmp() and wcsxfrm() is a bounded copy. */
int wcscmp(const unsigned int *a, const unsigned int *b) {
    while (*a && *a == *b) { a++; b++; }
    return (*a > *b) - (*a < *b);
}

int wcscoll(const unsigned int *a, const unsigned int *b) {
    return wcscmp(a, b);
}

unsigned int wcsxfrm(unsigned int *dst, const unsigned int *src, unsigned int n) {
    unsigned int len = wcslen(src);
    if (n) {
        unsigned int c = len < n - 1 ? len : n - 1;
        for (unsigned int i = 0; i < c; i++) dst[i] = src[i];
        dst[c] = 0;
    }
    return len;
}

/* "C" locale collation is byte order, so strcoll() is strcmp(). */
int strcoll(const char *a, const char *b) {
    return strcmp(a, b);
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *s = str ? str : *saveptr;
    if (!s)
        return 0;
    while (*s && strchr(delim, *s))          /* skip leading delimiters */
        s++;
    if (!*s) {
        *saveptr = 0;
        return 0;
    }
    char *tok = s;
    while (*s && !strchr(delim, *s))         /* find the end of the token */
        s++;
    if (*s) {
        *s++ = 0;
        *saveptr = s;
    } else {
        *saveptr = 0;
    }
    return tok;
}
