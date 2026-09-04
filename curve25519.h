/**
 * @file curve25519.h
 * @brief X25519 (RFC 7748) — Curve25519 Diffie-Hellman, built on bignum256's
 *        generic mod-p arithmetic via the Montgomery ladder.
 */
#pragma once

#include <types.h>

/**
 * @brief X25519(scalar, u_in) -> u_out, all 32-byte little-endian.
 *
 * @p scalar is clamped internally per RFC 7748 (the caller may pass raw
 * random bytes). Pass @ref CURVE25519_BASE_U as @p u_in to compute a public
 * key from a private scalar.
 */
void curve25519_x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t u_in[32]);

/** The base point's u-coordinate (9), little-endian, for deriving public keys. */
extern const uint8_t curve25519_base_u[32];
