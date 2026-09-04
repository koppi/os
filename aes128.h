/**
 * @file aes128.h
 * @brief AES-128, forward cipher only (FIPS-197) plus a CTR-mode stream
 *        helper. No decryption round functions exist — CTR mode only ever
 *        needs the forward block cipher to build a keystream.
 */
#pragma once

#include <types.h>

typedef struct {
    uint32_t rk[44];      /**< 11 round keys x 4 words. */
} aes128_ctx;

void aes128_init(aes128_ctx *ctx, const uint8_t key[16]);
/** @brief Encrypt one 16-byte block. */
void aes128_encrypt_block(const aes128_ctx *ctx, const uint8_t in[16], uint8_t out[16]);

/**
 * @brief AES-128-CTR keystream XOR (SSH's aes128-ctr): @p counter_block is a
 *        16-byte big-endian counter, incremented by one per 16-byte block
 *        (RFC 3686 / RFC 4344), and is updated in place so the caller can
 *        resume the stream across multiple calls.
 */
void aes128_ctr_xor(const aes128_ctx *ctx, uint8_t counter_block[16],
                     const uint8_t *in, uint8_t *out, uint32_t len);
