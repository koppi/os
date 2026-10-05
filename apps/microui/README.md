# microui in ring 3

A shared runtime that lets a **userspace** program put a microui window on the
desktop. Two programs use it:

* [`apps/calc`](../calc) — a four-function calculator
* [`apps/clock`](../clock) — an analog clock

This directory builds no binary of its own; `mui.mk` is included by each app's
`Makefile`.

```
make -C apps/calc && make -C apps/clock
make qemu-iso                 # then, at the shell:
                              #   calc          a window on the desktop
                              #   clock &       ...and a second one beside it
                              #   calc -f       the whole screen instead
make qemu-microui             # or drive it all headless (test/microui-boot.sh)
```

Esc quits, and so does the window's close box.

## Why there is anything to port at all

The kernel already draws its desktop with microui: [`microui.c`](../../microui.c)
is vendored at the repo root, [`graphics.c`](../../graphics.c) drives it and
[`renderer.c`](../../renderer.c) renders its command list into the 32-bpp
framebuffer shadow. None of that is reachable from ring 3 — the shadow is
kernel memory, and `draw_rect`/`draw_string` are kernel functions.

What ring 3 *does* have is an 8-bpp indexed surface of its own, which the
kernel will put on screen one of two ways:

* **as a window** ([`wm.h`](../../wm.h), syscalls 39–43). The desktop keeps
  drawing, the window has a title bar to drag it by and a close box, up to four
  programs can have one at once, and the keyboard reaches a program only while
  its window is focused. The window manager owns the frame; the program owns
  the content and never learns where on screen it is.
* **as the whole screen** ([`video.h`](../../video.h), syscalls 22–25 — the
  grab the Doom port added). The desktop is parked until the program gives it
  back. This is the fallback when there is no desktop to put a window on, and
  what `-f` asks for.

Add the raw keyboard ring and the pointer and that is everything a GUI needs.
So the port is a second renderer for the same toolkit, plus an event pump —
and `microui.c` itself is compiled a **second time**, for ring 3, rather than
copied or forked.

## The pieces

| file | what it is |
| --- | --- |
| `mui.c` / `mui.h` | the runtime: grab, paletted renderer, input pump, frame loop, drawing primitives |
| `mkfont.c` | build-machine tool: slices the ASCII range out of `unifont.sfn` into `mui_font.h` |
| `mui.mk` | the shared build rules an app's `Makefile` includes |
| `shim/` | the three headers the vendored `microui.c` and the apps need that `lib/` does not have |

### Colour: the palette is built, not chosen

`gfx_blit` takes indices, so something has to decide what the 256 slots mean.
The Qt port ([`third_party/qt6-gui`](../../third_party/qt6-gui)) is handed
finished true-colour pictures and has to quantize them into a fixed cube. This
one is not: it sees the *drawing calls*, so it interns each colour into the
next free slot as the frame is painted and uploads the palette just before the
blit. A microui frame uses on the order of a dozen distinct colours, so every
one of them lands exactly — no quantization error anywhere. A frame that
somehow asked for more than 256 falls back to the nearest slot already taken.

The palette is rebuilt every frame. That costs one extra syscall and a 1 KiB
copy, and buys the guarantee that the indices in the surface can never refer to
a palette the kernel no longer has. Slot 0 is always the backdrop, so the
`memset` that clears the surface is also the background fill.

### Text: Unifont, sliced at build time

The kernel draws text with `ssfn.h` out of the 1.2 MiB `unifont.sfn` blob
linked into its image. A ring-3 program can afford neither the blob (bigger
than every app in `apps/` put together) nor a second copy of the glyphs in its
own source. `mkfont.c` runs on the build machine instead and rasterises the 95
printable ASCII glyphs into a 1520-byte table, which is 8×16 and *identical* to
what the kernel console draws, because Unifont is natively an 8×16 bitmap font
and ssfn's console renderer hands back exactly the cell it stores.

