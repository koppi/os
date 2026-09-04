/**
 * @file curve25519.c
 * @brief X25519 (RFC 7748), Montgomery ladder over bignum256's mod-p ops.
 */
#include <curve25519.h>
#include <bignum256.h>
#include <lib/string.h>

const uint8_t curve25519_base_u[32] = { 9 };

/* p = 2^255 - 19, big-endian. */
static const uint8_t P_BE[32] = {
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xed
};

static void cswap(uint32_t swap, u256 *a, u256 *b) {
    uint32_t mask = (uint32_t)(-(int32_t)swap);
    for (int i = 0; i < 8; i++) {
        uint32_t t = mask & (a->w[i] ^ b->w[i]);
        a->w[i] ^= t;
        b->w[i] ^= t;
    }
}

void curve25519_x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t u_in[32]) {
    u256 p;
    u256_from_bytes_be(&p, P_BE, 32);

    uint8_t k[32];
    memcpy(k, (void *)scalar, 32);
    k[0]  &= 248;
    k[31] &= 127;
    k[31] |= 64;

    uint8_t u_masked[32];
    memcpy(u_masked, (void *)u_in, 32);
    u_masked[31] &= 0x7F;         /* RFC 7748: mask the top bit of the u-coordinate */

    u256 x1;
    u256_from_bytes_le(&x1, u_masked);
    if (u256_cmp(&x1, &p) >= 0) {
        u512 t;
        memset(&t, 0, sizeof t);
        memcpy(t.w, x1.w, sizeof x1.w);
        u256_mod(&x1, &t, &p);
    }

    u256 x2, z2, x3, z3;
    u256_set_u32(&x2, 1);
    u256_zero(&z2);
    x3 = x1;
    u256_set_u32(&z3, 1);

    u256 a24;
    u256_set_u32(&a24, 121665);

    uint32_t swap = 0;
    for (int t = 254; t >= 0; t--) {
        uint32_t kt = (k[t / 8] >> (t % 8)) & 1;
        swap ^= kt;
        cswap(swap, &x2, &x3);
        cswap(swap, &z2, &z3);
        swap = kt;

        u256 A, AA, B, BB, E, C, D, DA, CB, t1, t2;
        u256_add_mod(&A, &x2, &z2, &p);
        u256_mul_mod(&AA, &A, &A, &p);
        u256_sub_mod(&B, &x2, &z2, &p);
        u256_mul_mod(&BB, &B, &B, &p);
        u256_sub_mod(&E, &AA, &BB, &p);
        u256_add_mod(&C, &x3, &z3, &p);
        u256_sub_mod(&D, &x3, &z3, &p);
        u256_mul_mod(&DA, &D, &A, &p);
        u256_mul_mod(&CB, &C, &B, &p);

        u256_add_mod(&t1, &DA, &CB, &p);
        u256_mul_mod(&x3, &t1, &t1, &p);
        u256_sub_mod(&t2, &DA, &CB, &p);
        u256_mul_mod(&t2, &t2, &t2, &p);
        u256_mul_mod(&z3, &x1, &t2, &p);

        u256_mul_mod(&x2, &AA, &BB, &p);
        u256 a24E, sum;
        u256_mul_mod(&a24E, &a24, &E, &p);
        u256_add_mod(&sum, &AA, &a24E, &p);
        u256_mul_mod(&z2, &E, &sum, &p);
    }
    cswap(swap, &x2, &x3);
    cswap(swap, &z2, &z3);

    u256 zinv, res;
    u256_inv_mod(&zinv, &z2, &p);
    u256_mul_mod(&res, &x2, &zinv, &p);
    u256_to_bytes_le(out, &res);
}
