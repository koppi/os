/*
 * GCC 15 emits __udivmoddi4 for 64-bit unsigned div/mod; arith64.c (from the
 * kernel tree) provides the same operation as __divmoddi4.
 */
unsigned long long __divmoddi4(unsigned long long a, unsigned long long b,
                               unsigned long long *rem);

unsigned long long __udivmoddi4(unsigned long long a, unsigned long long b,
                                unsigned long long *rem) {
    return __divmoddi4(a, b, rem);
}
