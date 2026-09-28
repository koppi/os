/**
 * @file apps/hello-cpp/cxxabi.cpp
 * @brief Minimal Itanium C++ ABI runtime for freestanding userspace.
 *
 * There is no libstdc++/libsupc++ port for this target, so the handful of
 * symbols GCC assumes always exist for any C++ translation unit are defined
 * here by hand: the new/delete operators (routed to the existing malloc/free
 * syscall wrappers), the pure-virtual-call thunk, and the guard functions for
 * function-local statics. Building with -fno-exceptions -fno-rtti keeps the
 * rest of libsupc++ (typeinfo, unwinding) out of the picture entirely.
 */

#include <stdlib.h>

void *operator new(size_t size) {
    return malloc(size);
}

void *operator new[](size_t size) {
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
