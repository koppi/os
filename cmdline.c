/**
 * @file cmdline.c
 * @brief Storage + lookup for the bootloader command line (see cmdline.h).
 */
#include <cmdline.h>
#include <types.h>

#include <lib/string.h>

char kernel_cmdline[256];

int cmdline_has(const char *word) {
    size_t wl = strlen(word);
    if (wl == 0)
        return 0;
    const char *p = kernel_cmdline;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        if ((size_t) (p - start) == wl &&
            strncmp((char *) start, (char *) word, wl) == 0)
            return 1;
    }
    return 0;
}
