#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void recurse(int n) {
    char buf[64];
    buf[0] = (char) n;
    if (n <= 0)
        return;
    recurse(n - 1);
    buf[63] = buf[0];
}

int main() {
    _write("A\n", 2);
    char *p = (char *) malloc(1 << 20);
    if (!p) { _write("E1\n", 3); return 1; }
    memset(p, 0xAA, 1 << 20);
    _write("B\n", 2);
    p = (char *) realloc(p, 2 << 20);
    if (!p) { _write("E2\n", 3); return 2; }
    memset(p, 0xBB, 2 << 20);
    _write("C\n", 2);
    recurse(2000);
    _write("D\n", 2);
    _write("mem: heap grew, realloc ok, recurse ok\n", 38);
    return 0;
}
