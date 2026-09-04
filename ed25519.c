/**
 * @file ed25519.c
 * @brief Ed25519 (RFC 8032): extended-coordinate twisted Edwards point
 *        arithmetic over bignum256, SHA-512 for the hash-based key/nonce
 *        derivation.
 */
#include <ed25519.h>
#include <bignum256.h>
#include <sha2.h>
#include <lib/string.h>

/* p = 2^255 - 19. */
static const uint8_t P_BE[32] = {
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xed
};
/* L, the order of the base point's prime-order subgroup. */
static const uint8_t L_BE[32] = {
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x14, 0xde, 0xf9, 0xde, 0xa2, 0xf7, 0x9c, 0xd6,
    0x58, 0x12, 0x63, 0x1a, 0x5c, 0xf5, 0xd3, 0xed
};
/* d = -121665/121666 mod p, the twisted Edwards curve parameter. */
static const uint8_t D_BE[32] = {
    0x52, 0x03, 0x6c, 0xee, 0x2b, 0x6f, 0xfe, 0x73, 0x8c, 0xc7, 0x40, 0x79,
    0x77, 0x79, 0xe8, 0x98, 0x00, 0x70, 0x0a, 0x4d, 0x41, 0x41, 0xd8, 0xab,
    0x75, 0xeb, 0x4d, 0xca, 0x13, 0x59, 0x78, 0xa3
};
/* Base point B = (Bx, By). */
static const uint8_t BX_BE[32] = {
    0x21, 0x69, 0x36, 0xd3, 0xcd, 0x6e, 0x53, 0xfe, 0xc0, 0xa4, 0xe2, 0x31,
    0xfd, 0xd6, 0xdc, 0x5c, 0x69, 0x2c, 0xc7, 0x60, 0x95, 0x25, 0xa7, 0xb2,
    0xc9, 0x56, 0x2d, 0x60, 0x8f, 0x25, 0xd5, 0x1a
};
static const uint8_t BY_BE[32] = {
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x58
};

/** Point in extended coordinates: x = X/Z, y = Y/Z, x*y = T/Z. */
typedef struct { u256 X, Y, Z, T; } ed_point;

static void point_identity(ed_point *r) {
    u256_zero(&r->X);
    u256_set_u32(&r->Y, 1);
    u256_set_u32(&r->Z, 1);
    u256_zero(&r->T);
}

static void point_base(ed_point *r, const u256 *p) {
    u256_from_bytes_be(&r->X, BX_BE, 32);
    u256_from_bytes_be(&r->Y, BY_BE, 32);
    u256_set_u32(&r->Z, 1);
    u256_mul_mod(&r->T, &r->X, &r->Y, p);
}

/**
 * @brief r = p1 + p2 ("add-2008-hwcd-3", unified: also correct for p1 == p2).
 * @param d2 2*d mod p, precomputed by the caller.
 */
static void point_add(ed_point *r, const ed_point *p1, const ed_point *p2,
                      const u256 *p, const u256 *d2) {
    u256 y1mx1, y2mx2, y1px1, y2px2, A, B, t1t2, C, z1z2, D, E, F, G, H;

    u256_sub_mod(&y1mx1, &p1->Y, &p1->X, p);
    u256_sub_mod(&y2mx2, &p2->Y, &p2->X, p);
    u256_mul_mod(&A, &y1mx1, &y2mx2, p);

    u256_add_mod(&y1px1, &p1->Y, &p1->X, p);
    u256_add_mod(&y2px2, &p2->Y, &p2->X, p);
    u256_mul_mod(&B, &y1px1, &y2px2, p);

    u256_mul_mod(&t1t2, &p1->T, &p2->T, p);
    u256_mul_mod(&C, &t1t2, d2, p);

    u256_mul_mod(&z1z2, &p1->Z, &p2->Z, p);
    u256_add_mod(&D, &z1z2, &z1z2, p);

    u256_sub_mod(&E, &B, &A, p);
    u256_sub_mod(&F, &D, &C, p);
    u256_add_mod(&G, &D, &C, p);
    u256_add_mod(&H, &B, &A, p);

    u256_mul_mod(&r->X, &E, &F, p);
    u256_mul_mod(&r->Y, &G, &H, p);
    u256_mul_mod(&r->T, &E, &H, p);
    u256_mul_mod(&r->Z, &F, &G, p);
}

