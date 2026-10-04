# unifont-subset.ttf

A **subset** of GNU Unifont 16.0.03 (the same font the kernel console uses via
`unifont.sfn`): Basic Latin, Latin-1 Supplement, Latin Extended-A, General
Punctuation, the euro sign, arrows, box drawing and a few geometric shapes —
550 glyphs, produced by
[`third_party/qt6-gui/tools/subset_ttf.py`](../../third_party/qt6-gui/tools/subset_ttf.py)
from the full 11 MB font:

    subset_ttf.py unifont-16.0.03.ttf unifont-subset.ttf \
        U+0020-007E U+00A0-017F U+2010-203A U+20AC U+2190-2199 U+2500-257F U+25A0-25CF

Copyright © 1998–2025 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller,
Johnnie Weaver, David Corbett, Nils Moskopp, Rebecca Bettencourt, Ho-Seok Ee,
et al. (https://unifoundry.com/unifont/).

Dual licensed: **SIL Open Font License 1.1**, and **GNU GPL version 2 or later
with the GNU Font Embedding Exception** (the notice the font itself carries in
its `name` table, IDs 0/13/14). GPL-2.0-or-later text:
[`third_party/qt6-gui/LICENSES/GPL-2.0-or-later.txt`](../../third_party/qt6-gui/LICENSES/GPL-2.0-or-later.txt).
