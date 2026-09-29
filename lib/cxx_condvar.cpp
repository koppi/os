/**
 * cxx_condvar.cpp -- NOT Qt source; a minimal freestanding
 * implementation of std::condition_variable's four out-of-line methods
 * (real libstdc++ defines them in its compiled src/c++11/condition_
 * variable.cc, not linked here) plus std::_Sp_make_shared_tag::_S_eq
 * (shared_ptr's -fno-rtti fallback for identifying a make_shared
 * allocation without typeid()). Same category as cxx_pmr_koppios.cpp
 * and lib/cxx_chrono.cpp: real standard-library runtime support this
 * kernel doesn't link a full libstdc++ for, not Qt-specific code.
 *
 * <condition_variable>'s own private __condvar helper (bits/std_mutex.h)
 * is fully inline, delegating to __gthread_cond_wait/_signal/_broadcast
 * -- which map to the real pthread_cond_t primitives lib/pthread_glibc.c
 * already provides (real glibc-shaped mutex/cond types, backed by this
 * kernel's thread_create/mutex_lock syscalls). So condition_variable's
 * own ctor/dtor/notify_one/notify_all/wait bodies are just thin
 * pass-throughs to its private _M_cond member -- there is no new
 * synchronization primitive to invent here, only the out-of-line glue
 * real libstdc++ would otherwise provide.
 *
 * _Sp_make_shared_tag::_S_eq: with RTTI disabled, real libstdc++'s
 * shared_ptr deleter-lookup can't compare arbitrary type_info via
 * typeid(), so it falls back to this identity check against its own
 * fake, non-polymorphic "tag" object (_S_ti(), a reinterpret_cast of a
 * zeroed byte buffer, not a real RTTI type_info at all) -- exactly the
 * same address comparison the call site already performs for the
 * "&__ti == &_S_ti()" case one line above the fallback; _S_eq only
 * needs to repeat that same check for the (with -fno-rtti) sole caller
 * that doesn't already have both addresses in hand at the call site.
 */
#include <condition_variable>
#include <memory>

namespace std {

condition_variable::condition_variable() noexcept = default;
condition_variable::~condition_variable() noexcept = default;

void condition_variable::notify_one() noexcept
{
    _M_cond.notify_one();
}

void condition_variable::notify_all() noexcept
{
    _M_cond.notify_all();
}

void condition_variable::wait(unique_lock<mutex> &lock)
{
    _M_cond.wait(*lock.mutex());
}

bool _Sp_make_shared_tag::_S_eq(const type_info &ti) noexcept
{
    return &ti == &_S_ti();
}

} // namespace std
