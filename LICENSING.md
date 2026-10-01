# Potato Chips Licensing

Potato Chips uses separate terms for source code, visual assets, and
imported components. This guide records the existing scope; it does not
relicense the repository. [LICENSE](LICENSE) contains the unmodified GNU
GPL version 3 text previously stored as `LICENSE-GPLv3.txt`.

## Source Code

Project source in `src/` is distributed under the GNU General Public
License, version 3 or, at your option, any later version
(`GPL-3.0-or-later`), as declared in [plugin.json](plugin.json). Existing
Christian Kauten and contributor notices retain their authors and years.
Imported material also carries the provenance and terms recorded in
[THIRD-PARTY.txt](docs/licenses/THIRD-PARTY.txt).

The YM2612 import has an unresolved historical license discrepancy: its
original MAME notice restricted commercial redistribution, while current
local headers state GPL-3.0-or-later. The original notice is reproduced in
[MAME-HISTORICAL.txt](docs/licenses/MAME-HISTORICAL.txt). Modern upstream
licensing alone does not establish permission for this exact older import.
This inventory preserves that evidence and does not certify that the
blanket source declaration resolves the discrepancy.

## Visual Assets

The existing project terms identify the module visual designs, KautenjaDSP
logo, and icon as copyright 2020-2024 Christian Kauten, under Creative
Commons Attribution-NonCommercial-NoDerivatives 4.0 International
(`CC-BY-NC-ND-4.0`). That scope covers project graphics in `res/` and
`manual/`; see the [complete text](docs/licenses/CC-BY-NC-ND-4.0.txt).
The source-code license is not an alternative artwork license.

The component inventory separately identifies imported algorithm diagrams
and manual raster illustrations whose original author/license is not
fully established. The project artwork declaration does not establish
third-party ownership. The runtime footer replacement uses Arhythmetic Units vector artwork,
copyright 2025-2026 Arhythmetic Units, under the same CC-BY-NC-ND-4.0 terms.
The shared manual wordmark uses the same terms; its RackNES PDF source and
revision are recorded in the inventory. The runtime footer source is also in
`res/ArhythmeticUnits.svg`. Other panel artwork and attribution are preserved;
production captures and vector panel guides replace the legacy module
illustrations. Screenshots also depict Rack-provided controls, whose terms
remain separate from project artwork. See the component inventory.

The replacement Potato Chips social banner in
`manual/PotatoChips-SocialMedia.svg`, `.png`, and `.pdf` follows the same
CC-BY-NC-ND-4.0 artwork terms. Its original composition and embedded
Arhythmetic Units logo provenance are recorded in the component inventory.

## Dependencies And Tests

Blargg-derived audio code retains its authorship and historical LGPL
provenance alongside the local adaptation notices. Mutable Instruments
Edges code retains Emilie Gillet's credit and GPL terms. The
[component inventory](docs/licenses/THIRD-PARTY.txt) lists paths, import
evidence, known versions, and remaining uncertainties.

Tests in `test/` carry their existing MIT notices, reproduced in
[MIT-TESTS.txt](docs/licenses/MIT-TESTS.txt). Catch2 is a test dependency
under the [Boost Software License 1.0](docs/licenses/BSL-1.0.txt); it is not
linked into the plugin. Rack and its runtime resources are supplied by
the host and are not bundled as Potato Chips assets.

Plugin packages include `LICENSE`, this guide, and `docs/licenses/` via
the [Makefile](Makefile). Individual source notices remain authoritative
evidence; uncertain provenance is recorded rather than replaced with a
new grant of rights.
