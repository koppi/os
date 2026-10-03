/*
 * sscanf for this port.
 *
 * lib/libc_ext.c has one, but it handles only %d/%i/%u/%s -- enough for the
 * Qt closure it was written for, not for ChipNomad, which parses its whole
 * project format with sscanf and needs more:
 *
 *   %x %X %o       hex register values, "0x%x" (20 call sites)
 *   %f             tick rate and mix volume, written and read back
 *   hh, h          "- Tone on: %hhu" stores through a uint8_t*; writing four
 *                  bytes there would quietly corrupt the next field
 *   %[^\n]         project and sample titles
 *   literals       "- Volume envelope: %hhu,%hhu,%hhu,%hhu" matches text and
 *                  separators between conversions
 *
 * Conversions are whitespace-skipping and width-limited as the standard
 * requires, and the return value is the number of items assigned (or EOF if
 * the input ended before the first one), which is what every caller tests.
 * %n, %p, the apostrophe flag and the wide-character forms are not here:
 * nothing in this tree uses them.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

enum len_mod { LEN_HH, LEN_H, LEN_INT, LEN_L, LEN_LL };

static int is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

static int digit_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

/* Store @p v through a pointer of the width @p mod describes. */
static void store_signed(va_list *ap, enum len_mod mod, long long v) {
    switch (mod) {
    case LEN_HH: *va_arg(*ap, signed char *) = (signed char) v; break;
    case LEN_H:  *va_arg(*ap, short *)       = (short) v;       break;
    case LEN_L:  *va_arg(*ap, long *)        = (long) v;        break;
    case LEN_LL: *va_arg(*ap, long long *)   = v;               break;
    default:     *va_arg(*ap, int *)         = (int) v;         break;
    }
}

static void store_unsigned(va_list *ap, enum len_mod mod, unsigned long long v) {
    switch (mod) {
    case LEN_HH: *va_arg(*ap, unsigned char *)  = (unsigned char) v;  break;
    case LEN_H:  *va_arg(*ap, unsigned short *) = (unsigned short) v; break;
    case LEN_L:  *va_arg(*ap, unsigned long *)  = (unsigned long) v;  break;
    case LEN_LL: *va_arg(*ap, unsigned long long *) = v;              break;
    default:     *va_arg(*ap, unsigned int *)   = (unsigned int) v;   break;
    }
}

