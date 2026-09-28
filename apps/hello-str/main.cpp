/**
 * @file apps/hello-str/main.cpp
 * @brief First std::string userspace app.
 *
 * Deliberately does NOT include <stdio.h> (ours, in include/lib/): the real
 * <string> pulls in the real system <cstdio> internally (for std::to_string
 * of float/double), which declares the actual libc `printf`/`FILE`/`fopen`/
 * `fclose`/`fread`/`scanf` -- global, extern "C" names that collide with our
 * custom libc's declarations of the very same names but different
 * signatures the moment both are visible in one translation unit. `_write`
 * (raw length-delimited output, syscall 12) has no such collision, so output
 * goes through that instead.
 */
#include <string>

extern "C" unsigned int _write(const void *buf, unsigned int len);

static void put(const std::string &s) {
    _write(s.c_str(), (unsigned int) s.size());
}

int main() {
    std::string a = "hello";
    std::string b = "world";
    std::string c = a + " " + b;
    put(c);
    put("\n");

    put("substr(6): ");
    put(c.substr(6));
    put("\n");

    put("find(\"wor\"): ");
    put(c.find("wor") != std::string::npos ? "found\n" : "not found\n");

    put("a < b: ");
    put(a < b ? "true\n" : "false\n");

    put("to_string(42): ");
    put(std::to_string(42));
    put("\n");

    return 0;
}
