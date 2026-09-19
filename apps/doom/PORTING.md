# Porting notes — Doom on koppi-os

`apps/doom` runs **id Software's Doom** (via [doomgeneric]) as an ordinary
ring-3 program. This file records what the port adds, what it changes, and
what it does not do.

[doomgeneric]: https://github.com/ozkl/doomgeneric

## Getting a WAD

The engine is free software; the game data is not. Nothing in this repository
ships a WAD, and `.gitignore` keeps one from being committed by accident.

Put an IWAD in the repository root as `doom1.wad` and the build stages it on
the boot RAM disk:

```bash
cp /path/to/doom1.wad .        # the Doom shareware IWAD, 4,196,020 bytes
make iso
```

`hda.sh` grows the RAM disk from 8 MiB to 16 MiB when it finds one, so the
image only carries that weight when you ask for it.

The port was developed and tested against the **shareware `doom1.wad`**
(4,196,020 bytes, 1264 lumps). The other names `d_iwad.c` recognises —
`doom.wad`, `doom2.wad`, `plutonia.wad`, `tnt.wad` — are searched for too but
have not been tried here.

A WAD too big for the RAM disk (a Freedoom IWAD is ~30 MiB) can go on a real
volume instead: the port searches `/rd`, `/hda`, `/fda` and the console's
working directory, and `-iwad /hda/doom2.wad` names one outright.

## Playing

```
doom                      # from the shell; also `start /rd/doom`
doom -warp 1 1 -skill 4   # straight into E1M1
doom -iwad /hda/doom2.wad
doom -savedir /hda/       # keep saves on a disk that survives a reboot
doom -timedemo demo1      # render the built-in demo as fast as the box can
```

Vanilla controls: **arrows** move and turn, **Ctrl** fires, **space** uses,
**Alt** strafes, **Shift** runs, **1**–**7** pick a weapon, **Tab** is the
automap, **Esc** is the menu. Quit from the menu (or `-timedemo`, which exits
when it is done) and the desktop comes back.

The game takes the whole screen at 320x200 and the kernel scales it up by the
largest whole number that fits, centred on black — 2x on a 640x480 or 800x600
display, 4x on a 1440x900 laptop panel. Resize the QEMU window with
`-vga virtio` and the next frame follows it.

## Layout

```
src/                  vendored doomgeneric. Only the platform files that do
                      not apply here were dropped (SDL, X11, Win32, Allegro,
                      emscripten, the OPL/PC-speaker music backends,
                      w_file_posix/w_file_win32/w_file_stdc). Five edits,
                      each marked with a "koppi-os:" comment — see below.
shim/                 the i386 / freestanding C library
doomgeneric_koppi.c   the platform layer: the six DG_* entry points
w_file_koppi.c        the WAD backend (whole file in memory)
doom.lds              flat ring-3 image at 8 MiB, entry `main`
```

## How a frame reaches the screen

The kernel grew four calls for this, plus one each for input and sleeping
(`syscall.c`, numbers 22-27). They are not Doom-specific: any program that
wants the whole display uses the same four.

```
gfx_open(w,h)   park the compositor, blank the screen, switch the keyboard
                into raw-scancode mode
gfx_palette(p)  install 256 entries of 0x00RRGGBB
gfx_blit(pix)   present one w*h 8-bpp indexed frame
gfx_close()     hand the screen back
```

The frame crosses the boundary **indexed, not true-colour**: 320x200 is
64 KiB paletted against 256 KiB at 32 bpp, and the engine already renders into
an 8-bpp buffer (`-DCMAP256`), so the palette lookup and the integer upscale
happen once, in the kernel, straight into the framebuffer shadow
(`video_blit8` in [`video.c`](../../video.c)).

Two details that are easy to get wrong:

* **The grab is taken on the first frame, not in `DG_Init`.** Everything
  between the two — finding the IWAD, loading it, building the textures —
  prints its progress, and a grabbed screen shows none of it. Waiting means a
  startup that fails reports itself on a console the player can still read.
* **A process that dies holding the screen still gives it back.** `remove_proc`
  ([`proc.c`](../../proc.c)) drops the grab when it reaps a process, so a
  segfault mid-game leaves the desktop frozen on one frame rather than
  forever.

## Input

