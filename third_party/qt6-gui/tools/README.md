# tools — how `third_party/qt6-gui` was made, and how to refresh it

You do **not** need any of this to build `apps/hello-qt-gui` or `apps/hello-qt-widgets`;
the vendored tree is complete. These exist to re-derive it (e.g. to move to a newer Qt 6.8.x).

Pipeline, all against a throw-away *scratch* directory (use `$TMPDIR`; it takes
~400 MB and the build ~40 min):

1. `setup.sh <scratch>` — clones `qtbase` branch `6.8`, lays out the flattened
   `QtCore/`, `QtGui/`, `QtWidgets/`, `qpa/` include trees, copies and edits the config
   headers from an installed Qt 6.10 SDK (`QT_SDK_INCLUDE`, default
   `~/.cache/qgcoder-wasm/Qt/6.10.2/gcc_64/include`), generates the CamelCase
   forwarding headers (`gen_camel.py`, then `redirect_forwards.sh` so a
   `QtWidgets` forward never shadows a real `QtGui` one), applies the source
   patches (`patch_osdetect.py`, `patch_tls.py`, `patch_gui.py`,
   `patch_minimal.py`, `patch_widgets.py`) and runs `genmoc.sh`. The QtWidgets
   feature set is trimmed there by hand-editing `qtwidgets-config*.h` (see the
   vendored README for why `menu` and `shortcut` must stay on).
2. `compile_all.sh <scratch>` — compiles everything with the real flags in
   `compile_flags.sh`. `exclude_files.txt` (core+gui) and `exclude_widgets.txt` list Qt files that
   must not be compiled alone or belong to disabled platforms/features. It
   runs 11 jobs in parallel (`xargs -P 11`); a full run took roughly 5-10
   minutes on a 12-core machine.
3. `vendor.py <scratch> --copy` — runs `g++ -MM` over every translation unit and
   copies exactly the files it reads into `qtbase/` and `koppios/`; regenerates
   `koppios/sources.mk` and `incs.mk`. Re-running it on an unchanged scratch is
   byte-for-byte idempotent.

`genmoc.sh` needs a real **moc 6.8.4** (`MOC=...`, default
`~/.cache/qt6-gui/moc6.8.4`), built from `qtbase/src/tools/moc` with
`QT_BOOTSTRAPPED`; the system moc (6.10) emits a different meta-object
revision and must not be used. Its output is already checked in
(`koppios/mocgen/`).

Other tools:

* `subset_ttf.py` — dependency-free TrueType subsetter (used for the Unifont subset).
* `boot_test.sh` — boots the built app on a private GRUB ISO under QEMU; see its header.
* `link_and_boot.sh` + `probes/` — scratch-tree link + boot for diagnostic
  programs. `probes/t5.cpp` shows the technique that found the heap bug: draw
  one primitive at a time and touch every page of the image after each.

## Gotchas that cost real time

* `setup.sh`'s `_p.h` redirect loop must skip `.h`-suffixed names and symlinks
  or it writes *through* the symlinks into the cloned Qt sources.
* `pkill -f`/`pgrep -f` on a script name match the calling shell itself.
* Never edit a running bash script (bash reads it by offset).
* `ld` on a flat list of bloated objects needs GBs; archives + no
  `-fkeep-inline-functions` need ~560 MB.
