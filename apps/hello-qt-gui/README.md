# hello-qt-gui — graphical "Hello, Qt6!"

A real Qt 6.8 GUI application on this kernel: a `QRasterWindow` painted with
`QPainter` (gradients, anti-aliased paths, translucent fills, FreeType +
HarfBuzz text in a Unifont subset), animated by a `QTimer` inside a real
`QGuiApplication::exec()` event loop, and presented on the framebuffer through
the full-screen-grab syscalls Doom uses. **Esc** or **Enter** quits and hands
the desktop back.

Everything Qt-side lives in [`third_party/qt6-gui`](../../third_party/qt6-gui)
(real, vendored source; start with its README).

## Build and run

```
make -C apps/hello-qt-gui -j4      # ~20 min the first time, incremental after
make initrd.img                    # hda.sh stages hqtgui + /rd/font.ttf if the binary exists
make qemu-iso                      # needs the GRUB framebuffer boot; then type: hqtgui
```

It is deliberately not part of `make -C apps` (about 560 translation units).
`-kernel` boots stay in VGA text mode, where `gfx_open` correctly declines;
use the ISO (or real hardware).

Automated check (headless; builds nothing, needs `make iso` first):

```
make qemu-qt-gui          # start / animate / quit, each judged from the screenshot's pixels
```

To script an ad-hoc run and capture screenshots without a display:

```
MON_SCRIPT="28:screendump a.ppm;31:sendkey esc;36:screendump c.ppm" \
  third_party/qt6-gui/tools/boot_test.sh /tmp/qt6run
```

## Files

* `hello_qt_gui.cpp` — the app. `argv[0]` is the absolute `/rd/hqtgui` on
  purpose: with no `PATH` search, a bare name makes `applicationDirPath()`
  empty and `QLibraryInfo` asserts.
* `hello_qt_gui.lds` — same layout as `apps/hello-qt` (image at 8 MiB).
* `unifont-subset.ttf` — see [`FONT-LICENSE.md`](FONT-LICENSE.md).
* `Makefile` — builds the vendored tree with the exact flags the port needs.
