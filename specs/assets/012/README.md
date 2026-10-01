# Panel Artwork Handoff

This package fixes the visual decisions for spec 012 and supplies the paired
SVG artwork for spec 008. It contains 16 generated concept sheets, 32 exact
size vector panel candidates, and the existing control coordinates. The paired SVGs are integrated into runtime `res/`; native theme wiring and
dark manual captures now use them.

Created: 2026-10-01. Design prepared and integrated under specs 008/012.

Open the [integrated native gallery](IMPLEMENTED.md) for the implemented
panels, or the [design preview gallery](GALLERY.md) for the earlier review.

## Implementation Authority

Use [artwork-index.json](artwork-index.json) and the SVGs under `panels/` as
the concrete artwork to integrate. [directions.json](directions.json) records
the palettes and the full prompts used with the built-in image generation
tool. The PNGs under `concepts/` explore those identities; they are not
dimensional drawings or substitutes for native widgets. They sometimes
omit repeated controls, stretch panels, change LED states, or embellish
labels. Do not reproduce those errors in code.

The SVG candidates resolve that ambiguity by retaining original panel
geometry and functional label paths. [layout.json](layout.json) specifies
every captured control's original and target top-left coordinates, size,
ID, and label in Rack pixels. **Every target equals its original position;
every delta is zero.** This includes knobs, jacks, switches, sliders, and
lights. Displays and screws remain at their existing constructor positions.
The two blanks have no controls. No hardware or hit area is resized.

The movement register also includes source-checked screw rectangles, both
sets of five wavetable-editor rectangles, and Voice 2612's algorithm display.

The implementation is therefore mechanical: integrate the selected paired
SVGs at their existing light paths and `-dark.svg` companion paths, apply
008's native theme wiring, and carry the new names into metadata/manuals.
Do not infer positions, colors, typography, or missing controls from pixels
in the concept sheets. Source code and the captured geometry take precedence
over a concept image, and these exact SVGs settle the artwork decisions.

## Module Designs And Movement Register

All dimensions below are Rack pixels; height is always 380. Each concept
link shows both themes. SVG filenames are listed in the artwork index.

| Module | Width | Identity | Control Movement | Specific Implementation Notes |
| --- | --- | --- | --- | --- |
| [Facets](concepts/Blocks.png) | 150 | Chalk/cyan and blue charcoal | None | Four unchanged voice strips. Keep the small existing footer emblem; the concept's extra footer text and oversized header are not adopted. |
| [Operator 2612](concepts/MiniBoss.png) | 240 | Cream/vermilion and FM charcoal | None | Retain six sliders, rate scale, eight knobs, and both jack rows. Exact SVG retains the existing output field. |
| [Pocket APU](concepts/PalletTownWavesSystem.png) | 300 | Atomic lilac and smoked atomic purple | None | Preserve original circuit-board paths, five black editors with colored traces, and four voice columns. No LCD redesign or transparency shader. |
| [Staircase 2A03](concepts/InfiniteStairs.png) | 150 | Console gray/red and warm charcoal | None | Triangle column stays sparse. Use the SVG's subdued strips; the concept's bright red dark-mode fields are not adopted. |
| [Ramp VRC6](concepts/StepSaw.png) | 120 | Pale violet and plum | None | Preserve two pulse columns, saw column, and its SYNC position. No new staircase icon is needed. |
| [Pulse FME-7](concepts/Pulses.png) | 120 | Cartridge ivory/copper and dark umber | None | Three pulse lanes. Dark SVG uses muted brown surfaces, not the concept's broad bright orange fields. |
| [Trio AY](concepts/Jairasullator.png) | 255 | Cobalt gray and midnight blue | None | Retain all three complete tone/noise/envelope groups and the shared right column. The concept omits part of the third group; do not copy that omission. |
| [Polynomial](concepts/PotKeys.png) | 210 | Parchment/olive and dark olive | None | All four noise knobs remain, along with the six utility switch/CV rows. The concept omits noise knobs and alters aspect ratio. |
| [Tone 76489](concepts/MegaTone.png) | 150 | Blush/plum and dark aubergine | None | Preserve all three tone lanes and the distinct noise lane. |
| [Voice 2612](concepts/BossFight.png) | 975 | Cream/vermilion and FM charcoal | None | Keep the full global block, all four operator banks, and both CV rows. Concept omissions are rejected. Large operator numerals retain their original geometry. |
| [Octal 163](concepts/NameCorpOctalWaveGenerator.png) | 510 | Celadon/jade and forest green | None | Exactly eight voice columns, five editors, and both global control groups remain. Do not reproduce the concept's repeated-column count errors. |
| [Contour](concepts/SuperADSR.png) | 180 | Lavender gray/rose and muted plum | None | Both existing envelope groups remain. Spec 007 still owns verified envelope-label changes; do not treat the old RR label as a new behavior decision. |
| [Echo](concepts/SuperEcho.png) | 240 | Sea-glass/teal and deep teal | None | Retain all eight tap rows and stereo markings. Spec 006 owns restoring the missing FIR sliders at their reserved source positions. |
| [Gaussian](concepts/SuperVCA.png) | 90 | Champagne/gold and plum charcoal | None | Preserve both channels and mode controls. SVG retains red right-channel markings; the concept's gold right-channel recoloring is not adopted. |
| [Contour 2612](concepts/2612_Blank1.png) | 480 | Cream/vermilion and FM charcoal | None; passive | Existing envelope diagram, stage labels, and key-on/off markers remain. Only palette and header change. |
| [Silicon S-SMP](concepts/Sony_S_SMP_Blank1.png) | 420 | PCB green and dark forest | None; passive | Original chip/pin/connector artwork stays fixed. Add the title in the free top margin; omit a subtitle to avoid the board illustration. |

