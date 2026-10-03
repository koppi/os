# ChipNomad Tracker on koppi-os

[ChipNomad](https://chipnomad.org) is an LSDJ/M8-style tracker for the
AY-3-8910 / YM2149F sound chips, by Megus. This is a port of its
[`cpp-migration` branch](https://github.com/Megus/chipnomad-tracker/tree/cpp-migration)
to this kernel: the real tracker and the real Ayumi emulator, running in ring 3
on the kernel's own framebuffer, PCM ring, keyboard and filesystem.

```
start /rd/cnomad
```

Arrow keys move, **X** is EDIT, **Z** is OPT, **space** plays, **left shift**
is SHIFT, and SHIFT + a direction changes screen — the desktop build's layout,
so [the manual](https://chipnomad.org/manual/) applies unchanged. Hold
**Escape** for about a third of a second to quit (a tap does nothing, so a
stray keystroke cannot drop you out of an unsaved project).

The engine, the project format, the AY emulation, all nineteen screens, the
file browser, the importers and the WAV/VGM/PSG exporters are upstream's own
code, unmodified except where this file says otherwise.

## Layout

| | |
|---|---|
| `../../third_party/chipnomad/` | the vendored tracker and engine (see its `VENDORED-COMMIT`) |
| `bridge/` | the part of upstream's own C++ migration that branch has not finished |
| `koppios/` | this port: `Gfx`, `AudioDevice`, `InputUtils`, `MainLoop`, `FileSystem`, `Assets` |
| `shim/` | the C library surface the tracker needs beyond `lib/` |
| `main_koppios.cpp` | entry point, replacing upstream's SDL-only `platforms/shared/main.cpp` |

ChipNomad abstracts its platform behind six small classes in
`tracker/src/corelib/`, so `koppios/` is the whole of the "porting" in the
usual sense — about 1,100 lines. `bridge/` exists for a different reason.

## The branch does not build

The `cpp-migration` branch is a migration in progress, and it does not compile
for **any** of its own platforms — not macOS, not Linux, not PortMaster. Its
`make -f Makefile.linux linux` stops on the first file:

```
src/app.cpp:2:10: fatal error: corelib_gfx.h: No such file or directory
```

The migration converted `tracker/src/corelib/` from a C API of free functions
into C++ classes (`Gfx`, `FileSystem`, `FontManager`, …), split `src/common.h`
into an `AppSettings` class and a `TrackerState` class, and rewrote the two
SDL backends to match. It did not convert the ~37 files under `src/screens/`
and `src/import/`, or `src/app.cpp` itself — 15,000 lines that still call
`gfxPrint()`, read `appSettings.colorScheme`, and include headers the branch
deleted. `platforms/shared/main.cpp` instantiates a `TrackerApp` class that
exists nowhere in the tree.

So this port had to finish that migration far enough to build. `bridge/` is
that work, and it is deliberately all adapter and no tracker logic:

- **`common.h`** — the globals and free functions the un-migrated code
  declares `extern`, defined in terms of the new classes.
- **`corelib_{gfx,font,file,input,mainloop}.h` + `bridge.cpp`** — one
  forwarding call per entry point, from the old free-function name to the
  bound platform object. The declarations are wrapped in `extern "C++"`
  because two un-migrated headers include their dependencies from inside an
  `extern "C"` block.
- **`tracker_app.{h,cpp}`** — the missing `TrackerApp`: `src/app.cpp`'s
  `appSetup`/`appDraw`/`appOnEvent`/`appCleanup` adapted to the `App`
  interface the new `MainLoop` drives.
- **`audio_init.h`** — construction of the `AudioManager` singleton, which
  `app.cpp` has no way to reach the platform's `AudioDevice` for.
- **`chipnomad_prelude.h`** — force-included into every translation unit, to
  fix two order dependencies once instead of thirty times: headers that use
  types they do not include (`chips/chip_ay.h` names `ChipSetup`,
  `src/project_utils.h` names `Project`), and unqualified uses of names the
  migration moved into `namespace chipnomad`. `src/screens/screens.h` already
  pulls that namespace in at global scope for exactly this reason, with a
  comment calling it a bridge "until the tracker is migrated to explicit
  qualification"; the prelude just hoists it to where every file sees it.
- **`tracker_constants.h`** — two constants `app_settings.h` includes a header
  for that the branch does not contain.

None of this is koppi-specific. If upstream finishes the migration, `bridge/`
goes away and `koppios/` is untouched.

## The five patched vendored files

Everything under `third_party/chipnomad/` is upstream's, verbatim, except for
five files with changes marked inline (`grep -rn koppios third_party/chipnomad`):

- **`tracker/src/tracker_state.h`** — adds the `Engine*` member and
  `initEngine()` that `src/app.cpp` already calls and reads
  (`chipnomadState->engine->player`, …). Upstream's `TrackerState` owns only
  the `Project`, and the `Engine` ended up by value inside the new
  `AudioManager`; declaring it where `app.cpp` expects it was the smaller of
  the two reconciliations.
- **`tracker/src/app_settings.h`** — adds `ColorTheme& colorScheme`, the
  pre-migration name of `colorTheme`, which 60-odd reads under `src/screens/`
  still use. A reference, so both names reach one object.
- **`tracker/src/audio_manager.{h,cpp}`** — takes the `Engine` by reference
  from `TrackerState`, and moves opening the device out of the constructor
  into the `start()`/`stop()` pair `app.cpp` actually calls. Also fixes
  `bufferSize(bufferSize)` in the constructor's initializer list, which read
  the member before initializing it (`-Winit-self`) and sized `renderBuffer`
  from that indeterminate value.
- **`tracker/src/app.cpp`** — two lines. Upstream wrote
  `audio = *new AudioManager(chipnomadState)`, copy-assigning through an
  `AudioManager&` that is not bound to anything yet, with a constructor the
  migrated class does not have.
- **`chipnomad_lib/project_io.cpp`** — exceptions, below.

Plus three outright gaps filled in place:
`platforms/shared/file_system.cpp` defines `extractFilenameWithoutExtension`
as the free function it used to be, so the method `file_system.h` declares was
never defined; `src/screens/edit_fx.cpp` declares `extern FXGroup fxGroups[]`
at block scope, which names a *global*-namespace entity the migration moved
into `namespace chipnomad`; and `src/screens/screen_settings.cpp` indexes an
array with the `quality` field, which the migration turned into an
`enum class`.

## No C++ exceptions

`chipnomad_lib/project_io.cpp` reports parse errors by throwing `const char*`
— 91 throws, caught by two handlers at the bottom of `projectLoad()` and
`instrumentLoad()`. Nothing else in the tree throws.

Real exceptions *do* link against this kernel. `libsupc++.a` and
`libgcc_eh.a` resolve cleanly against `lib/`'s runtime once the image's
`.eh_frame` is handed to `__register_frame_info()` and `_dl_find_object()`
answers "no shared objects"; a test program built that way reaches `main`.
What it does not survive is the first `throw`: the distribution's
`libsupc++` is compiled with `-fstack-protector`, so `__gxx_personality_v0`
and `class_type_info` read their canary from `%gs:0x14`, and this kernel's
segmentation is flat — a ring-3 `%gs` has base 0, so that lands on linear
address `0x14` and page-faults. Fixing it properly means per-process TLS
descriptors in the GDT, reloaded on every context switch: a kernel feature,
not something a port gets to decide.

So the app is built `-fno-exceptions` and `project_io.cpp` uses
`setjmp`/`longjmp` (both real, in `lib/libc_ext.c`). The substitution is
faithful here because every throw carries a `const char*`, every one is caught
by one of those two handlers, and neither the throwing helpers nor the frames
between them have a local with a destructor — POD structs and `FILE*` only —
so there is nothing for unwinding to do that a `longjmp` skips.

## The platform backend

**Graphics.** The kernel hands a ring-3 program the screen as one 8-bpp
indexed surface plus a 256-entry palette (`video.c`, syscalls 22–25), and
`video_blit8()` scales what it is given by a whole number and centres it. So
this backend asks for a *small* surface — 480×320, which is 40×20 of the 12×16
font — and lets the kernel multiply it: pixel-exact at 2× on a 1024×768
desktop, 3× on 1440×900, never resampled, and 150 KB a frame instead of a
megabyte. Indexed colour costs nothing here: a theme is ten RGB values, and
the only other colours on screen are the four shades the waveform display
blends, so the surface *is* the drawing buffer and each RGB value is interned
into a palette slot on first use (about fifty in practice; nearest-match if a
hand-written `.cth` ever pushes past 256).

To trade the letterboxing for a larger font, set `screenWidth` /
`screenHeight` in `settings.txt` — 640×480 selects the 16×24 font, 960×720 the
24×36. The tracker's own font loader (`.cnfont` files) works too.

**Audio.** The kernel's PCM output is a push device — ask `snd_avail` how much
room the ring has, `snd_write` that many interleaved stereo frames, and
whichever card is present drains it at 44.1 kHz (`snd.h`). ChipNomad's
`AudioDevice` is the opposite shape, so `pump()` adapts it, and the ring is the
clock: render exactly as much as there is room for, which is the pacing policy
`apps/doom/i_sound_koppi.c` arrived at too. There is no audio thread — this
kernel has userspace threads, but a process gets one CPU, so a render thread
would interleave with the UI rather than run beside it, for the same total work
plus a shared `Engine` to protect.

**Input.** A screen grab also puts the keyboard in raw mode, which is what
makes key *releases* visible — ChipNomad needs them, since its chords and key
repeat are built on press-and-hold. `getscan` (#26) is drained every frame.
Codes stored in a key mapping are set-1 scancodes, with `0x100` set for the
0xE0-prefixed keys, whose codes otherwise collide with the numeric keypad's.

**Filesystem.** Paths are device-qualified (`/rd` is the boot RAM disk, `/hda`
the persistent scratch disk), names are 8.3, and directories exist on the read
side only — the FAT driver has `touch` and `delete` but no `mkdir`, and only
ever lists a volume's root. So each device is one flat directory, the "create
folder" screen reports an honest failure, and a project saved with a name
longer than eight characters comes back truncated. ChipNomad's own two files,
`settings.txt` and `autosave.cnm`, fit 8.3 exactly. They live on `/hda` when
there is a disk and `/rd` when there is not.

**C library.** The app is built against the real system headers and linked
against this kernel's runtime, never host glibc — the same arrangement as
`apps/hello-qt`, and necessary here because `waveform_display.cpp` uses
`std::function`. `shim/` supplies what `lib/` does not:

- `stdio_koppios.c` — seekable `FILE` streams over whole-file syscalls, the
  same three-stream design as `apps/doom/shim/stdio.c`. Nothing a write stream
  produces reaches the disk before `fclose()`, so a project is either the
  previous version or the new one, never half-written.
- `scanf_koppios.c` — a real `sscanf`. The project format needs `%x`, `%f`,
  the `hh`/`h` length modifiers (`"- Tone on: %hhu"` stores through a
  `uint8_t*`; four bytes there would corrupt the next field) and `%[^\n]`,
  none of which `lib/libc_ext.c`'s has.
- `misc_koppios.c` — `strtod`, the ctype and string entries C++ needs as real
  symbols, `rand`, `clock`, `fabsf`, and the standard spellings of the
  vendored mpaland `printf` (`apps/doom/shim/printf.c`, compiled into this
  app's own object). `libc_ext.c`'s formatter ignores width and precision
  entirely — `%02X` comes out as `A` — and the tracker's whole 40×20 grid is
  built out of those.

`lib/libc_ext.c` gained a `KOPPIOS_APP_STDIO` guard so an app can bring its
own stdio without colliding with the stand-ins; nothing else that links it is
affected.

## Two kernel bugs this port surfaced

Both were pre-existing, both are fixed in this change, and both were found by
the tracker rather than looked for:

- **`fat.c: to_normal_file_name()`** put the dot where it ran off the end of
  the base name into its padding — but a name whose base fills all eight
  characters has no padding, so `MICROEGG` + `CNM` came back as
  `microeggcnm`. That name then failed every lookup (it does not round-trip
  through `to_dos_file_name()`), and `ls`, the shell's completion and any
  program reading a directory all showed it wrong. The dot belongs at the
  fixed boundary between the two fields.
- **`lib/libc_ext.c: qsort()`** staged each element through a `char tmp[256]`.
  The file browser sorts `FileEntry`, which is 260 bytes, so every swap wrote
  four bytes past the end of that buffer onto its own frame. No temporary is
  needed at all.

## Testing

`test/chipnomad-boot.sh` boots `os.iso` headless and drives the tracker
through QEMU's monitor — real scancodes, make and break, the same way
`test/doom-boot.sh` drives Doom. Every step leaves a PNG behind, and the
`play` scenario captures the audio to a WAV and reports peak and RMS per
second.

```
make iso && test/chipnomad-boot.sh all
```

## Content

`hda.sh` stages a theme, eight AY instrument presets, the PT3 pitch tables,
three wavetables and three of upstream's demo songs, renamed to 8.3:

| on disk | upstream |
|---|---|
| `microegg.cnm` | `MICROEGGZ.cnm` |
| `modtimer.cnm` | `ModAndTimerDemos.cnm` |
| `wb7.cnm` | `WB7.cnm` |
| `fifth.ayw`, `nestri.ayw`, `vrc6saw.ayw` | `FIFTH.aywave`, `NESTRI.aywave`, `VRC6SAW.aywave` |
| `bass1.cni`, `lead1.cni`, `bd1.cni`, `snare1.cni`, `hat1.cni`, `clap.cni`, `pluck1.cni`, `waves.cni` | `Bass 1.cni`, `Lead 1.cni`, `BD 1.cni`, `Snare 1.cni`, `Hat 1.cni`, `Clap.cni`, `Pluck 1.cni`, `Waves.cni` |

The ST-01 and ChocolateAmen sample sets are not staged — ChipNomad's AY sample
instruments can load them from `/hda` if you put them there, but a megabyte of
WAVs on an 8 MiB RAM disk is not a good default.

## What is not done

- **Sample playback is untested on real hardware.** The AY sample instrument
  path works in QEMU; nothing here has been run on the X250 or T470s.
- **`mkdir` fails**, so the file browser cannot create folders, and since the
  FAT driver only lists a volume's root there is nowhere to create them.
- **No gamepad.** `InputDeviceType::gamepad` is left unmapped; this kernel's
  USB HID support does not surface one.
- **The sample rate is the kernel's**, not the settings'. `snd_open` reports
  44.1 kHz and the engine is told that; a different `audioSampleRate` in
  `settings.txt` is reported and ignored rather than resampled.