The console keyboard ring carries decoded ASCII of key *presses* only, which
is no use to a game: it cannot say that the player let go of the forward key,
and it has no entry for the arrows, Ctrl or Alt at all. `keyboard.c` therefore
has a **second ring** that records raw scancodes with their make/break bit and
their 0xE0 prefix, filled by the same IRQ and drained by `getscan` (#26).

Two rings rather than one consumer switching modes: the ASCII ring has exactly
one consumer and a second reader would split the keystrokes between them. Both
rings are flushed when the mode changes, so nothing typed during a game turns
up in the shell afterwards.

A USB keyboard works too — [`usb_hid.c`](../../usb_hid.c) maps HID usages back
to set-1 scancodes and reports releases and modifiers, which the ASCII path
had no reason to track. That is the path a machine with no PS/2 controller
(a MacBook) has to use.

## Files

The kernel VFS reads a FAT chain forward, 512 bytes per syscall, and cannot
seek; the only write is `spit`, which replaces a whole file in one call. The
shim's `<stdio.h>` is shaped to that rather than pretending otherwise:

* **Reading** — `fopen` slurps the file into memory, so `fread`, `fseek` and
  `ftell` are pointer arithmetic afterwards and are exact.
* **Writing** — output accumulates in a growable buffer and `fclose` hands it
  to `spit`. Nothing reaches the disk before `fclose`.
* **The WAD** — `w_file_koppi.c` takes the buffer `fopen` produced
  (`__koppi_steal`) and gives it to `w_wad.c` as `mapped`, the path the engine
  already has for an mmap()ed WAD. Lumps are then pointers into that buffer
  and are never copied again, so the 4 MiB the shareware IWAD costs *replaces*
  what the zone would otherwise hold rather than adding to it.
* `remove` and `rename` go through the `run` syscall to the console's own `rm`
  and `mv` — the same door the shell uses to reach every coreutil. Doom calls
  them once each, when a save game replaces its predecessor; growing the ABI
  for that seemed the worse trade.

Save games live in `/rd/` by default; `-savedir /hda/` puts them somewhere
that survives a reboot. Their names (`doomsav0.dsg`, `temp.dsg`) are already
8.3, which matters: the FAT driver cannot create directories, so the port
keeps them in a flat directory that already exists. The config file has the
same path but is never written — see the limitations below.

## The C library (`shim/`)

| file | provides |
| --- | --- |
| `ksys.h` | `int 0x72` stubs, one asm statement each, and the call numbers |
| `stdio.c/.h` | the three stream kinds above, `fprintf`, `sscanf`, `remove`, `rename` |
| `stdlib.c/.h` | `malloc`/`free`/`realloc`/`calloc` over the heap syscalls, `exit` + `atexit`, `strtol`, `strtod`, `atoi`, `atof`, `abs`; `getenv` returns NULL and `system` fails, which is what puts `I_Error` on its plain-text path |
| `string.c/.h` | standard-signature `<string.h>` plus `strdup` and `str[n]casecmp` |
| `libm.S` + `libm.c` | `<math.h>` on the x87 FPU (vendored from the Lua port) |
| `printf.c/.h` | vendored from the kernel tree (eyalroz/printf) |
| `ctype.h`, `errno.h`, `assert.h`, `inttypes.h`, `strings.h`, `unistd.h`, `fcntl.h`, `io.h`, `sys/{types,stat,time}.h` | the headers the engine includes |

`shim/` borrows `printf`, `string`, `ctype` and the x87 `libm` from
[`apps/lua/shim`](../lua/shim); the stream and allocator code is new, because
Lua never needed to write a file or seek in one.

`sscanf` handles `%d %i %u %x %o %c %s`, a field width and a literal prefix —
the conversions `M_StrToInt` and the config reader actually use. It is not a
general `sscanf` and does not pretend to be.

## Changes to the vendored engine

Five, each marked with a `koppi-os:` comment:

| file | change |
| --- | --- |
| `i_system.c` | `I_Quit` ends with `exit(0)`. Upstream leaves that to SDL, so without `ORIGCODE` the quit menu ran the exit hooks and fell back into the game loop. |
| `i_system.c` | `I_ConsoleStdout` returns 1. Stdout really is a console here, and saying so keeps `I_Error` on the plain-text path instead of shelling out to `zenity`. |
| `d_iwad.c` | `BuildIWADDirList` also looks in `/hda`, `/fda` and the working directory. |
| `m_config.c` | `GetDefaultConfigDir` returns `/rd/`, or `-savedir`. There is no home directory and no per-process working directory to default to. |
| `m_config.c` | `M_GetSaveGameDir` returns the config directory unchanged instead of a `.savegame/` subdirectory of it, because the FAT driver cannot create one. |

Everything else — the renderer, the game logic, `w_wad.c`, `z_zone.c` — is
doomgeneric as it ships.

## Known limitations

* **No sound.** `i_sound.c` is compiled with its backends off, exactly as
  upstream doomgeneric ships it. The kernel has a working HD Audio / SB16
  path ([`hda.c`](../../hda.c)) and wiring Doom's mixer to it is the obvious
  next step, but nothing here does it yet.
* **No mouse.** `usemouse` is 0. The PS/2 mouse driver reports absolute
  position for the desktop cursor, not the relative deltas `ev_mouse` wants.
* **No config persistence.** `SaveDefaultCollection` and
  `LoadDefaultCollection` are inside `#if ORIGCODE` upstream, so key rebinds
  last only as long as the process. (`LoadDefaultCollection` wants `fscanf`,
  which the shim does not have.) Save games do work.
* **Save games live on a RAM disk** unless you pass `-savedir /hda/`, so by
  default they do not survive a reboot.
* **No Ctrl-C**, as everywhere else on this OS. Quit from the menu.
* **One program at a time may hold the screen**, and the shell that spawned it
  is blocked until it exits — the same arrangement `lua` and `cc` run under.
* `-devparm`, `-record`/`-playdemo` to a file, and the network game modes are
  untested; `D_CheckNetGame` is the stub doomgeneric ships.

## Testing

[`test/doom-boot.sh`](../../test/doom-boot.sh) boots `os.iso` headless, starts
the game over the serial console, drives it through the QEMU monitor and saves
a screenshot per step, so the port can be checked without a display:

```bash
make qemu-doom               # all six scenarios -> /tmp/doom-boot
test/doom-boot.sh play       # just one: demo|menu|play|quit|timedemo|noiwad
VGA=std test/doom-boot.sh demo   # the 24-bpp 800x600 path instead of virtio
```

The scenarios cover the attract-mode demo, the menu (arrow keys, so the
0xE0-prefixed scancodes), held movement keys, quitting back to a usable
shell, `-timedemo`, and a missing IWAD.