int koppios_vsscanf(const char *str, const char *fmt, va_list ap) __asm__("vsscanf");
int koppios_vsscanf(const char *str, const char *fmt, va_list ap)
{
    int assigned = 0;
    int seen_input = 0;
    const char *s = str;

    for (const char *f = fmt; *f; f++) {
        if (is_space((unsigned char) *f)) {
            while (is_space((unsigned char) *s)) s++;
            continue;
        }

        if (*f != '%') {
            if (*s != *f)
                return assigned ? assigned : (seen_input ? 0 : EOF);
            s++; seen_input = 1;
            continue;
        }

        f++;
        if (*f == '%') {
            while (is_space((unsigned char) *s)) s++;
            if (*s != '%') return assigned;
            s++;
            continue;
        }

        int assign = 1;
        if (*f == '*') { assign = 0; f++; }

        int width = 0;
        while (*f >= '0' && *f <= '9')
            width = width * 10 + (*f++ - '0');

        enum len_mod mod = LEN_INT;
        if (*f == 'h') { f++; mod = LEN_H;  if (*f == 'h') { f++; mod = LEN_HH; } }
        else if (*f == 'l') { f++; mod = LEN_L; if (*f == 'l') { f++; mod = LEN_LL; } }
        else if (*f == 'z' || *f == 'j' || *f == 't') { f++; mod = LEN_L; }
        else if (*f == 'L') { f++; mod = LEN_LL; }

        switch (*f) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': {
            int base = (*f == 'd' || *f == 'u') ? 10
                     : (*f == 'i') ? 0
                     : (*f == 'o') ? 8 : 16;
            int is_signed = (*f == 'd' || *f == 'i');

            while (is_space((unsigned char) *s)) s++;
            if (!*s) return assigned ? assigned : EOF;

            const char *p = s;
            int used = 0, neg = 0;
            int lim = width > 0 ? width : 0x7FFFFFFF;

            if (used < lim && (*p == '+' || *p == '-')) {
                neg = (*p == '-'); p++; used++;
            }
            if ((base == 0 || base == 16) && used + 1 < lim && p[0] == '0' &&
                (p[1] == 'x' || p[1] == 'X') && digit_val((unsigned char) p[2]) >= 0 &&
                digit_val((unsigned char) p[2]) < 16) {
                p += 2; used += 2; base = 16;
            } else if (base == 0) {
                base = (p[0] == '0') ? 8 : 10;
            }

            unsigned long long v = 0;
            int digits = 0;
            while (used < lim) {
                int d = digit_val((unsigned char) *p);
                if (d < 0 || d >= base) break;
                v = v * (unsigned long long) base + (unsigned long long) d;
                p++; used++; digits++;
            }
            if (digits == 0)
                return assigned;

            s = p; seen_input = 1;
            if (assign) {
                if (is_signed)
                    store_signed(&ap, mod, neg ? -(long long) v : (long long) v);
                else
                    store_unsigned(&ap, mod, neg ? (unsigned long long) -(long long) v : v);
                assigned++;
            }
            break;
        }

        case 'f': case 'e': case 'E': case 'g': case 'G': case 'a': {
            while (is_space((unsigned char) *s)) s++;
            if (!*s) return assigned ? assigned : EOF;

            /* strtod (lib/libc_ext.c has the real one) does the conversion;
             * a field width is honoured by copying out that many characters
             * first, which no call site here actually uses. */
            char tmp[64];
            const char *src = s;
            if (width > 0) {
                int i = 0;
                while (i < width && i < (int) sizeof tmp - 1 && src[i])
                    tmp[i] = src[i], i++;
                tmp[i] = 0;
                src = tmp;
            }

            char *end = 0;
            double d = strtod(src, &end);
            if (!end || end == src)
                return assigned;

            s += (end - src); seen_input = 1;
            if (assign) {
                if (mod == LEN_L || mod == LEN_LL)
                    *va_arg(ap, double *) = d;
                else
                    *va_arg(ap, float *) = (float) d;
                assigned++;
            }
            break;
        }

        case 'c': {
            int n = width > 0 ? width : 1;
            char *out = assign ? va_arg(ap, char *) : 0;
            for (int i = 0; i < n; i++) {
                if (!*s) return (i || assigned) ? assigned : EOF;
                if (out) out[i] = *s;
                s++;
            }
            seen_input = 1;
            if (assign) assigned++;
            break;
        }

        case 's': {
            while (is_space((unsigned char) *s)) s++;
            if (!*s) return assigned ? assigned : EOF;
            char *out = assign ? va_arg(ap, char *) : 0;
            int n = 0;
            int lim = width > 0 ? width : 0x7FFFFFFF;
            while (*s && !is_space((unsigned char) *s) && n < lim) {
                if (out) out[n] = *s;
                s++; n++;
            }
            if (out) out[n] = 0;
            seen_input = 1;
            if (assign) assigned++;
            break;
        }

        case '[': {
            /* Scanset: %[abc], %[^\n], ranges a-z, and ']' first meaning a
             * literal bracket. */
            f++;
            int negate = 0;
            if (*f == '^') { negate = 1; f++; }

            unsigned char set[256] = {0};
            if (*f == ']') { set[(unsigned char) ']'] = 1; f++; }
            while (*f && *f != ']') {
                if (f[1] == '-' && f[2] && f[2] != ']' &&
                    (unsigned char) f[2] >= (unsigned char) f[0]) {
                    for (int c = (unsigned char) f[0]; c <= (unsigned char) f[2]; c++)
                        set[c] = 1;
                    f += 3;
                } else {
                    set[(unsigned char) *f] = 1;
                    f++;
                }
            }
            if (!*f) return assigned;   /* unterminated scanset */

            char *out = assign ? va_arg(ap, char *) : 0;
            int n = 0;
            int lim = width > 0 ? width : 0x7FFFFFFF;
            while (*s && n < lim) {
                int in_set = set[(unsigned char) *s] != 0;
                if (negate ? in_set : !in_set) break;
                if (out) out[n] = *s;
                s++; n++;
            }
            if (n == 0)
                return assigned ? assigned : (seen_input ? 0 : EOF);
            if (out) out[n] = 0;
            seen_input = 1;
            if (assign) assigned++;
            break;
        }

        default:
            /* An unknown conversion: stop rather than guess at the argument
             * width and walk off the va_list. */
            return assigned;
        }
    }

    return assigned;
}

/*
 * Both spellings, defined with explicit assembler names.
 *
 * glibc's <stdio.h> redirects sscanf to __isoc23_sscanf when C23 scanf
 * semantics are selected (they are, with these headers), so a plain
 * `int sscanf(...)` definition here would be emitted *as* __isoc23_sscanf
 * and collide with the second one. Naming the symbols directly sidesteps the
 * redirect and defines both, so an object compiled either way links.
 */
static int sscanf_impl(const char *str, const char *fmt, va_list ap) {
    return koppios_vsscanf(str, fmt, ap);
}

int koppios_sscanf(const char *str, const char *fmt, ...) __asm__("sscanf");
int koppios_sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = sscanf_impl(str, fmt, ap);
    va_end(ap);
    return n;
}

int koppios_isoc23_sscanf(const char *str, const char *fmt, ...) __asm__("__isoc23_sscanf");
int koppios_isoc23_sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = sscanf_impl(str, fmt, ap);
    va_end(ap);
    return n;
}