`mkfont` uses ssfn's `SSFN_CONSOLEBITMAP_PALETTE` mode — which writes 8-bit
indices, the same thing this runtime wants — together with `CLEARBG`. The
`CLEARBG` is not optional: without it that path writes `ssfn_bg` for *set*
pixels (an upstream slip in the vendored header) and every glyph comes out
blank.

The generated header is not committed. It is a slice of a file already in the
tree, so checking it in would be a second copy of the same glyphs, free to
drift from the console's.

### Drawing things microui has no widget for

microui's command list carries rectangles, text and four built-in icons, and
nothing else — which is a problem for a clock face. `mui_draw_custom()` pushes
a command of its own type into that list, carrying a callback and a rect; the
renderer paints it in list order with the clip rect a widget in that position
would have had. So a custom-drawn thing sits *inside* its window and layers and
clips with it, instead of being painted over the top afterwards. The
primitives it draws with (`mui_fill`, `mui_line`, `mui_disc`, `mui_ring`,
`mui_string`) all clip and intern colour themselves.

### Input

Keys arrive the same way on both paths — a scancode with its break and 0xE0
bits, which is what `getscan` (26) carries and what the window manager's key
events carry — so one table translates both, and Shift and Caps Lock are
tracked here.

The pointer is where the two differ, and the difference is the whole point.
`getmouse` (36) is the raw *device*: relative motion and a button mask, from
which a full-screen program keeps its own pointer position and has to *draw*
the arrow itself, because the kernel's own pointer belongs to the desktop it
just parked. A window's pointer is already a pointer: the desktop has decided
where the window is and whether this program is the one being pointed at, and
delivers a position in the window's own coordinates. Drawing an arrow there
too would put two on screen, a frame apart.

## The shim

Three headers, for the gap between what the vendored `microui.c` includes and
what the ring-3 libc in [`lib/`](../../lib) actually has:

* `io.h` — `microui.c` includes the kernel's port-I/O header for exactly one
  symbol, `halt()`, which its `expect()` macro calls on a failed assertion. In
  ring 0 that is the `hlt` instruction, which ring 3 may not execute: it would
  turn a diagnosable assertion into a general protection fault. `mui.c` defines
  a userspace one that gives the screen back and exits.
* `stdlib.h` — adds `qsort`, which `mu_end()` uses to order root containers by
  z-index and which `include/lib/stdlib.h` has no reason to carry for anyone
  else. `mui.c` implements it as an insertion sort; the one caller sorts at
  most 32 pointers, already nearly in order, once a frame.
* `printf.h` — the repo root's, by path, so that the apps and `microui.c`
  (whose own quoted `#include "printf.h"` resolves next to itself) agree on
  what `sprintf` means without putting the whole kernel include directory on an
  app's search path. The implementation linked in is
  [`apps/doom/shim/printf.c`](../doom/shim/printf.c) — the same upstream,
  already built for ring 3. The kernel's own `printf.c` cannot be reused: it
  takes `con_lock` around every line.

## Writing another one

The frame callback is called inside a window that is already open, so it only
lays widgets out — and therefore cannot tell, and does not need to, which of
the two paths it is on.

```c
#include <printf.h>
#include "mui.h"

static void frame(mu_Context *ctx, void *udata) {
    mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
    mu_label(ctx, "Esc quits");
}

int main(void) { return mui_run("Hello", 200, 120, frame, 0) < 0 ? 1 : 0; }
```

plus a `Makefile` of four lines (`APP`, `OBJ`, an optional `APP_LIB_OBJ` for
more of `lib/`, and `include ../microui/mui.mk`) and a linker script copied
from a neighbour. Then add it to [`apps/Makefile`](../Makefile),
[`hda.sh`](../../hda.sh) and `.gitignore`.

## Licensing

microui is © 2020 rxi, MIT. The glyphs `mkfont` extracts are GNU Unifont,
dual-licensed GPLv2-or-later with the font-embedding exception, and OFL 1.1 —
the same `unifont.sfn` the kernel itself embeds.
