/*
 * test.c - self-checking regression test for cc.  Prints "test: N checks OK"
 * and exits 0 on success, or "test: FAIL at <tag>" and exits 1.
 */
#include <stdio.h>   /* ignored - the prelude provides printf etc. */

int checks = 0;
int failed = 0;

void ok(char *tag, int cond) {
    checks++;
    if (!cond) { printf("test: FAIL at %s\n", tag); failed = 1; }
}

/* --- arithmetic & precedence --- */
int arith(void) {
    ok("mul-add", 2 + 3 * 4 == 14);
    ok("div", 17 / 5 == 3);
    ok("mod", 17 % 5 == 2);
    ok("shift", (1 << 10) == 1024);
    ok("bitops", (0xF0 | 0x0F) == 0xFF && (0xFF & 0x3C) == 0x3C && (0xFF ^ 0x0F) == 0xF0);
    ok("cmp", (3 < 4) && (4 <= 4) && !(5 < 4) && (4 == 4) && (4 != 5));
    ok("ternary", (1 ? 10 : 20) == 10 && (0 ? 10 : 20) == 20);
    ok("logic", (1 && 1) && (1 || 0) && !(0 && 1) && !0);
    unsigned u = 0xFFFFFFFF;
    ok("unsigned-shr", (u >> 28) == 15);
    ok("signed-shr", (-16 >> 2) == -4);
    int x = 5;
    x += 3; ok("plus-eq", x == 8);
    x *= 2; ok("star-eq", x == 16);
    x--; ok("post-dec", x == 15);
    ok("pre-inc", ++x == 16);
    return 0;
}

/* --- pointers & arrays --- */
int arrsum(int *a, int n) { int s = 0; for (int i = 0; i < n; i++) s += a[i]; return s; }

int ptrs(void) {
    int a[5];
    for (int i = 0; i < 5; i++) a[i] = (i + 1) * (i + 1);
    ok("arr-init", a[0] == 1 && a[4] == 25);
    ok("arr-sum", arrsum(a, 5) == 55);
    int *p = a + 2;
    ok("ptr-arith", p[-2] == 1 && p[0] == 9 && p[2] == 25);
    ok("ptr-diff", (p - a) == 2);
    char s[8];
    char *src = "abc";
    int i = 0;
    while ((s[i] = src[i])) i++;
    ok("strcpy-loop", s[0] == 'a' && s[2] == 'c' && s[3] == 0);
    int **pp = &p;
    ok("double-ptr", (*pp)[0] == 9);
    return 0;
}

/* --- structs & unions --- */
struct Point { int x, y; };
struct Box { struct Point lo, hi; char *name; };
union U { int i; char c[4]; };

int area(struct Box *b) { return (b->hi.x - b->lo.x) * (b->hi.y - b->lo.y); }

int structs(void) {
    struct Box b;
    b.lo.x = 1; b.lo.y = 2; b.hi.x = 6; b.hi.y = 9;
    b.name = "b";
    ok("struct-member", b.hi.y == 9);
    ok("struct-ptr", area(&b) == 35);
    struct Point q = { 3, 4 };
    struct Point r = q;          /* struct copy */
    r.x = 99;
    ok("struct-copy", q.x == 3 && r.x == 99 && r.y == 4);
    union U u;
    u.i = 0;
    u.c[0] = 0x41;
    ok("union", u.i == 0x41);
    ok("sizeof", sizeof(struct Point) == 8 && sizeof(int) == 4 && sizeof(char) == 1);
    return 0;
}

/* --- control flow --- */
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int classify(int c) {
    switch (c) {
    case 0: return 100;
    case 1:
    case 2: return 200;
    default: return -1;
    }
}

int flow(void) {
    ok("recursion", fib(10) == 55);
    ok("switch", classify(0) == 100 && classify(2) == 200 && classify(9) == -1);
    int sum = 0;
    for (int i = 1; i <= 100; i++) { if (i % 3 == 0) continue; if (i > 50) break; sum += i; }
    ok("break-continue", sum == 867);
    int n = 0, i = 0;
    do { n += i; i++; } while (i < 5);
    ok("do-while", n == 10);
    int j = 0;
    goto skip;
    j = 999;
skip:
    ok("goto", j == 0);
    return 0;
}

/* --- function pointers --- */
int add1(int x) { return x + 1; }
int dbl(int x) { return x * 2; }

int fnptr(void) {
    int (*f)(int) = add1;
    ok("fnptr-call", f(10) == 11);
    f = dbl;
    ok("fnptr-reassign", f(10) == 20);
    int (*tab[2])(int);
    tab[0] = add1; tab[1] = dbl;
    ok("fnptr-array", tab[0](5) + tab[1](5) == 6 + 10);
    return 0;
}

/* --- varargs (via the __builtin-free &arg trick, like the prelude) --- */
int sumv(int count, ...) {
    int *ap = ((int *)&count) + 1;
    int s = 0;
    for (int i = 0; i < count; i++) s += *ap++;
    return s;
}

int varargs(void) {
    ok("varargs", sumv(4, 10, 20, 30, 40) == 100);
    return 0;
}

/* --- globals with initializers --- */
char *words[] = { "zero", "one", "two", "three", 0 };
int primes[] = { 2, 3, 5, 7, 11, 13 };
struct Point origin = { 0, 0 };

int globals(void) {
    int n = 0;
    while (words[n]) n++;
    ok("global-strtab", n == 4 && words[2][0] == 't');
    ok("global-intarr", primes[5] == 13 && sizeof(primes) == 24);
    ok("global-struct", origin.x == 0 && origin.y == 0);
    return 0;
}

int main(void) {
    arith();
    ptrs();
    structs();
    flow();
    fnptr();
    varargs();
    globals();
    if (failed) return 1;
    printf("test: %d checks OK\n", checks);
    return 0;
}
