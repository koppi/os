/**
 * @file apps/hello-cpp/cxx_start.c
 * @brief Real ELF entry point for a C++ app.
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

int _start(void) {
    for (ctor_fn *f = __init_array_start; f != __init_array_end; f++) {
        (*f)();
    }

    return main();
}
