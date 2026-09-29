/**
 * @file lib/cxxabi.cpp
 * @brief Minimal Itanium C++ ABI runtime, shared by every C++ app.
 *
 * There is no libstdc++/libsupc++ port for this target, so the handful of
 * symbols GCC assumes always exist for any C++ translation unit are defined
 * here by hand: the new/delete operators (routed to the existing malloc/free
 * syscall wrappers), the pure-virtual-call thunk, and the guard functions for
 * function-local statics. Building with -fno-exceptions -fno-rtti keeps the
 * rest of libsupc++ (typeinfo, unwinding) out of the picture entirely.
 *
 * STL headers (<vector>, <string>, ...) are usable despite that: they are
 * mostly header-only templates, and the real, matching libstdc++ headers for
 * this target compile fine as long as the compile is NOT `-ffreestanding`
 * (that flag makes bits/c++config.h set _GLIBCXX_HOSTED=0, which walls off
 * <vector>/<algorithm>/... behind a `#error` in bits/requires_hosted.h). What
 * we still don't have is libstdc++.a itself, so anything the library
 * compiles out-of-line into it -- the error-path __throw_* helpers below,
 * and the std::string char specialization's extern-template members -- has
 * to be supplied here or avoided.
 */

#include <lib/stdio.h>
#include <lib/stdlib.h>
#include <new> // std::align_val_t only -- header-only, no OS dependency

void *operator new(size_t size) {
    return malloc(size);
}

void *operator new[](size_t size) {
    return malloc(size);
}

/* nothrow new (C++11) -- returns nullptr on failure instead of throwing,
 * which malloc() already does on its own; -fno-exceptions makes this the
 * ONLY kind of new that could ever meaningfully report failure here, so
 * unlike the throwing overloads above (which just assume malloc() always
 * succeeds), this one is genuinely faithful to the standard's contract. */
void *operator new(size_t size, const std::nothrow_t &) noexcept {
    return malloc(size);
}

void operator delete(void *p) noexcept {
    free(p);
}

void operator delete[](void *p) noexcept {
    free(p);
}

/* Sized deallocation (C++14) -- same underlying free(), size unused. */
void operator delete(void *p, size_t) noexcept {
    free(p);
}

void operator delete[](void *p, size_t) noexcept {
    free(p);
}

/* Aligned new/delete (C++17, std::align_val_t) -- same underlying
 * malloc()/free(), the requested alignment is not actually honored (this
 * kernel's allocator makes no over-alignment guarantee beyond whatever
 * malloc() itself naturally gives). Only reached by code that merely
 * declares an over-aligned type without actually depending on the
 * alignment for correctness (e.g. an unused SIMD-friendly member some
 * container's storage class provides); a type that genuinely needs
 * stricter alignment than malloc() provides would silently misbehave. */
void *operator new(size_t size, std::align_val_t) {
    return malloc(size);
}

void *operator new[](size_t size, std::align_val_t) {
    return malloc(size);
}

void operator delete(void *p, std::align_val_t) noexcept {
    free(p);
}

void operator delete[](void *p, std::align_val_t) noexcept {
    free(p);
}

void operator delete(void *p, size_t, std::align_val_t) noexcept {
    free(p);
}

void operator delete[](void *p, size_t, std::align_val_t) noexcept {
    free(p);
}

extern "C" {

/* Reached only if a call arrives through a not-yet-constructed or
 * already-destroyed vtable slot -- a bug, not a recoverable condition, and
 * there is no abort() in this freestanding target to hand it to. */
void __cxa_pure_virtual() {
    for (;;) {
    }
}

/* -fno-use-cxa-atexit still leaves the compiler free to emit __cxa_atexit
 * for some global destructors; process exit here never runs user-level
 * cleanup anyway (end_process_return() goes straight to the exit syscall),
 * so registration is a deliberate no-op rather than a real implementation. */
int __cxa_atexit(void (*)(void *), void *, void *) {
    return 0;
}

void __cxa_finalize(void *) {
}

/* i386 Itanium ABI guard variables are 32-bit; only the low byte is the
 * initialized flag. Not used by the current hello-world (no function-local
 * statics with dynamic initializers) but cheap to provide correctly. */
int __cxa_guard_acquire(int *guard) {
    return !*(char *) guard;
}

void __cxa_guard_release(int *guard) {
    *(char *) guard = 1;
}

void __cxa_guard_abort(int *) {
}

} /* extern "C" */

void *__dso_handle = 0;

/* libstdc++'s error paths (vector::at, string growth over max_size, etc.)
 * call these instead of `throw` directly -- that indirection is what lets
 * the headers stay usable under -fno-exceptions at all. There is nothing to
 * unwind to here, so every one is fatal: print what/where and hang, the same
 * fallback __cxa_pure_virtual above uses. The full set is small and fixed
 * (bits/functexcept.h), so it is all provided up front rather than grown one
 * link error at a time as more STL headers get used. */
namespace std {

[[noreturn]] static void halt(const char *what) {
    printf((char *) "libstdc++: fatal: %s\n", (char *) what);
    for (;;) {
    }
}

void __throw_bad_exception() {
    halt("bad_exception");
}
void __throw_bad_alloc() {
    halt("bad_alloc");
}
void __throw_bad_array_new_length() {
    halt("bad_array_new_length");
}
void __throw_bad_cast() {
    halt("bad_cast");
}
void __throw_bad_typeid() {
    halt("bad_typeid");
}
void __throw_logic_error(const char *msg) {
    halt(msg);
}
void __throw_domain_error(const char *msg) {
    halt(msg);
}
void __throw_invalid_argument(const char *msg) {
    halt(msg);
}
void __throw_length_error(const char *msg) {
    halt(msg);
}
void __throw_out_of_range(const char *msg) {
    halt(msg);
}
void __throw_out_of_range_fmt(const char *fmt, ...) {
    halt(fmt);
}
void __throw_runtime_error(const char *msg) {
    halt(msg);
}
void __throw_range_error(const char *msg) {
    halt(msg);
}
void __throw_overflow_error(const char *msg) {
    halt(msg);
}
void __throw_underflow_error(const char *msg) {
    halt(msg);
}
void __throw_ios_failure(const char *msg) {
    halt(msg);
}
void __throw_ios_failure(const char *msg, int) {
    halt(msg);
}
void __throw_system_error(int) {
    halt("system_error");
}
void __throw_future_error(int) {
    halt("future_error");
}
void __throw_bad_function_call() {
    halt("bad_function_call");
}

/* GCC 15's _GLIBCXX_ASSERTIONS hardening (on by default): a failed internal
 * precondition check (e.g. map::erase on an invalid iterator) calls this
 * instead of assert()/abort(), neither of which exist here either. */
void __glibcxx_assert_fail(const char *, int, const char *, const char *condition) {
    halt(condition);
}

/* Reached if real C++ code (not ours -- everything here builds
 * -fno-exceptions, so nothing we write can throw) calls std::terminate()
 * directly, e.g. a third-party library's own explicit error path. Same
 * halt()-and-hang treatment as everything else above: there is nothing to
 * unwind to or report through. */
void terminate() noexcept {
    halt("terminate");
}

} // namespace std
