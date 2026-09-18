/**
 * @file bignum256.c
 * @brief See bignum256.h.
 */
#include <bignum256.h>
#include <lib/string.h>

void u256_zero(u256 *r) { memset(r, 0, sizeof *r); }

void u256_set_u32(u256 *r, uint32_t v) {
    u256_zero(r);
    r->w[0] = v;
}

void u256_from_bytes_le(u256 *r, const uint8_t b[32]) {
    for (int i = 0; i < 8; i++)
        r->w[i] = (uint32_t)b[i * 4] | ((uint32_t)b[i * 4 + 1] << 8) |
                  ((uint32_t)b[i * 4 + 2] << 16) | ((uint32_t)b[i * 4 + 3] << 24);
}

void u256_to_bytes_le(uint8_t b[32], const u256 *a) {
    for (int i = 0; i < 8; i++) {
        b[i * 4]     = (uint8_t)(a->w[i]);
        b[i * 4 + 1] = (uint8_t)(a->w[i] >> 8);
        b[i * 4 + 2] = (uint8_t)(a->w[i] >> 16);
        b[i * 4 + 3] = (uint8_t)(a->w[i] >> 24);
    }
}

void u256_from_bytes_be(u256 *r, const uint8_t *b, int len) {
    u256_zero(r);
    for (int i = 0; i < len && i < 32; i++)
        r->w[i / 4] |= (uint32_t)b[len - 1 - i] << ((i % 4) * 8);
}

void u256_to_bytes_be(uint8_t b[32], const u256 *a) {
    for (int i = 0; i < 32; i++)
        b[31 - i] = (uint8_t)(a->w[i / 4] >> ((i % 4) * 8));
}

int u256_is_zero(const u256 *a) {
    uint32_t acc = 0;
    for (int i = 0; i < 8; i++)
        acc |= a->w[i];
    return acc == 0;
}

int u256_bit(const u256 *a, int i) {
    return (a->w[i / 32] >> (i % 32)) & 1;
}

int u256_cmp(const u256 *a, const u256 *b) {
    for (int i = 7; i >= 0; i--) {
        if (a->w[i] != b->w[i])
            return a->w[i] < b->w[i] ? -1 : 1;
    }
    return 0;
}

uint32_t u256_add(u256 *r, const u256 *a, const u256 *b) {
    uint64_t carry = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t s = (uint64_t)a->w[i] + b->w[i] + carry;
        r->w[i] = (uint32_t)s;
        carry = s >> 32;
    }
    return (uint32_t)carry;
}

uint32_t u256_sub(u256 *r, const u256 *a, const u256 *b) {
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t s = (uint64_t)a->w[i] - b->w[i] - borrow;
        r->w[i] = (uint32_t)s;
        borrow = (s >> 32) ? 1 : 0;
    }
    return (uint32_t)borrow;
}

void u256_mul(u512 *r, const u256 *a, const u256 *b) {
    uint32_t res[16];
    memset(res, 0, sizeof res);
    for (int i = 0; i < 8; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < 8; j++) {
            uint64_t cur = (uint64_t)res[i + j] + (uint64_t)a->w[i] * (uint64_t)b->w[j] + carry;
            res[i + j] = (uint32_t)cur;
            carry = cur >> 32;
        }
        int k = i + 8;
        while (carry) {
            uint64_t cur = (uint64_t)res[k] + carry;
            res[k] = (uint32_t)cur;
            carry = cur >> 32;
            k++;
        }
    }
    memcpy(r->w, res, sizeof res);
}

void u256_mod(u256 *r, const u512 *x, const u256 *m) {
    u256 rem;
    u256_zero(&rem);
    for (int i = 511; i >= 0; i--) {
        uint32_t bit = (x->w[i / 32] >> (i % 32)) & 1;
        uint32_t c = bit;
        for (int wi = 0; wi < 8; wi++) {
            uint32_t nc = rem.w[wi] >> 31;
            rem.w[wi] = (rem.w[wi] << 1) | c;
            c = nc;
        }
        if (u256_cmp(&rem, m) >= 0)
            u256_sub(&rem, &rem, m);
    }
    *r = rem;
}

void bytes_be_mod(u256 *r, const uint8_t *x, int len, const u256 *m) {
    u256 rem;
    u256_zero(&rem);
    for (int i = 0; i < len; i++) {
        uint8_t byte = x[i];
        for (int b = 7; b >= 0; b--) {
            uint32_t bit = (byte >> b) & 1;
            uint32_t c = bit;
            for (int wi = 0; wi < 8; wi++) {
                uint32_t nc = rem.w[wi] >> 31;
                rem.w[wi] = (rem.w[wi] << 1) | c;
                c = nc;
            }
            if (u256_cmp(&rem, m) >= 0)
                u256_sub(&rem, &rem, m);
        }
    }
    *r = rem;
}

void u256_add_mod(u256 *r, const u256 *a, const u256 *b, const u256 *m) {
    u256 s;
    u256_add(&s, a, b);          /* a, b < m < 2^255, so a+b < 2^256: no lost carry */
    if (u256_cmp(&s, m) >= 0)
        u256_sub(&s, &s, m);
    *r = s;
}

void u256_sub_mod(u256 *r, const u256 *a, const u256 *b, const u256 *m) {
    u256 s;
    if (u256_cmp(a, b) >= 0) {
        u256_sub(&s, a, b);
    } else {
        u256 t;
        u256_sub(&t, m, b);
        u256_add(&s, &t, a);
    }
    *r = s;
}

void u256_mul_mod(u256 *r, const u256 *a, const u256 *b, const u256 *m) {
    u512 p;
    u256_mul(&p, a, b);
    u256_mod(r, &p, m);
}

void u256_pow_mod(u256 *r, const u256 *base, const u256 *exp, const u256 *m) {
    u256 result, b;
    u256_set_u32(&result, 1);
    b = *base;
    if (u256_cmp(&b, m) >= 0) {
        u512 tmp;
        memset(&tmp, 0, sizeof tmp);
        memcpy(tmp.w, b.w, sizeof b.w);
        u256_mod(&b, &tmp, m);
    }
    for (int i = 255; i >= 0; i--) {
        u256_mul_mod(&result, &result, &result, m);
        if (u256_bit(exp, i))
            u256_mul_mod(&result, &result, &b, m);
    }
    *r = result;
}

void u256_inv_mod(u256 *r, const u256 *a, const u256 *m) {
    u256 m_minus_2, two;
    u256_set_u32(&two, 2);
    u256_sub(&m_minus_2, m, &two);
    u256_pow_mod(r, a, &m_minus_2, m);
}
