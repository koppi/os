# `cc` — a tiny self-hosting two-pass C compiler

`cc` runs as a ring-3 program on the OS and compiles a single C file straight to
a runnable ELF executable — there is **no assembler or linker step**, `cc` is
front end + i386 code generator + ELF writer in one. It is written in the same C
subset it accepts, so it compiles itself: on the OS,

```
start hda/cc  hda/cc.c -o hda/cc2      # cc  compiles its own source
start hda/cc2 hda/cc.c -o hda/cc3      # the result compiles it again
sum hda/cc2 ; sum hda/cc3              # identical checksums  => fixed point
```

## Pipeline

```
prelude.c  ─┐
            ├─►  Pass 1: lex → parse → semantic analysis  (typed AST + symbols)
<input>.c  ─┘    Pass 2: AST → i386 machine code → relocation fixups
                         → one PT_LOAD ELF at 0x800000, fixups patched
                         → write_file()  (syscall 16)
```

* **One translation unit.** `cc` prepends `prelude.c` (the runtime library)
  unless `-nostdlib` is given, so a program never needs to link anything. Line
  and file tracking reset at the user-file boundary, so errors read
  `foo.c:42: error: ...`.
* **No preprocessor.** The lexer skips any line whose first non-blank character
  is `#`, so `#include <stdio.h>` and friends are silently ignored — the libc
  surface a program expects comes from the prepended prelude. Use `enum` for
  named constants.
* **Two builtins**, lowered inline by the code generator (no general inline asm):
  * `__syscall0(n)` … `__syscall3(n,a,b,c)` → `int $0x72`
    (`eax`=n, `ebx`/`ecx`/`edx`=args, result in `eax`).
  * varargs use the plain i386 trick `int *ap = ((int *)&last_named_arg) + 1;`
    (works because `cc` passes every argument on the stack, in order).

## Accepted language

Supported: `void _Bool char short int long unsigned signed`, `enum`, pointers
(any depth), arrays, `struct`, `union`, `typedef`; functions, prototypes,
recursion, function pointers, **varargs call sites**; every C operator including
`?:`, `&&` `||`, `,`, `sizeof`, casts, `++`/`--`, compound assignment;
`if/else while for do break continue return switch/case/default goto:`;
string-literal concatenation; global / local / `static` variables with constant
global initializers and brace initializers for arrays and structs.

`const volatile register inline restrict _Noreturn` are parsed and ignored.

**Not supported** (diagnosed, never silently mis-compiled): `float`/`double`;
`long`/`long long` are **32-bit** (with a note); passing or returning a
`struct`/`union` **by value** (use a pointer); `<stdarg.h>` (use the `&arg`
trick above); the preprocessor; bitfields, VLAs, `_Generic`, compound literals,
designated initializers, wide strings.

## ABI / code model

* cdecl: arguments pushed right-to-left, caller cleans up, return value in
  `eax`. Parameters at `[ebp+8+4i]`, locals at `[ebp-n]`. The code generator
  only ever uses `eax`/`ecx`/`edx` (all caller-saved) plus `ebp`/`esp`, so
  prologue/epilogue are just `push ebp; mov ebp,esp; sub esp,N` … `leave; ret`.
* Stack-machine lowering: every expression leaves its value in `eax`; a binary
  op is `gen(lhs); push; gen(rhs); mov ecx,eax; pop eax; <op>`. No register
  allocator, no optimiser.
* Output: `[52-byte ELF header][32-byte program header][code][rodata+data]`,
  one `PT_LOAD` at `0x800000`, `p_flags=RWX`, `e_entry = 0x800000 + off(main)`.
  All globals and strings are emitted into the file (no `.bss`), so the output
  is byte-deterministic and the OS ELF loader path stays trivial.
* `main` is the entry point (the OS loader jumps straight to it); it returns its
  exit code in `eax` like any other function.

## Building

`make` (run from the repo root, or `make -C apps/cc`) builds the OS binary
`cc` the same way every other app is built — host `gcc -m32` + `ld` + the shim
libc in `../../lib`, with `io_os.c` providing the six primitives `cc.c` needs
(`xalloc`, `xrealloc`, `sys_readfile`, `sys_writefile`, `sys_out`, `sys_exit`).
This bootstrap does **not** depend on `cc`'s own code generator existing yet.

For fast iteration on Linux: `make -C apps/cc native` builds `./cc-native`
(same `cc.c`, glibc-backed primitives from `sys_native.c`). `cc-native` emits
OS ELFs exactly as the on-OS `cc` does — the two produce byte-identical output
for any input, which is how self-hosting is checked without a QEMU boot.

## Running it

Boot with a single core while compiling:

```
make qemu-nox SMP=1
> start hda/cc hda/cc.c -o hda/cc2
```

`cc` is compute- and syscall-heavy and currently trips a pre-existing kernel
SMP context-switch bug under load (a repeated fault at a fixed bogus address);
the same code compiles cleanly on `-smp 1`. This is orthogonal to the compiler
and would affect any long-running ring-3 program.

## Runtime file I/O

The prelude reads a whole file straight into a heap buffer (`malloc(n + 512)`,
one `fread` block per 512 bytes, last block over-reads into the padding) rather
than through a large stack scratch buffer — on this OS the top of a process's
user stack sits directly under its kernel stack, and a multi-hundred-byte stack
buffer used as a syscall target that close to the boundary can be clobbered by
kernel-side activity during the call. `io_os.c` does the same.
