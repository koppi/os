# apps/qt-lua — Lua with the Qt6 bindings of tsujikiri-qt

`qtlua` is the Lua 5.4 of [`apps/lua`](../lua) with `qt6` registered: the classes of QtCore, QtGui and QtWidgets that [`third_party/qt6-gui`](../../third_party/qt6-gui) builds, bound by [tsujikiri-qt](https://github.com/koppi/tsujikiri-qt) (Lua bindings generated with [tsujikiri](https://github.com/kunitoki/tsujikiri) and [LuaBridge3](https://github.com/kunitoki/LuaBridge3)). A script builds widgets, paints, connects signals to Lua functions; the window is real QtWidgets on the kernel framebuffer, driven by the keyboard and the mouse.

```lua
local qt = qt6
local window = qt.QWidget()
local layout = qt.QVBoxLayout(window)
local label = qt.QLabel("Hello from Lua")
local button = qt.QPushButton("Click")
layout:add_widget(label)
layout:add_widget(button)
button:connect("clicked", function() label:set_text("Clicked!") end)
window:show()
qt.QApplication.exec()
```

This directory is only the build: the generator, the host program, the support layer and the scripts are in tsujikiri-qt (`koppios/`), which has the documentation of what is bound, how, and why some of it is not — [koppios/README.md](https://github.com/koppi/tsujikiri-qt/blob/main/koppios/README.md).

## Build and run

Needs a checkout of tsujikiri-qt next to this repository (as `tsujikiri-qt` or `tsujikiri-qt6`, or `TSUJIKIRI_QT6=/path`), Python 3.10+ and git (tsujikiri and LuaBridge3 are fetched into `koppios/build` of that checkout on first use), and the Qt of `third_party/qt6-gui`, which this shares the object files with.

```bash
make -C apps/qt-lua -j2                                  # about 15 minutes cold; a unit of bindings takes ~1 GB, so -j2, not -j12
make iso                                                 # qtlua is 11.7 MB; with hqtgui, hqtwid and the WAD the 32 MiB RAM disk keeps ~3 MiB free
make qemu-iso                                            # then, at the prompt:  qtlua
```

```
qtlua                       # the widgets demo (/rd/qtlua.lua): Tab, arrows, Space, mouse; Esc quits
qtlua /rd/selftest.lua      # checks the bindings: values, containers, enums, signals, painting, models
qtlua /rd/my.lua            # a script of your own (FAT 8.3 names)
```

Needs a framebuffer boot (the ISO), like the other Qt programs. `print` goes to the serial console.

`make qemu-qt-lua` boots headless and judges `selftest.lua` and the demo from the serial log and a few pixels (`test/qt-lua-boot.sh`); `test/qt-lua-boot.sh script file.lua` runs a script of yours on a fresh boot and shows what it printed.

## What the OS gave it

The bindings are generated from the headers of the vendored Qt with the OS's feature set, and only for what the vendored libraries define, so they follow `third_party/qt6-gui`: vendor more of Qt and the next build binds it. For the Lua runtime, `lib/` grew `ldexp`, `frexp`, `asin`, `sincos`, `strspn`, `strpbrk`, `time`, `clock` and the POSIX file calls that fail the way a read-only file system does (`mkdir`, `unlink`, `flock`, `uname`, ...), and `lib/cxx_start.c` now hands `argc` and `argv` to `main`.
