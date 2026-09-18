# Porting notes — Lua 5.4.8 on koppi-os

`apps/lua` runs the **reference Lua 5.4.8** interpreter as an ordinary ring-3
program (`start hda/lua`). This file records what the port adds and why.

## Layout

```
src/        vendored Lua 5.4.8 (lua.org). Unmodified except:
              - luaconf.h: a 2-line change (see below)
              - lua.c renamed to frontend.c so the l*.c wildcard skips it
shim/       the i386 / freestanding platform layer (hand-written + 2 vendored)
openlibs.c  luaL_openlibs() minus io/debug, os trimmed
test.lua    self-test, staged as /rd/t.lua
mod.lua     `require` demo, staged as /rd/mod.lua
Makefile    builds `lua`; links ../../lib/{stdlib,system_calls,unistd}.o + ../../arith64.o
```

## Build configuration (`Makefile`)

* `-DLUA_USE_C89` — selects Lua's C89 code paths: `setjmp`/`longjmp` (not
  `_setjmp`), the built-in hex-float parser (`lua_strx2number`), and
  `l_sprintf` → `sprintf`. We then point `l_sprintf` back at `snprintf`.
* `luaconf.h` is patched so `LUA_C89_NUMBERS` stays **0** even under
  `LUA_USE_C89`: i386 GCC has `long long` and `double`, so `lua_Integer` is
  64-bit and `lua_Number` is `double` (stock Lua semantics) rather than C89's
  32-bit `long`.
* `lua_writestring` / `lua_writeline` / `lua_writestringerror` are redefined
  (via `-D`) to go straight to the `write` syscall.
* `-fno-tree-loop-distribute-patterns` — stops GCC turning the byte loops in
  `shim/string.c` into calls to `memset`/`memcpy` (infinite recursion).
* `LUA_PATH_DEFAULT = "/rd/?.lua;/rd/?/init.lua;/fda/?.lua;./?.lua"`, no C path.
* `l_system(cmd)` and `lua_getlocaledecpoint()` are stubbed via `-D`.

## Platform layer (`shim/`)

| file | provides |
| --- | --- |
| `ksys.h` | `int 0x72` stubs (`ksysN`) — one asm statement each, safe at `-Os` |
| `libm.S` + `libm.c` | `<math.h>` on the **x87 FPU**; `pow` special-cases negative bases |
| `port_asm.S` + `setjmp.h` | i386 `setjmp`/`longjmp` (6 words: ebx esi edi ebp esp eip) |
| `stdio.c` + `stdio.h` | `FILE` shim: console out (write syscall), console in (gets), FAT files (fopen/fread/fclose) |
| `string.c` + `string.h` | full standard-signature `<string.h>` (kernel `lib/string.o` is **not** linked — its sigs differ) |
| `stdlib.c` + `stdlib.h` | `strtod` (decimal + inf/nan), `strtol`, `abs`, `abort`, `getenv`→NULL, `errno`, `setlocale`/`localeconv` |
| `time.c` + `time.h` | `time`/`clock` syscalls, `gmtime`/`localtime`/`mktime`/`strftime` (civil-time algorithm), `difftime` |
| `ctype.h` | ascii `<ctype.h>` (no `__ctype_b_loc`) |
| `printf.c` + `printf.h` | vendored from the kernel tree (eyalroz/printf); header trimmed to drop `<types.h>`. Gives `snprintf`/`sprintf`/… |
| `port.c` | `__udivmoddi4` → `arith64.c`'s `__divmoddi4` |
| `signal.h`, `locale.h`, `errno.h` | stubs |

## Kernel changes this port needed

All of these are useful beyond Lua and are described in the top-level README:

* **`sched.c` / `fpu.c` / `thread.c`** — per-thread `fxsave`/`fxrstor` on
  context switch, so float-using ring-3 code is not corrupted by other threads.
* **`elf.c`** — `load_elf_relocate` mapped every page of a segment onto the
  frame backing its first page; now each page gets its own frame and `.bss` is
  covered. `image_size` is derived from the segment span, not the entry point.
* **`proc.c` / `heap.c`** — 256 KiB user stack, 16 KiB kernel stack, growable
  heap (`heap_grow`), fresh user pages zeroed, `argv[argc] == NULL`.
* **`syscall.c`** — added `realloc`, `write`, `fread`, `time`, `clock`; the
  per-call debug print is now behind `SYSCALL_TRACE` (default off).
* **`uart.c`** — serial RX is fed into the console input ring (headless use);
  16550 FIFO enabled.
* **`video.c`** — the framebuffer redraw thread parks itself in text mode
  instead of spinning and stealing console keystrokes.
* **`keyboard.c`** — `gets` reports Ctrl-D as end-of-input.

## Known limitations

* No `io` or `debug` library. `os` is read-only + `exit`.
* No Ctrl-C — a runaway script needs `reboot`.
* `strtod` chunks by 10^22, so values past ~1e22 magnitude can be ~1 ULP off
  (`1e100` prints as `9.99…e+99`); everything within 10^±22 is exact.
* One Lua process at a time in practice (the console `start` blocks on it).
* `fopen` is read-only.
