/**
 * @file lib/cxx_start.c
 * @brief Real ELF entry point for a C++ app. Shared by every C++ app; each
 *        one's linker script must say `ENTRY(_start)` instead of `ENTRY(main)`.
 *
 * Plain C apps here are entered straight at `main` (see apps/hello/hello.lds):
 * the kernel pushes a return address pointing at end_process_return() onto
 * the initial stack, so `main`'s own `ret` falls straight into process exit.
 * A C++ app needs one thing done first -- global constructors run from
 * .init_array -- so this becomes the entry point instead and calls `main`
 * itself. `main`'s int return value comes back in %eax under cdecl, which is
 * also how this function returns its own int result, so the kernel's
 * injected return address still works unmodified once `_start` falls through
 * to its `ret`.
 */

typedef void (*ctor_fn)(void);

extern ctor_fn __init_array_start[];
extern ctor_fn __init_array_end[];

extern int main(void);

/* lib/tls.c: weak, so a program linked without it (or without TLS symbols in its linker script)
 * is unaffected. TLS must exist before the first constructor runs: they may use thread_local. */
extern int __tls_thread_init(void) __attribute__((weak));
extern void __tls_main_exit(void) __attribute__((weak));

int _start(void) {
    if (__tls_thread_init) {
        __tls_thread_init();
    }

    for (ctor_fn *f = __init_array_start; f != __init_array_end; f++) {
        (*f)();
    }

    int rc = main();

    if (__tls_main_exit) {
        __tls_main_exit();     /* the main thread's thread_local destructors (not its pthread keys: exit() skips those) */
    }
    return rc;
}
