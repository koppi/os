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

Sound and music play through whichever card the kernel found; `-nosound`,
`-nosfx` and `-nomusic` turn them off. The volume keys and the console's
`sound 0-100` set the level while the game runs, the same as for the module,
and the options menu has Doom's own separate sound and music sliders.

The game takes the whole screen at 320x200 and the kernel scales it up by the
largest whole number that fits, centred on black — 2x on a 640x480 or 800x600
display, 4x on a 1440x900 laptop panel. Resize the QEMU window with
`-vga virtio` and the next frame follows it.

## Layout

```
src/                  vendored doomgeneric. Only the platform files that do
                      not apply here were dropped (SDL, X11, Win32, Allegro,
                      emscripten, the OPL/PC-speaker music backends,
                      w_file_posix/w_file_win32/w_file_stdc). Nine edits,
                      each marked with a "koppi-os:" comment — see below.
opl/                  the music support, vendored from chocolate-doom:
                      Nuked OPL3 (opl3.c), the sequencer's callback queue,
                      the MIDI reader and i_oplmusic.c, unmodified. Plus
                      opl_koppi.c, this port's backend for them.
shim/                 the i386 / freestanding C library
doomgeneric_koppi.c   the platform layer: the six DG_* entry points
w_file_koppi.c        the WAD backend (whole file in memory)
i_sound_koppi.c       DG_sound_module: an eight-channel mixer
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

## Sound

Upstream doomgeneric ships `i_sound.c` with its backends compiled out, because
the ones it has are SDL_mixer and Allegro. The hook is there, though --
`DG_sound_module` under `FEATURE_SOUND` -- so the port defines that flag and
fills it in (`i_sound_koppi.c`).

What SDL_mixer would have done has to be done by hand, so that file is a
mixer: up to eight voices of 8-bit DMX samples, summed into the interleaved
stereo frames the kernel's PCM ring takes (`snd.h`, syscalls 28-31).

```
snd_open()          claim the card -> the sample rate to produce
snd_avail()         room left, in frames
snd_write(buf, n)   queue stereo frames -> how many were taken
snd_close()         give it back
```

Some of it falls out of decisions already made elsewhere:

* **A cached sound costs nothing.** The WAD is mapped whole, so
  `W_CacheLumpNum` hands back a pointer into it; a sfxinfo's `driver_data` is
  three fields saying where in that mapping its samples are. None of the
  sample cache, eviction or `snd_cachesize` accounting the SDL backend
  carries is needed.
* **Nothing is pre-converted.** Samples stay 8-bit at their own rate (11025 Hz
  in Doom's case) and are stepped through with a 16.16 phase accumulator while
  mixing -- an add and a table lookup per output sample, against expanding
  every sound in the game to eight times its size at load.
* **Vanilla's panning curve**, because that is what the game was voiced for:
  each side falls off with the square of the distance from it.
* **The ring is the clock.** `Update()` renders exactly what `snd_avail`
  reports and no more, so the mixer follows the sample rate rather than the
  frame rate, at 35 fps or at 1000.

On the kernel side `snd.c` is the buffer between a program that produces
audio when it feels like it and a card that consumes it at a fixed rate. Both
backends drain it ahead of the MOD player, so a game is never heard over the
module, and the module becomes audible again when the program closes the
stream -- or when `remove_proc` closes it on the program's behalf, the same
safety net the screen grab has.

## Music

Doom's music is MUS, a packed MIDI, and what turned it into sound in 1993 was
an OPL2 chip. There is no OPL here, so `opl/` carries chocolate-doom's
emulation of one -- Nuked OPL3, the callback queue its sequencer runs on, the
MIDI reader and `i_oplmusic.c` -- all unmodified. doomgeneric dropped this
support (its own music backends hand MIDI to SDL_mixer or Allegro), so it
comes from chocolate-doom rather than from the tree in `src/`.

What this port writes is `opl_koppi.c`, which replaces two upstream files at
once: `opl.c`, the layer that picks between a real OPL and an emulated one,
and `opl_sdl.c`, the emulated one. Neither has anything to choose between
here -- there is no hardware OPL on any machine this runs on -- so the driver
table is an indirection to nowhere and what is left is the synthesiser wired
to this system's clock.

That wiring is the interesting part. Upstream, `opl_sdl.c` is a post-mix
hook: SDL_mixer calls it, and it renders and advances musical time in
response. Here the game's own mixer is in charge, so `OPL_Koppi_Render` is a
pull, and **musical time advances only as samples are consumed**. Nothing is
scheduled against a wall clock, so the score cannot drift away from the shots
fired over it however the frame rate wanders. Rendering stops at the next
sequencer callback rather than running to the end of the block, so a note
lands on the sample it was written for.

It is also all one thread -- the mixer, the MIDI callbacks it runs, and every
`I_*Song` call from the game loop -- so `OPL_Lock` and `OPL_Unlock` have
nothing to do, where upstream needs two mutexes for SDL's audio thread.

Nuked builds its 16 KB waveform table at startup (`OPL_WF_TABLE_RUNTIME=1`)
rather than carrying `wf_rom.h`, which is a thousand lines of the same data.

`I_OPL_RegisterSong` converts MUS to MIDI in memory and then writes it to a
temporary file for the MIDI reader to open again. That round trip is
upstream's, kept rather than unpicked: it costs one small write and read on
the RAM disk per song -- a handful of times in a session -- and `M_TempFile`
now points at the config directory, because there is no `/tmp` on a system
whose VFS mounts whole volumes rather than a directory tree.

Two latency knobs, and they add up:

| | frames | ms at 44.1 kHz |
| --- | --- | --- |
| `HDA_STREAM_HALF_FRAMES` (hda.h), x2 for the ring | 1024 | ~46 |
| `SND_RING_FRAMES` (snd.c) | 4096 | ~93 |

The HD Audio figure used to be 4096 frames per half -- 186 ms, fine for music
and far too much for a game. It is now polled every 2 ms instead of 5, which
keeps the same wide margin against a missed DMA crossing in a quarter of the
window.

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

Nine, each marked with a `koppi-os:` comment. The first six were needed to
run at all; the last three are the newer interfaces the vendored music
support in `opl/` expects, which this tree predates.

| file | change |
| --- | --- |
| `i_system.c` | `I_Quit` ends with `exit(0)`. Upstream leaves that to SDL, so without `ORIGCODE` the quit menu ran the exit hooks and fell back into the game loop. |
| `i_system.c` | `I_ConsoleStdout` returns 1. Stdout really is a console here, and saying so keeps `I_Error` on the plain-text path instead of shelling out to `zenity`. |
| `d_iwad.c` | `BuildIWADDirList` also looks in `/hda`, `/fda` and the working directory. |
| `m_config.c` | `GetDefaultConfigDir` returns `/rd/`, or `-savedir`. There is no home directory and no per-process working directory to default to. |
| `m_config.c` | `M_GetSaveGameDir` returns the config directory unchanged instead of a `.savegame/` subdirectory of it, because the FAT driver cannot create one. |
| `i_sound.c` | the `<SDL_mixer.h>` include also requires `ORIGCODE`. `FEATURE_SOUND` selects *a* platform sound module, not SDL's specifically. |
| `i_sound.c` / `.h` | `InitMusicModule` points at `music_opl_module` directly, so there is no `DG_music_module` wrapper forwarding to it; `opl_driver_ver_t` and `I_SetOPLDriverVer` are declared for it. |
| `doomtype.h` / `i_swap.h` | `PACKED_STRUCT` and the two big-endian swaps. chocolate-doom spells a packed struct differently and gets its byte swapping from SDL; MIDI is big-endian, which nothing in a WAD is. |
| `m_misc.c` / `.h` / `i_system.c` | `M_fopen`, `M_remove` and `I_Realloc`, three wrappers the newer code calls, plus `M_TempFile` pointing at the config directory instead of `/tmp`. |

Everything else — the renderer, the game logic, `w_wad.c`, `z_zone.c` — is
doomgeneric as it ships.

## Known limitations

* **`-nosfx` silences the music too.** The PCM stream is opened by the sound
  module, and the mixer that pumps the synthesiser is the sound module's
  `Update`, so with no sound effects nothing drives the music either.
  `-nomusic` on its own works as expected.
* **No mouse.** `usemouse` is 0. The PS/2 mouse driver reports absolute
  position for the desktop cursor, not the relative deltas `ev_mouse` wants.
* **Mono on a Sound Blaster.** The SB16 path runs the card in mono, so the
  mixer's stereo output is downmixed on the way out and the panning is lost.
  HD Audio, which is what a real laptop and the default QEMU machine have,
  is stereo.
* **OPL2, not OPL3.** Nuked emulates an OPL3 and `OPL_Init` says so, but the
  extra voices only come on with `DMXOPTION=-opl3`, and there is no
  environment to set it in. Doom's music was written for an OPL2 anyway.
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
make qemu-doom               # all seven scenarios -> /tmp/doom-boot
test/doom-boot.sh sound      # one: demo|menu|play|quit|timedemo|noiwad|sound
VGA=std test/doom-boot.sh demo   # the 24-bpp 800x600 path instead of virtio
```

The scenarios cover the attract-mode demo, the menu (arrow keys, so the
0xE0-prefixed scancodes), held movement keys, quitting back to a usable
shell, `-timedemo`, and a missing IWAD.

`sound` points QEMU's audio backend at a WAV file instead of a speaker, so
what came out is a file you can measure and listen to. It reports peak, RMS
and stereo/mono per second, and a good run reads: silence while the machine
boots (the module is muted from boot), fifteen seconds of stereo once the
game has the stream, silence again after it quits, then the module in mono
once the console's `sound on` unmutes it.
