/**
 * @file apps/hello-umap/main.cpp
 * @brief std::unordered_map: a self-checking stress test (same reasoning as
 *        apps/hello-map -- lib/cxx_hashtable.cpp hand-implements the
 *        rehash policy this container relies on, so trust the checks, not
 *        the eye). Iteration order is unspecified for an unordered_map, so
 *        this checks size/find/values/checksum instead of sorted order.
 *
 * Same reason as apps/hello-str/hello-map for not including our own
 * <stdio.h>: the real <unordered_map> transitively reaches the real system
 * <cstdio>.
 */
#include <unordered_map>

extern "C" unsigned int _write(const void *buf, unsigned int len);

static void put(const char *s) {
    int n = 0;
    while (s[n]) {
        n++;
    }
    _write(s, (unsigned int) n);
}

static void put_int(long v) {
    char tmp[24];
    int tn = 0;
    bool neg = v < 0;
    unsigned long u = neg ? (unsigned long) (-v) : (unsigned long) v;
    if (u == 0) {
        tmp[tn++] = '0';
    }
    while (u) {
        tmp[tn++] = (char) ('0' + (u % 10));
        u /= 10;
    }
    char buf[24];
    int n = 0;
    if (neg) {
        buf[n++] = '-';
    }
    while (tn) {
        buf[n++] = tmp[--tn];
    }
    buf[n] = 0;
    put(buf);
}

int main() {
    std::unordered_map<int, int> m;
    bool ok = true;

    /* Enough inserts to force several rehashes past the default initial
     * bucket count. */
    const int N = 200;
    long expected_sum = 0;
    for (int i = 0; i < N; i++) {
        m[i] = i * 3 + 1;
    }
    if (m.size() != (size_t) N) {
        put("FAIL size after insert\n");
        ok = false;
    }

    /* Erase every 4th key. */
    for (int i = 0; i < N; i += 4) {
        m.erase(i);
    }
    int expected_count = 0;
    for (int i = 0; i < N; i++) {
        if (i % 4 != 0) {
            expected_count++;
            expected_sum += i * 3 + 1;
        }
    }
    if ((int) m.size() != expected_count) {
        put("FAIL size after erase\n");
        ok = false;
    }

    /* Every surviving key must still map to its original value, every
     * erased key must be gone, and the checksum over all values (order
     * doesn't matter, only membership) must match. */
    long sum = 0;
    int count = 0;
    for (auto &kv : m) {
        if (kv.first % 4 == 0) {
            put("FAIL erased key present: ");
            put_int(kv.first);
            put("\n");
            ok = false;
        }
        if (kv.second != kv.first * 3 + 1) {
            put("FAIL value mismatch at ");
            put_int(kv.first);
            put("\n");
            ok = false;
        }
        sum += kv.second;
        count++;
    }
    if (count != expected_count || sum != expected_sum) {
        put("FAIL checksum\n");
        ok = false;
    }

    /* find()/count() spot checks, plus an explicit reserve() to stress the
     * rehash policy directly instead of only through incidental growth. */
    if (m.find(1) == m.end()) {
        put("FAIL find(1) missing\n");
        ok = false;
    }
    if (m.count(0) != 0) {
        put("FAIL count(0) should be erased\n");
        ok = false;
    }
    m.reserve(10000);
    if (m.find(N - 1) == m.end() || m[N - 1] != (N - 1) * 3 + 1) {
        put("FAIL lookup survives reserve()\n");
        ok = false;
    }

    /* Drain to empty via begin()-erase, a different traversal pattern from
     * the fixed-stride erase above. */
    while (!m.empty()) {
        m.erase(m.begin());
    }
    if (!m.empty()) {
        put("FAIL not empty after draining\n");
        ok = false;
    }

    put(ok ? "PASS\n" : "OVERALL FAIL\n");
    return ok ? 0 : 1;
}
