/**
 * @file csprng.h
 * @brief A best-effort random byte generator for SSH key material: an
 *        AES-128-CTR keystream seeded from timing jitter (@c rdtsc), the RTC
 *        and the PIT tick count, re-mixed with fresh jitter on every call.
 *
 * This is not a hardened CSPRNG — there is no hardware entropy source on the
 * emulated QEMU machine this kernel targets, so it leans entirely on
 * instruction-timing jitter. That is enough to make each boot's SSH host key
 * and per-session key-exchange keys distinct from each other, which is what
 * this OS needs; it is not an audited source of cryptographic randomness.
 */
#pragma once

#include <types.h>

/** @brief Fill @p out with @p len pseudo-random bytes. */
void csprng_bytes(uint8_t *out, int len);
