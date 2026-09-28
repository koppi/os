/**
 * @file apps/hello-map/main.cpp
 * @brief std::map: a self-checking stress test, not just a "does it link"
 *        demo, since lib/cxx_rbtree.cpp hand-implements the rb-tree
 *        insert/erase-fixup algorithms this container relies on.
 *
 * Same reason as apps/hello-str for not including our own <stdio.h>: the
 * real <map> transitively reaches the real system <cstdio>.
 */
#include <map>

extern "C" unsigned int _write(const void *buf, unsigned int len);

static void put(const char *s) {
    int n = 0;
    while (s[n]) {
        n++;
    }
    _write(s, (unsigned int) n);
}

static void put_int(int v) {
    char tmp[16];
    int tn = 0;
    bool neg = v < 0;
    unsigned int u = neg ? (unsigned int) (-(long) v) : (unsigned int) v;
    if (u == 0) {
        tmp[tn++] = '0';
    }
    while (u) {
        tmp[tn++] = (char) ('0' + (u % 10));
        u /= 10;
    }
    char buf[16];
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
    std::map<int, int> m;
    bool ok = true;

    /* Ascending insert is the classic worst case for a naive BST -- forces
     * plenty of rotations to keep an rb-tree balanced. */
    for (int i = 1; i <= 40; i++) {
        m[i] = i * 10;
    }
    if (m.size() != 40) {
        put("FAIL size after insert\n");
        ok = false;
    }

    /* Erase every 3rd key: leaves, one-child and two-child erase cases all
     * happen across a run like this. */
    for (int i = 1; i <= 40; i += 3) {
        m.erase(i);
    }

    int expected = 0;
    for (int i = 1; i <= 40; i++) {
        if (i % 3 != 1) {
            expected++;
        }
    }
    if ((int) m.size() != expected) {
        put("FAIL size after erase\n");
        ok = false;
    }

    /* In-order walk must be strictly increasing, values must match, and
     * none of the erased keys may still be present. */
    int prev = -1;
    int count = 0;
    for (auto &kv : m) {
        if (kv.first <= prev || kv.second != kv.first * 10 || kv.first % 3 == 1) {
            put("FAIL at key ");
            put_int(kv.first);
            put("\n");
            ok = false;
        }
        prev = kv.first;
        count++;
    }
    if (count != expected) {
        put("FAIL iterator count\n");
        ok = false;
    }

    if (m.find(2) == m.end()) {
        put("FAIL find(2) missing\n");
        ok = false;
    }
    if (m.count(1) != 0) {
        put("FAIL count(1) should be erased\n");
        ok = false;
    }

    /* Erase down to empty via begin() -- a different stress pattern from
     * the fixed-stride erase above (always removes the leftmost node). */
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
