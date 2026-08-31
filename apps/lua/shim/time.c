/* Civil-time helpers for the koppi-os Lua port. The RTC keeps UTC, so
 * localtime == gmtime. Algorithms: Howard Hinnant's days<->civil. */
#include <time.h>
#include "ksys.h"

time_t time(time_t *t) {
    time_t now = (time_t) ksys0(SYS_TIME);
    if (t) *t = now;
    return now;
}

clock_t clock(void) { return (clock_t) ksys0(SYS_CLOCK); }

double difftime(time_t a, time_t b) { return (double) (a - b); }

/* days since 1970-01-01 for a given y/m/d (m in 1..12). */
static long days_from_civil(long y, unsigned m, unsigned d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned) (y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + (long) doe - 719468L;
}

static void civil_from_days(long z, int *y, int *m, int *d) {
    z += 719468L;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned) (z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long yr = (long) yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = (int) (doy - (153 * mp + 2) / 5 + 1);
    *m = (int) (mp + (mp < 10 ? 3 : -9));
    *y = (int) (yr + (*m <= 2));
}

static struct tm tmbuf;

struct tm *gmtime(const time_t *tp) {
    time_t t = *tp;
    long days = t / 86400;
    long rem = t % 86400;
    if (rem < 0) { rem += 86400; days -= 1; }

    tmbuf.tm_hour = (int) (rem / 3600);
    tmbuf.tm_min = (int) (rem % 3600 / 60);
    tmbuf.tm_sec = (int) (rem % 60);
    tmbuf.tm_wday = (int) ((days % 7 + 4 + 7) % 7);   /* 1970-01-01 was Thursday */

    int y, m, d;
    civil_from_days(days, &y, &m, &d);
    tmbuf.tm_year = y - 1900;
    tmbuf.tm_mon = m - 1;
    tmbuf.tm_mday = d;
    tmbuf.tm_yday = (int) (days - days_from_civil(y, 1, 1));
    tmbuf.tm_isdst = 0;
    return &tmbuf;
}

struct tm *localtime(const time_t *t) { return gmtime(t); }

time_t mktime(struct tm *tm) {
    long days = days_from_civil(tm->tm_year + 1900, (unsigned) tm->tm_mon + 1,
                                (unsigned) tm->tm_mday);
    time_t t = (time_t) days * 86400 + tm->tm_hour * 3600 +
               tm->tm_min * 60 + tm->tm_sec;
    struct tm *n = gmtime(&t);           /* normalise the output fields */
    *tm = *n;
    return t;
}

static const char *const wday[] = {"Sunday","Monday","Tuesday","Wednesday",
                                   "Thursday","Friday","Saturday"};
static const char *const mon[] = {"January","February","March","April","May",
    "June","July","August","September","October","November","December"};

static char *put(char *p, char *end, const char *s) {
    while (*s && p < end) *p++ = *s++;
    return p;
}
static char *putn(char *p, char *end, int v, int w) {
    char t[12]; int i = 0, neg = v < 0;
    unsigned u = neg ? (unsigned) -v : (unsigned) v;
    do { t[i++] = (char) ('0' + u % 10); u /= 10; } while (u);
    while (i < w) t[i++] = '0';
    if (neg && p < end) *p++ = '-';
    while (i && p < end) *p++ = t[--i];
    return p;
}

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm) {
    char *p = s, *end = s + (max ? max - 1 : 0);
    for (; *fmt && p < end; fmt++) {
        if (*fmt != '%') { *p++ = *fmt; continue; }
        switch (*++fmt) {
            case 'Y': p = putn(p, end, tm->tm_year + 1900, 1); break;
            case 'y': p = putn(p, end, (tm->tm_year + 1900) % 100, 2); break;
            case 'm': p = putn(p, end, tm->tm_mon + 1, 2); break;
            case 'd': p = putn(p, end, tm->tm_mday, 2); break;
            case 'H': p = putn(p, end, tm->tm_hour, 2); break;
            case 'M': p = putn(p, end, tm->tm_min, 2); break;
            case 'S': p = putn(p, end, tm->tm_sec, 2); break;
            case 'I': { int h = tm->tm_hour % 12; if (!h) h = 12;
                        p = putn(p, end, h, 2); break; }
            case 'p': p = put(p, end, tm->tm_hour < 12 ? "AM" : "PM"); break;
            case 'j': p = putn(p, end, tm->tm_yday + 1, 3); break;
            case 'w': p = putn(p, end, tm->tm_wday, 1); break;
            case 'a': { const char *wn = wday[tm->tm_wday % 7];
                        for (int k = 0; k < 3 && p < end; k++) *p++ = wn[k];
                        break; }
            case 'A': p = put(p, end, wday[tm->tm_wday % 7]); break;
            case 'b':
            case 'h': { const char *mn = mon[tm->tm_mon % 12];
                        for (int k = 0; k < 3 && p < end; k++) *p++ = mn[k];
                        break; }
            case 'B': p = put(p, end, mon[tm->tm_mon % 12]); break;
            case 'Z': p = put(p, end, "UTC"); break;
            case 'z': p = put(p, end, "+0000"); break;
            case 'x': p = putn(p, end, tm->tm_mon + 1, 2);
                      if (p < end) *p++ = '/';
                      p = putn(p, end, tm->tm_mday, 2);
                      if (p < end) *p++ = '/';
                      p = putn(p, end, (tm->tm_year + 1900) % 100, 2); break;
            case 'X': p = putn(p, end, tm->tm_hour, 2);
                      if (p < end) *p++ = ':';
                      p = putn(p, end, tm->tm_min, 2);
                      if (p < end) *p++ = ':';
                      p = putn(p, end, tm->tm_sec, 2); break;
            case 'c': {
                char *q = p;
                for (int k = 0; k < 3 && q < end; k++) *q++ = wday[tm->tm_wday % 7][k];
                if (q < end) *q++ = ' ';
                for (int k = 0; k < 3 && q < end; k++) *q++ = mon[tm->tm_mon % 12][k];
                if (q < end) *q++ = ' ';
                q = putn(q, end, tm->tm_mday, 2);
                if (q < end) *q++ = ' ';
                q = putn(q, end, tm->tm_hour, 2);
                if (q < end) *q++ = ':';
                q = putn(q, end, tm->tm_min, 2);
                if (q < end) *q++ = ':';
                q = putn(q, end, tm->tm_sec, 2);
                if (q < end) *q++ = ' ';
                q = putn(q, end, tm->tm_year + 1900, 4);
                p = q; break;
            }
            case '%': if (p < end) *p++ = '%'; break;
            default: break;
        }
    }
    *p = 0;
    return (size_t) (p - s);
}
