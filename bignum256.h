/**
 * @file bignum256.h
 * @brief Minimal fixed-width 256-bit unsigned integer arithmetic.
 *
 * Backs Curve25519 / Ed25519 field (mod p = 2^255-19) and scalar (mod L, the
 * Ed25519 group order) arithmetic. Deliberately simple over fast: modular
 * reduction is a generic binary long division, valid for any modulus @c m
 * with @c m < 2^255 (true for both p and L) — there is no fast reduction
 * exploiting the special shape of p. Curve25519/Ed25519 only run once per SSH
 * session, so this trades a modest amount of CPU time for far less code to
 * get wrong.
 */
#pragma once

#include <types.h>

/** 256-bit unsigned integer, little-endian words (w[0] = bits 0..31). */
typedef struct { uint32_t w[8]; } u256;

/** 512-bit unsigned integer, little-endian words. Holds a 256x256 product. */
typedef struct { uint32_t w[16]; } u512;

void u256_zero(u256 *r);
void u256_set_u32(u256 *r, uint32_t v);
void u256_from_bytes_le(u256 *r, const uint8_t b[32]);
void u256_to_bytes_le(uint8_t b[32], const u256 *a);
/** Big-endian byte i/o, used for the SSH mpint wire encoding. */
void u256_from_bytes_be(u256 *r, const uint8_t *b, int len);
void u256_to_bytes_be(uint8_t b[32], const u256 *a);

int u256_is_zero(const u256 *a);
int u256_bit(const u256 *a, int i);
/** @return -1 / 0 / 1 as a < b / a == b / a > b. */
int u256_cmp(const u256 *a, const u256 *b);

/** r = a + b (mod 2^256). @return carry out (0 or 1). */
uint32_t u256_add(u256 *r, const u256 *a, const u256 *b);
/** r = a - b (mod 2^256). @return borrow out (0 or 1). */
uint32_t u256_sub(u256 *r, const u256 *a, const u256 *b);
/** r (512-bit) = a * b. */
void u256_mul(u512 *r, const u256 *a, const u256 *b);

/**
 * @brief r = x mod m, via binary long division.
 * @pre m < 2^255 (top bit of m->w[7] clear) so the running remainder never
 *      needs more than 256 bits.
 */
void u256_mod(u256 *r, const u512 *x, const u256 *m);

/** r = (a + b) mod m. @pre a < m, b < m, m < 2^255. */
void u256_add_mod(u256 *r, const u256 *a, const u256 *b, const u256 *m);
/** r = (a - b) mod m. @pre a < m, b < m. */
void u256_sub_mod(u256 *r, const u256 *a, const u256 *b, const u256 *m);
/** r = (a * b) mod m. @pre m < 2^255. */
void u256_mul_mod(u256 *r, const u256 *a, const u256 *b, const u256 *m);
/** r = (base ^ exp) mod m, square-and-multiply. */
void u256_pow_mod(u256 *r, const u256 *base, const u256 *exp, const u256 *m);
/** r = a^-1 mod m, via Fermat (r = a^(m-2) mod m) — only valid for prime m. */
void u256_inv_mod(u256 *r, const u256 *a, const u256 *m);

/** r = x mod m for an arbitrary-length big-endian byte string (x may be
 *  longer than 512 bits, e.g. nothing here needs it yet, but @p len <= 64
 *  covers a SHA-512 digest). @pre m < 2^255. */
void bytes_be_mod(u256 *r, const uint8_t *x, int len, const u256 *m);
