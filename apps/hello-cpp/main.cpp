/**
 * @file apps/hello-cpp/main.cpp
 * @brief First C++ userspace app: exercises global constructors (run from
 *        .init_array by cxx_start.c before main), virtual dispatch and the
 *        operator new/delete shim in cxxabi.cpp, on top of the existing C
 *        libc syscall wrappers.
 */
#include <stdio.h>

class Greeter {
public:
    explicit Greeter(const char *name) : name_(name) {
        printf((char *) "Greeter constructed for %s\n", (char *) name_);
    }

    virtual ~Greeter() {
    }

    virtual void greet() const {
        printf((char *) "Hello from C++, %s!\n", (char *) name_);
    }

private:
    const char *name_;
};

/* Global scope: proves .init_array constructor running actually happens
 * before main() is entered. */
static Greeter global_greeter("global scope");

int main() {
    global_greeter.greet();

    Greeter *heap_greeter = new Greeter("the heap");
    heap_greeter->greet();
    delete heap_greeter;

    printf((char *) "C++ hello world done.\n");
    return 0;
}
