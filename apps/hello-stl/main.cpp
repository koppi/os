/**
 * @file apps/hello-stl/main.cpp
 * @brief First real-libstdc++ userspace app: std::vector, std::sort and
 *        range-for over the actual GCC 32-bit headers, on top of the
 *        cxxabi.cpp shim (operator new/delete, the std::__throw_* error
 *        paths) that makes them link.
 */
#include <stdio.h>
#include <vector>
#include <algorithm>

int main() {
    std::vector<int> v;
    for (int i = 5; i >= 1; i--) {
        v.push_back(i);
    }

    printf((char *) "before sort:");
    for (int x : v) {
        printf((char *) " %d", x);
    }
    printf((char *) "\n");

    std::sort(v.begin(), v.end());

    printf((char *) "after sort: ");
    for (int x : v) {
        printf((char *) " %d", x);
    }
    printf((char *) "\n");

    printf((char *) "size=%d capacity=%d\n", (int) v.size(), (int) v.capacity());
    return 0;
}