/** @brief r = scalar * base, scalar as a 32-byte little-endian integer. */
static void point_scalarmult(ed_point *r, const uint8_t scalar[32], const ed_point *base,
                             const u256 *p, const u256 *d2) {
    ed_point acc;
    point_identity(&acc);
    for (int t = 255; t >= 0; t--) {
        ed_point dbl;
        point_add(&dbl, &acc, &acc, p, d2);
        acc = dbl;
        if ((scalar[t / 8] >> (t % 8)) & 1) {
            ed_point sum;
            point_add(&sum, &acc, base, p, d2);
            acc = sum;
        }
    }
    *r = acc;
}

static void point_encode(uint8_t out[32], const ed_point *pt, const u256 *p) {
    u256 zinv, x, y;
    u256_inv_mod(&zinv, &pt->Z, p);
    u256_mul_mod(&x, &pt->X, &zinv, p);
    u256_mul_mod(&y, &pt->Y, &zinv, p);
    u256_to_bytes_le(out, &y);
    out[31] = (uint8_t)((out[31] & 0x7F) | ((u256_bit(&x, 0) ? 1 : 0) << 7));
}

/** @brief Reverse 64 bytes (hash digests are big-endian; RFC 8032 scalars are little-endian integers). */
static void reverse64(uint8_t out[64], const uint8_t in[64]) {
    for (int i = 0; i < 64; i++)
        out[i] = in[63 - i];
}

void ed25519_derive(const uint8_t seed[32], uint8_t pubkey_out[32],
                     uint8_t scalar_out[32], uint8_t prefix_out[32]) {
    uint8_t h[64];
    sha512(seed, 32, h);
    memcpy(scalar_out, h, 32);
    scalar_out[0]  &= 248;
    scalar_out[31] &= 127;
    scalar_out[31] |= 64;
    memcpy(prefix_out, h + 32, 32);

    u256 p, d, d2;
    u256_from_bytes_be(&p, P_BE, 32);
    u256_from_bytes_be(&d, D_BE, 32);
    u256_add_mod(&d2, &d, &d, &p);

    ed_point base, A;
    point_base(&base, &p);
    point_scalarmult(&A, scalar_out, &base, &p, &d2);
    point_encode(pubkey_out, &A, &p);
}

void ed25519_sign(const uint8_t scalar[32], const uint8_t prefix[32],
                   const uint8_t pubkey[32], const uint8_t *msg, uint32_t msglen,
                   uint8_t sig_out[64]) {
    u256 p, d, d2, L;
    u256_from_bytes_be(&p, P_BE, 32);
    u256_from_bytes_be(&d, D_BE, 32);
    u256_add_mod(&d2, &d, &d, &p);
    u256_from_bytes_be(&L, L_BE, 32);

    ed_point base;
    point_base(&base, &p);

    /* r = SHA512(prefix || msg) mod L */
    sha512_ctx c;
    uint8_t rh[64], rh_be[64];
    sha512_init(&c);
    sha512_update(&c, prefix, 32);
    sha512_update(&c, (void *)msg, msglen);
    sha512_final(&c, rh);
    reverse64(rh_be, rh);
    u256 r;
    bytes_be_mod(&r, rh_be, 64, &L);

    /* R = r*B, encoded */
    uint8_t r_bytes[32], R_enc[32];
    u256_to_bytes_le(r_bytes, &r);
    ed_point Rpt;
    point_scalarmult(&Rpt, r_bytes, &base, &p, &d2);
    point_encode(R_enc, &Rpt, &p);

    /* k = SHA512(R || A || msg) mod L */
    uint8_t kh[64], kh_be[64];
    sha512_init(&c);
    sha512_update(&c, R_enc, 32);
    sha512_update(&c, pubkey, 32);
    sha512_update(&c, (void *)msg, msglen);
    sha512_final(&c, kh);
    reverse64(kh_be, kh);
    u256 k;
    bytes_be_mod(&k, kh_be, 64, &L);

    /* S = (r + k*s) mod L */
    u256 s_u, ks, S;
    u256_from_bytes_le(&s_u, scalar);
    u256_mul_mod(&ks, &k, &s_u, &L);
    u256_add_mod(&S, &r, &ks, &L);

    memcpy(sig_out, R_enc, 32);
    uint8_t S_bytes[32];
    u256_to_bytes_le(S_bytes, &S);
    memcpy(sig_out + 32, S_bytes, 32);
}
