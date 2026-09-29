/**
 * @file lib/cxx_string.cpp
 * @brief Explicit instantiation of std::basic_string<char>::_M_replace_cold,
 *        the one piece of std::string real libstdc++ marks `extern
 *        template` (bits/basic_string.tcc's own comment: "Export
 *        _M_replace_cold even for C++20") so ordinary code never
 *        implicitly instantiates it -- it expects to find the real,
 *        precompiled body in libstdc++.so/.a, which this -nostdlib tree
 *        doesn't have (same class of gap as lib/cxx_rbtree.cpp/
 *        lib/cxx_hashtable.cpp for std::map/unordered_map, and this
 *        project's own cxx_pmr.cpp/cxx_condvar.cpp/cxx_list.cpp).
 *
 * Unlike those, there is no algorithm to reimplement here: the full
 * template body is already visible in the (real, unmodified) system
 * <bits/basic_string.tcc>, marked `__attribute__((noinline, cold))` so
 * it's always a separate out-of-line function regardless of
 * optimization level -- this file's only job is to force one explicit
 * instantiation of it for `basic_string<char>` (the `std::string`
 * QByteArray's std::string-interop methods use), which real libstdc++
 * would otherwise have already done. Ordinary explicit instantiation
 * syntax, not a reimplementation.
 */
#include <string>

template void std::basic_string<char>::_M_replace_cold(
    char *, std::basic_string<char>::size_type, const char *,
    std::basic_string<char>::size_type, std::basic_string<char>::size_type);
