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

int __cxa_thread_atexit(void (*function)(void *), void *arg, void *dso) {
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

/* No real environment-variable storage on this kernel (same choice
 * third_party/qt6/koppios/qt_libc_compat.c already made). */
char *getenv(const char *name) {
    (void) name;
    return 0;
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
 * matters. */
void qsort(void *base, unsigned int nmemb, unsigned int size, int (*compar)(const void *, const void *)) {
    char *arr = (char *) base;
    char tmp[256];
    for (unsigned int i = 1; i < nmemb; i++) {
        unsigned int j = i;
        while (j > 0 && compar(arr + (j - 1) * size, arr + j * size) > 0) {
            memcpy(tmp, arr + j * size, size);
            memcpy(arr + j * size, arr + (j - 1) * size, size);
            memcpy(arr + (j - 1) * size, tmp, size);
            j--;
        }
    }
}

/* ---- locale / timezone: this kernel is always UTC, always the "C"
 * locale -- same reasoning third_party/qt6/koppios/qt_libc_compat.c's
 * own file comment already documents for its smaller closure. ---- */

char *setlocale(int category, const char *locale) {
    (void) category;
    (void) locale;
    return (char *) "C";
}

char *nl_langinfo(int item) {
    (void) item;
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
