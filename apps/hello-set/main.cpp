/**
 * @file apps/hello-set/main.cpp
 * @brief std::set: another self-checking stress test, but this one needs
 *        zero new runtime code -- std::set is backed by the exact same
 *        std::_Rb_tree as std::map (just keyed on the value itself instead
 *        of a pair's first member), so it links straight against the
 *        lib/cxx_rbtree.cpp functions apps/hello-map already exercises.
 *
 * Same reason as apps/hello-str/hello-map for not including our own
 * <stdio.h>: the real <set> transitively reaches the real system <cstdio>.
 */
#include <set>

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
    std::set<int> s;
    bool ok = true;

    /* Ascending insert is again the worst case for rotations. Duplicates
     * thrown in too, since a set (unlike a multiset) must silently drop
     * them -- that's a real behavioral check, not just a tree-shape one. */
    for (int i = 1; i <= 40; i++) {
        s.insert(i);
        s.insert(i); /* duplicate: must not grow the set or the tree */
    }
    if (s.size() != 40) {
        put("FAIL size after insert\n");
        ok = false;
    }

    /* Erase every 3rd value -- leaf/one-child/two-child cases again. */
    for (int i = 1; i <= 40; i += 3) {
        s.erase(i);
    }
    int expected = 0;
    for (int i = 1; i <= 40; i++) {
        if (i % 3 != 1) {
            expected++;
        }
    }
    if ((int) s.size() != expected) {
        put("FAIL size after erase\n");
        ok = false;
    }

    /* In-order walk must be strictly increasing and skip every erased
     * value. */
    int prev = -1;
    int count = 0;
    for (int v : s) {
        if (v <= prev || v % 3 == 1) {
            put("FAIL at value ");
            put_int(v);
            put("\n");
            ok = false;
        }
        prev = v;
        count++;
    }
    if (count != expected) {
        put("FAIL iterator count\n");
        ok = false;
    }

    if (s.find(2) == s.end()) {
        put("FAIL find(2) missing\n");
        ok = false;
    }
    if (s.count(1) != 0) {
        put("FAIL count(1) should be erased\n");
        ok = false;
    }

    /* lower_bound/upper_bound touch tree traversal from an arbitrary
     * (possibly absent) key, not just begin()/end() walks. */
    auto lb = s.lower_bound(20);
    if (lb == s.end() || *lb < 20) {
        put("FAIL lower_bound(20)\n");
        ok = false;
    }
    auto ub = s.upper_bound(20);
    if (ub != s.end() && *ub <= 20) {
        put("FAIL upper_bound(20)\n");
        ok = false;
    }

    /* Drain to empty via begin()-erase, a different traversal pattern from
     * the fixed-stride erase above. */
    while (!s.empty()) {
        s.erase(s.begin());
    }
    if (!s.empty()) {
        put("FAIL not empty after draining\n");
        ok = false;
    }

    put(ok ? "PASS\n" : "OVERALL FAIL\n");
    return ok ? 0 : 1;
}