For Echo, the reserved slider origins in current source are `(147, 29 +
43 * i)` for taps `i = 0..7`. These are not newly moved controls and are
not present in the current native captures. 006 owns that separate restoration. Current production captures intentionally
show the missing sliders; do not relocate rows to match a concept.

## Fixed Visual Rules

-   `directions.json` stores eight hex colors per module: light background,
    light surface, light ink, light accent, followed by the four dark roles.
    `build-artwork.py` contains the exact role mapping and blending rules.
    The emitted SVG colors are authoritative; no color sampling is needed.
-   Dark panels use dark surfaces rather than inverted light art. Preserve
    black display backgrounds and current waveform/algorithm colors.
    Stereo-right labels retain red as a functional distinction. LED values
    and states are unchanged; illustrated lights are not a state contract.
-   Titles use bundled **Liberation Sans Bold 1.07.4**, uppercase, with
    native font spacing. All title and subtitle glyphs are paths. The same
    geometry is used in both themes. Existing functional-label paths and
    Arhythmetic Units artwork are retained.
-   Title ink fits in the original header band, starting at y=4. Cap-height
    limits are 11 pixels for panels at least 150 pixels wide and 9 for
    narrower panels, reduced uniformly only when the fixed width demands
    it. Actual bounds are recorded per SVG in `artwork-index.json`. This
    measured rule replaces the earlier generic minimum-size proposal.
-   Subtitles have a 5-pixel cap height at y=18. Omit them on Voice 2612,
    Octal 163, Gaussian, Contour 2612, and Silicon S-SMP because of their
    header constraints. Hardware identity remains in the name or metadata.
-   Keep existing decorative paths, panel lanes, routing arrows, and
    footer geometry. Do not add concept-image motifs that lack a vector
    counterpart. No gradients, textures, new handheld buttons, or lighting
    shaders are needed for implementation.
-   Keep the original controls for this artwork review. Spec 008's native
    themed screw/port substitutions retain sizes and hit areas; any native
    display or slider contrast correction must preserve these art decisions.

## Files And Reproduction

-   `sources/`: frozen original panel SVG inputs, preserving attribution.
-   `panels/`: 32 authoritative SVG exports mirrored under `res/`.
-   `concepts/`: 16 built-in ImageGen outputs, one paired sheet per module.
-   `previews/`: native Rack previews of the exact SVG candidates.
-   `layout.json`: baseline source hashes and unchanged control coordinates.
-   `title-outlines.json`: generated font paths and font version.
-   `artwork-index.json`: output filenames, dimensions, and title bounds.
-   `fonts/`: pinned font and its SIL Open Font License 1.1 notice.
-   `outline-titles.swift`: optional macOS/CoreText title-path exporter.
-   `build-artwork.py`: standard-library-only SVG assembly from frozen inputs.

Run from the repository root with Python 3.9+. This regenerates only spec
assets and never overwrites runtime panels:

```shell
python3 specs/assets/012/build-artwork.py
python3 specs/assets/012/build-artwork.py --check
```

To reproduce title paths, use macOS with Swift/CoreText and the bundled font:

```shell
swift specs/assets/012/outline-titles.swift specs/assets/012/fonts/LiberationSans-Bold.ttf specs/assets/012/directions.json specs/assets/012/title-outlines.json
python3 specs/assets/012/build-artwork.py
```

The font SHA-256 is
`361c61b82d575c5c35fd9157fda8b0194bcfcd0d88ea8521a4fb5dd53d33dddc`.
It was sourced from the bundled PDF.js standard-font distribution; its
included license records Google/Red Hat attribution. Runtime builds need
neither the font nor CoreText because all lettering is outlined. No font
file is added to runtime `res/` by this design handoff.

Existing panel art retains the terms in [LICENSING.md](../../../LICENSING.md).
The new panel compositions and concept images are first-party design work
for this request and follow the repository's visual-asset terms. Preserve
the source/artwork distinction when promoting assets to production, and
record their provenance in the component inventory during implementation.

## Validation Boundary

The design package is not an implementation of native theme switching.
Native previews render each palette from an isolated temporary asset tree;
they do not modify the plugin installation or demonstrate 008's live API
wiring. Spec 008 still requires live switching, preview lifecycle, dim-room
review, minimum-Rack testing, packaging, and issue follow-through. Spec 012
still requires metadata/manual migration and obsolete-asset cleanup.

## Integrated Asset Checks

```shell
python3 specs/assets/012/build-artwork.py --check --installed
```

This additionally checks the enabled manifest inventory and names, title
source strings and bounds, and byte equality of all runtime panel exports.
Whitespace at line ends is normalized in frozen SVG inputs and exports;
`layout.json` retains original source hashes separately where normalization
changed bytes. Paths, colors and geometry are unchanged by normalization.
The native capture pipeline owns current implementation evidence. The
original `previews/` and `validation.json` remain the pre-integration design
review, with original screws/ports; production now uses themed components.
