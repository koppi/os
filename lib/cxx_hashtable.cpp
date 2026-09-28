/**
 * @file lib/cxx_hashtable.cpp
 * @brief Rehash policy backing std::unordered_map/std::unordered_set.
 *
 * <bits/hashtable_policy.h>'s _Prime_rehash_policy leaves two members
 * out-of-line -- `_M_next_bkt(size_t) const` and `_M_need_rehash(size_t,
 * size_t, size_t) const` -- whose bodies normally live in libstdc++'s
 * compiled hashtable_c++0x.cc. Everything else about _Hashtable is
 * header-only templates (same story as std::map's rb-tree functions in
 * lib/cxx_rbtree.cpp).
 *
 * The header's own comment on _M_next_bkt states its whole contract:
 * "Return a bucket size no smaller than n." It does not have to be the
 * exact prime GCC's own (much larger, hand-tuned) lookup table would have
 * picked -- any bucket count that keeps chaining correct works, primality
 * just keeps `hash % bucket_count` from degenerating for hash functions
 * that are themselves multiples of small numbers. So this finds the next
 * prime by plain trial division rather than reproducing that table (a
 * large, easy-to-transcribe-wrong literal this project has no way to
 * verify against ground truth) -- trivially fast at the bucket counts a
 * hobby OS's userspace programs actually reach.
 */

#include <unordered_map>

namespace std {
namespace __detail {

static bool is_prime(size_t n) {
    if (n < 2) {
        return false;
    }
    if (n % 2 == 0) {
        return n == 2;
    }
    for (size_t d = 3; d * d <= n; d += 2) {
        if (n % d == 0) {
            return false;
        }
    }
    return true;
}

size_t _Prime_rehash_policy::_M_next_bkt(size_t n) const {
    size_t candidate;
    if (n <= 2) {
        candidate = 2;
    } else {
        candidate = n | 1; /* smallest odd >= n */
        while (!is_prime(candidate)) {
            candidate += 2;
        }
    }
    _M_next_resize = (size_t) (candidate * _M_max_load_factor);
    return candidate;
}

pair<bool, size_t> _Prime_rehash_policy::_M_need_rehash(size_t n_bkt, size_t n_elt, size_t n_ins) const {
    if (n_elt + n_ins <= _M_next_resize) {
        return {false, 0};
    }

    double min_bkts = (double) (n_elt + n_ins) / (double) _M_max_load_factor;
    if (min_bkts < (double) n_bkt) {
        _M_next_resize = (size_t) (n_bkt * _M_max_load_factor);
        return {false, 0};
    }

    size_t grown = n_bkt * _S_growth_factor;
    size_t wanted = (size_t) min_bkts + 1;
    return {true, _M_next_bkt(wanted > grown ? wanted : grown)};
}

} // namespace __detail
} // namespace std
