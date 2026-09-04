/**
 * @file sha2.h
 * @brief SHA-256 and SHA-512 (FIPS 180-4), incremental API.
 */
#pragma once

#include <types.h>

typedef struct {
    uint32_t h[8];
    uint64_t len;        /**< Total message length, in bytes. */
    uint8_t  buf[64];
    int      buf_len;
} sha256_ctx;

void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const void *data, uint32_t len);
void sha256_final(sha256_ctx *c, uint8_t out[32]);
/** @brief One-shot convenience wrapper. */
void sha256(const void *data, uint32_t len, uint8_t out[32]);

typedef struct {
    uint64_t h[8];
    uint64_t len;         /**< Total message length, in bytes. */
    uint8_t  buf[128];
    int      buf_len;
} sha512_ctx;

void sha512_init(sha512_ctx *c);
void sha512_update(sha512_ctx *c, const void *data, uint32_t len);
void sha512_final(sha512_ctx *c, uint8_t out[64]);
void sha512(const void *data, uint32_t len, uint8_t out[64]);

/** @brief HMAC-SHA-256 (RFC 2104), one-shot. */
void hmac_sha256(const uint8_t *key, uint32_t keylen,
                  const uint8_t *data, uint32_t datalen, uint8_t out[32]);
