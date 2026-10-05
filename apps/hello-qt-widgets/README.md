# hello-qt-widgets — a QtWidgets test app

Real Qt 6.8 **QtWidgets** on this kernel: a `QApplication` with a `QTabWidget`
whose pages exercise the common controls, drawn by Qt's own Fusion (or Windows)
style into the kernel framebuffer and driven by the keyboard **and** the mouse.

| Tab | What is on it |
|---|---|
| Buttons | push button with a click counter, checkable push button, tool button, check box, a group of radio buttons |
| Sliders | horizontal and vertical slider, dial, scroll bar, spin box and progress bar — all six show one value, whichever you move |
| Input | line edit that echoes what you type, a **Style** combo box that switches the application style (Fusion ⇄ Windows) at run time, a **Fruit** combo box whose popup is a real `QComboBoxPrivateContainer` list view |
| About | Qt version, active style, font, screen size |

Below the tabs: a **Quit** button. **Esc** quits as well and hands the desktop
back. `Tab` / `Shift+Tab` walk the focus chain, arrows move sliders, dials,
tabs and combo selections, `Space` presses the focused button, and typing goes
into the focused line edit or spin box. The mouse clicks, drags and opens
popups.

Every state change is also written to the serial console as
`widgets: <what happened>` (`button clicked 3`, `slider 26`, `focus QCheckBox
'Enable the thing'`, `combo 1 'Windows'`, ...). That log, not pixel matching,
is what `test/qt-widgets-boot.sh` asserts.

Everything Qt-side lives in [`third_party/qt6-gui`](../../third_party/qt6-gui)
(real, vendored source; start with its README).

## Build and run

```
make -C apps/hello-qt-widgets -j4  # first Qt build ~20 min, shared with hello-qt-gui; then seconds
make iso                           # hda.sh stages hqtwid + /rd/font.ttf if the binary exists
make qemu-iso                      # needs the GRUB framebuffer boot; then type: hqtwid
```

It is deliberately not part of `make -C apps`. `-kernel` boots stay in VGA text
mode, where `gfx_open` declines; use the ISO (or real hardware).

Automated check (headless; builds nothing, needs `make iso` first):

```
make qemu-qt-widgets     # start / keys / sliders / input / style / mouse / quit
```

## Files

* `hello_qt_widgets.cpp` — the app. `argv[0]` is the absolute `/rd/hqtwid` on
  purpose (no `PATH` search; a bare name makes `applicationDirPath()` empty and
  `QLibraryInfo` asserts). The window is sized to the 640×400 `QScreen`.
* `Makefile` — three lines of real content: it includes
  [`qt.mk`](../../third_party/qt6-gui/koppios/qt.mk) and declares the app.
* The font is the same Unifont subset as `apps/hello-qt-gui`
  ([`FONT-LICENSE.md`](../hello-qt-gui/FONT-LICENSE.md)), staged as `/rd/font.ttf`.
