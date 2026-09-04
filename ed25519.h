/**
 * @file ed25519.h
 * @brief Ed25519 (RFC 8032) key generation and signing, built on bignum256.
 *
 * Only what an SSH host key needs: derive a public key from a 32-byte seed,
 * and sign a message. There is no verify path (the server never checks an
 * Ed25519 signature — user auth here is password-only).
 */
#pragma once

#include <types.h>

/**
 * @brief Derive the public key, the clamped private scalar and the signing
 *        prefix from a 32-byte seed (RFC 8032 §5.1.5).
 *
 * @p scalar_out and @p prefix_out feed directly into @ref ed25519_sign, so
 * they only need computing once per host key (e.g. at boot).
 */
void ed25519_derive(const uint8_t seed[32], uint8_t pubkey_out[32],
                     uint8_t scalar_out[32], uint8_t prefix_out[32]);

/** @brief Sign @p msg (RFC 8032 §5.1.6), given the values from @ref ed25519_derive. */
void ed25519_sign(const uint8_t scalar[32], const uint8_t prefix[32],
                   const uint8_t pubkey[32], const uint8_t *msg, uint32_t msglen,
                   uint8_t sig_out[64]);
