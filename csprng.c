/**
 * @file csprng.c
 * @brief See csprng.h.
 */
#include <csprng.h>
#include <aes128.h>
#include <sha2.h>
#include <rtc.h>
#include <pit.h>
#include <lib/string.h>
#include "cpu.h"

static aes128_ctx ctx;
static uint8_t counter[16];
static int ready;

static void reseed(void) {
    uint8_t material[32];
    uint64_t t1 = rdtsc();
    for (volatile int i = 0; i < 997; i++)   /* burn a little time for more jitter */
        ;
    uint64_t t2 = rdtsc();
    uint32_t rtc = rtc_now_unix();
    uint32_t ms  = pit_ms();

    memcpy(material,      &t1,  8);
    memcpy(material + 8,  &t2,  8);
    memcpy(material + 16, &rtc, 4);
    memcpy(material + 20, &ms,  4);
    memcpy(material + 24, &t1,  8);          /* pad to 32 bytes */

    uint8_t h[32];
    sha256(material, 32, h);
    aes128_init(&ctx, h);
    memcpy(counter, h + 16, 16);
    ready = 1;
}

void csprng_bytes(uint8_t *out, int len) {
    if (!ready)
        reseed();

    int off = 0;
    while (off < len) {
        uint8_t block[16];
        aes128_encrypt_block(&ctx, counter, block);
        for (int i = 15; i >= 0; i--)
            if (++counter[i] != 0)
                break;
        int n = len - off;
        if (n > 16) n = 16;
        memcpy(out + off, block, n);
        off += n;
    }

    /* Fold in fresh timing jitter so state keeps moving between calls. */
    uint64_t t = rdtsc();
    for (int i = 0; i < 8; i++)
        counter[i] ^= (uint8_t)(t >> (i * 8));
}
