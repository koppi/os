/**
 * @file lib/cxx_chrono.cpp
 * @brief std::chrono::steady_clock::now() -- the one clock function real
 *        Qt6 source (qtimerinfo_unix.cpp, this event dispatcher's timer
 *        queue) calls directly rather than through Qt's own QDeadlineTimer/
 *        QElapsedTimer wrappers. Also std::chrono::system_clock::now(),
 *        needed once the same real-Qt closure grew to include real
 *        FreeType/HarfBuzz text shaping and platform-integration code
 *        that touches wall-clock time (QDateTime-adjacent internals) --
 *        same out-of-line-symbol gap, same fix, just CLOCK_REALTIME
 *        instead of CLOCK_MONOTONIC.
 *
 * <chrono> declares `steady_clock::now()` but, like every other libstdc++
 * clock, leaves it out-of-line: in a normal install its body lives in
 * libstdc++'s compiled src/c++11/chrono.cc. It is mangled inside an
 * inline ABI-versioning namespace (`std::chrono::_V2`, see <bits/chrono.h>'s
 * own comment on `_GLIBCXX_BEGIN_INLINE_ABI_NAMESPACE(_V2)`), so the
 * definition below has to use that exact namespace nesting for the symbol
 * to match what real <chrono> callers actually link against. <chrono>
 * itself is safe to include here unmodified (only pulls in <ratio>/
 * <type_traits>/<limits>, no OS headers), so this reopens the REAL
 * steady_clock class from the real header rather than redeclaring it --
 * matching lib/cxx_rbtree.cpp's approach of including <map> for the real
 * _Rb_tree_node_base layout rather than guessing at one.
 *
 * Backed by clock_gettime(CLOCK_MONOTONIC, ...) -- defined in
 * lib/pthread_glibc.c, itself backed by this kernel's time() syscall (14),
 * whole Unix seconds only. steady_clock::now() therefore also only
 * advances in whole-second steps here, the same documented limitation as
 * every other timing primitive in this tree.
 */
/* <chrono> itself transitively includes <ctime> (bits/chrono.h -> <ctime>),
 * which is where clock_gettime()/struct timespec/CLOCK_MONOTONIC actually
 * come from below -- no separate declaration needed, and no risk of the
 * header-aliasing problem lib/pthread_glibc.c's file comment describes
 * (<ctime> does not touch <sched.h>/<pthread.h>). */
#include <chrono>

namespace std {
namespace chrono {
inline namespace _V2 {

steady_clock::time_point steady_clock::now() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return time_point(duration_cast<duration>(
        chrono::seconds(ts.tv_sec) + chrono::nanoseconds(ts.tv_nsec)));
}

system_clock::time_point system_clock::now() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return time_point(duration_cast<duration>(
        chrono::seconds(ts.tv_sec) + chrono::nanoseconds(ts.tv_nsec)));
}

} // inline namespace _V2
} // namespace chrono
} // namespace std
