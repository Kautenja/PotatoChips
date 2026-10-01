# Module Rebranding And Shared Title System

Apply the proposed names to every active Potato Chips module and replace
the individual SVG title logos with a consistent typographic system. Keep
the modules recognizable and patches compatible while establishing a
reusable visual system with a distinct hardware-inspired identity for each
module. The prepared artwork includes light and dark variants for spec 008.

Created: 2026-10-01
Status: IN PROGRESS
Planning baseline: `8d1b6681` (16 active models).

## Prepared Artwork

The [artwork handoff](assets/012/README.md) now supplies all 16 paired
concept sheets, 32 exact-size SVG candidates, native Rack previews, fixed
palettes, pinned title typography, and a per-control movement register.
**No controls move.** Use the SVGs and numeric records for implementation;
concept images are visual exploration and can omit or distort controls.

This October 1 follow-up expands the original title-only scope to the
prepared full-panel color treatments. It removes font/palette/layout
selection from implementation. Integrate the supplied artwork rather than
starting another design exercise. Spec 008 owns native theme selection and
themed control behavior; both specs use this same prepared asset set.
Runtime integration and manual migration are verified below. Release
publication remains separate; design generation alone was not completion.

## Goal And Behavior Examples

-   A saved patch containing plugin `KautenjaDSP-PotatoChips` and model
    `MiniBoss` opens as **Operator 2612**, with identical controls, state,
    and sound. Its panel and browser entry use the new name.
-   **Facets**, **Operator 2612**, and **Pocket APU** retain the control
    arrangements of Blocks, Mini Boss, and Pallet Town Waves System. The
    generated concept sheet establishes a naming and design direction;
    its approximate dimensions and invented graphical details are not
    production geometry or required implementation assets.
-   A user following an older Blocks tutorial can find the old-to-new
    mapping and read the Facets manual at the established `Blocks.pdf` URL.
-   Manuals render module names as shared styled text instead of loading
    individual legacy logo images. Obsolete logo files are removed, and a
    clean manual build succeeds without cached copies of those assets.
-   A maintainer changes a title string or shared typographic setting and
    regenerates consistent SVG lettering without hand-drawing a new logo
    for each module. An ordinary plugin build uses committed SVGs without
    needing a font installation or the artwork-generation toolchain.

## Scope And Naming Map

The following display names are the implementation targets from the
October 1 naming proposal. All 14 sound modules and both informational
blanks are included. The slug column is immutable; capitalization of the
display names is intentional. Panels use uppercase versions of those names.

| Stable Slug | Existing Display Name | New Display Name | Full Descriptor |
| --- | --- | --- | --- |
| `Blocks` | Blocks | Facets | EDGES / QUAD DIGITAL |
| `2A03` | Infinite Stairs | Staircase 2A03 | RICOH 2A03 / CONSOLE SOUND SYSTEM |
| `VRC6` | Step Saw | Ramp VRC6 | KONAMI VRC6 / PULSE + STEPPED SAW |
| `FME7` | Pulses | Pulse FME-7 | SUNSOFT FME-7 / TRIPLE PULSE |
| `AY_3_8910` | Jairasullator | Trio AY | AY-3-8910 / PROGRAMMABLE SOUND GENERATOR |
| `POKEY` | Pot Keys | Polynomial | ATARI POKEY / TONE + NOISE |
| `SN76489` | Mega Tone | Tone 76489 | SN76489 / PROGRAMMABLE SOUND GENERATOR |
| `2612` | Boss Fight | Voice 2612 | YM2612 / FOUR-OPERATOR FM |
| `MiniBoss` | Mini Boss | Operator 2612 | YM2612 / SINGLE FM OPERATOR |
| `106` | Name Corp Octal Wave Generator | Octal 163 | NAMCO 163 / EIGHT-VOICE WAVETABLE |
| `GBS` | Pallet Town Waves System | Pocket APU | DMG / HANDHELD SOUND SYSTEM |
| `SuperADSR` | Super ADSR | Contour | S-DSP / DUAL ENVELOPE |
| `SuperEcho` | Super Echo | Echo | S-DSP / STEREO FIR ECHO |
| `SuperVCA` | Super VCA | Gaussian | S-DSP / DUAL INTERPOLATION VCA |
| `2612_Blank1` | Boss Fight Envelope Generator (Blank) | Contour 2612 | YM2612 / ENVELOPE REFERENCE |
| `Sony_S_SMP_Blank1` | S-SMP Blank | Silicon S-SMP | SONY S-SMP / CHIP PORTRAIT |

Retain the blanks' `Blank` tags and describe them explicitly as passive
reference panels in browser metadata and documentation. Contour 2612 must
not appear to provide the active envelope function of Contour. Facets is
derived from Mutable Instruments Edges algorithms, not a separate historic
chip. Describe the active Sony effects as S-DSP functions; Silicon S-SMP
continues to identify the S-SMP illustration.

Removed SuperSampler and SuperSynth are excluded. Spec 011's planned YM2151
module is not an existing model and receives no new identity here. If its
implementation lands first, reconcile the active inventory and apply the
shared title rules in coordination with 011 rather than silently omitting it.

## Source Evidence And Ownership

-   [Manifest](../plugin.json), [registration](../src/plugin.cpp), and the
    [capture inventory](../tools/capture/modules.json) establish the current
    models, stable slugs, exact widths, panel filenames, and manual paths.
    Preserve those mappings, including the two differently named blank SVGs.
-   Runtime panels currently load SVG artwork. The
    [capture guide](../tools/capture/README.md) records existing title-path
    repairs, footer geometry, and native rendering limitations.
-   Fourier at `29561591653dada5a697a53d5a6047dddf9db84c` uses
    `src/rack_extensions/panel.hpp` and `panel_artwork.hpp` to draw fixed
    vector lettering. Its title comments identify Futura outlines and
    preserved spacing. This supports a consistent typographic appearance;
    it is not evidence that current Fourier titles use live font text.
    Fourier is reference material, never a build dependency. Do not copy
    its font files or assume its artwork terms grant font redistribution.
-   [003](archive/003-source-organization-and-rack-integration.md) retains general
    integration ownership. This spec owns intentional display-name and
    title-logo and prepared palette changes, superseding earlier artwork
    preservation constraints only for that scope. Preserve the Arhythmetic
    Units footer geometry and use the handoff's theme-appropriate ink.
-   Coordinate public metadata and provenance with
    [001](archive/001-licensing-and-project-documentation.md), manual updates with
    [004](archive/004-manual-content-and-publication-style.md), captures with
    [005](archive/005-production-panel-captures-and-figures.md), and native theme
    selection with [008](008-native-light-and-dark-themes.md). Spec 008 owns
    actual light/dark support; this spec does not make it a prerequisite.

## Requirements

### Preserve Compatibility And Behavior

1.  Change `name` fields and relevant public descriptions in `plugin.json`.
    Preserve plugin identity, module slugs, registration, tags except for
    justified descriptive corrections, and existing `manualUrl` asset names.
    Do not select a new release version as part of this work.
2.  Preserve C++ model identifiers and source filenames, runtime resource
    filenames, preset directories, manual directories, and historical patch
    filenames. Names
    visible to users can change without renaming these implementation paths.
    Obsolete manual image assets are removed under the cleanup rules below;
    this path-stability requirement does not preserve unused logo files.
3.  Preserve Rack parameter, input, output, and light IDs; control geometry;
    panel dimensions; defaults and ranges; custom JSON keys and semantics;
    presets; and all audio processing. No patch migration is required.
4.  Audit visible module-name strings in tooltips, menus, descriptions,
    captures, and documentation. Update those that identify the module;
    preserve actual hardware names, citations, and attribution. Keep old
    names intentionally in compatibility mappings and historical evidence.
5.  Add a concise "formerly ..." reference to browser descriptions and an
    old-to-new README table. Verify available browser search behavior;
    do not promise aliases or add unsupported manifest fields. Keep the
    documentation mapping usable even if old names are not searchable.

### Replace SVG Title Logos With Shared Typography

1.  Use the prepared Liberation Sans Bold 1.07.4 uppercase title outlines.
    The handoff includes the pinned font, checksum, SIL Open Font License,
    source strings, and CoreText exporter. Match its shared spacing,
    alignment, and descriptors. No font selection or acquisition is needed
    during implementation; ordinary builds consume outlined SVG artwork.
2.  Keep editable title strings and shared style settings in a small
    repository-owned data source keyed by stable slug. Record the old name,
    display name, full descriptor, compact descriptor, target SVGs, and
    measured title bounds. A small local export/check script is appropriate;
    a general panel-design framework is not required.
3.  Generate outlined SVG paths for runtime titles and descriptors from
    that text source. Do not depend on live SVG `<text>`, external fonts,
    or a user's installed fonts for Rack rendering. Retain readable source
    strings and SVG metadata so maintainers can identify and regenerate
    the lettering. Commit the maintained artwork exports; keep temporary
    renders and build products ignored.
4.  Replace the old title artwork rather than masking it or drawing new
    lettering over it. Group replacements under stable, identifiable SVG
    IDs. Preserve unrelated control labels, routing arrows, artwork,
    illustration content, display bounds, and footer logos. Review XML
    diffs to prevent whole-panel rewrites or accidental embedded images.
5.  Use the measured title bounds and descriptor exceptions in the
    [artwork index](assets/012/artwork-index.json). They account for the
    existing header and screw clearances, including 6 HP Gaussian and both
    8 HP panels. Do not change control positions to enlarge headers.
6.  Preserve the prepared title sizing: 11-pixel cap-height limit on wider
    panels, 9 on narrow panels, uniform width fitting, and 5-pixel compact
    descriptor cap height where space permits. The handoff explicitly lists
    omitted descriptors. Keep full descriptions in metadata/manuals. These
    concrete, native-rendered bounds supersede the initial generic minimum
    font-size proposal; do not stretch letters or copy concept-image sizing.
7.  Use consistent title geometry across available light/dark variants,
    with contrast appropriate to each background. When 008 is pending,
    update existing production panels and make style colors reusable;
    do not claim a new dark theme. When theme pairs exist, regenerate and
    validate both in the same change.
8.  Make regeneration deterministic with pinned inputs. The check mode
    must fail on missing/duplicate active slugs, wrong title mappings,
    missing panels, stale generated title groups, out-of-bounds lettering,
    or unexpected modifications outside designated title regions. Document
    any metadata normalization needed for reproducible SVG comparison.
9.  Keep artwork tooling outside the audio path and ordinary build.
    Update [licensing](../LICENSING.md) and the
    [component inventory](../docs/licenses/THIRD-PARTY.txt) for font/tool
    provenance as applicable. Preserve existing source and artwork notices.

### Align User-Facing Materials

1.  Update README module tables, gallery alt text, examples, support and
    contributor prose where names are current product guidance. Add an
    Unreleased changelog entry explaining that names and titles changed
    while saved-patch identifiers remain stable. Do not rewrite historical
    changelog entries or previous spec evidence to erase the old names.
2.  Update all 14 manuals' title/short-name macros, PDF metadata, operating
    prose, cross-module references, and illustration captions. Preserve
    published PDF filenames and existing links. Add a brief former-name
    note where useful for readers of old tutorials.
3.  Replace every remaining use of an old module logo in manuals, covers,
    headers, and figures with the new module name rendered through shared
    LaTeX typography. Reuse the canonical naming data and shared title
    conventions; do not create a new per-module `Logo.svg`, `Logo.pdf`, or
    raster title image as its replacement. Normal manual builds must not
    require the SVG exporter. Covers currently use production panels and
    the shared Arhythmetic Units wordmark, so their panel captures must
    also be refreshed to remove the old embedded module titles.
4.  Remove obsolete `manual/*/img/Logo.*` files after migrating consumers,
    including unused SVG sources and PDF/raster copies. Remove leftover
    per-manual `KautenjaDSP.pdf` duplicates if still present. Audit other
    manual image assets and delete those proven unnecessary, including
    obsolete derivatives and their unused sources. Check references in
    LaTeX, shared styles, build/export scripts, documentation, and other
    repository consumers before removal; an unused filename in a TeX
    search alone is not proof that an editable source is unnecessary.
5.  Retain production `Panel.png` captures, the shared Arhythmetic Units
    wordmark, and explanatory figures still used by the manuals, along
    with source artwork needed to maintain them. Update build dependencies,
    asset documentation, and current provenance inventories when removing
    files. Keep historical attribution evidence and applicable notices;
    Git history preserves retired artwork without keeping unused copies
    in the active tree. Record any justified retained legacy asset here.
6.  Refresh production captures and affected panel guides using the existing
    005 tooling. Keep native images for all 16 models as review evidence;
    only the 14 sound modules currently have publication panel PNGs/manuals.
    Never substitute the AI concept sheet for a production capture.
7.  Reconcile capture inventories, manual validation, and fixture name
    expectations without weakening slug, state, geometry, or audio checks.
    Preserve serialized patch/preset data unless a visible annotation needs
    a targeted update; do not rewrite fixtures through a general re-save.

## Implementation Sequence

1.  Snapshot the active inventory, state contracts, panel dimensions,
    geometry, and current native captures. Audit all affected name strings
    and title SVG groups. Reconcile any concurrently completed specs.
2.  Verify the prepared artwork with its existing checker, reconcile any
    source geometry changes against the movement register, and preserve
    the pinned font/provenance. Promote the maintained title/export inputs
    to their final tooling location if needed, without redesigning them.
3.  Apply the naming map and integrate all 32 prepared SVGs, including both
    blanks. Coordinate native theme wiring with 008. Review fresh production
    captures at native scale before updating publication assets.
4.  Update public prose, manuals, and captures together. Migrate logo
    consumers and remove unnecessary manual assets in the same change.
    Run compatibility, native rendering, clean manual builds, and package
    checks; record exact evidence here.

## Non-Goals

No DSP changes, new controls, repositioning, resized modules, new emulators,
renamed slugs, plugin rebrand, or release-version selection. No implementation
of specs 006-011 through this naming work. Do not reproduce invented knobs,
omitted controls, stretched geometry, textures, or decorative motifs from
concept images. Integrate the exact prepared SVG palette treatments and
retained source artwork. Further creative redesign is outside implementation.
Publishing, release uploads, and VCV Library submission remain separate tasks.

## Acceptance Criteria

- [x] All 16 names and descriptors match the mapping; both blanks are
      clearly passive, and all active models are accounted for.
- [x] Browser, panel, README, and manual identities agree. Old-name guidance
      and historical PDF links remain usable without changing patch IDs.
- [x] All title logos use the shared font/text source and reproducible SVG
      outlines, with documented font provenance and no runtime font need.
- [x] All 32 prepared SVGs are integrated with the documented per-module
      palettes; control coordinates match the zero-movement register.
- [x] Full names fit without collisions or illegible scaling. Compact or
      omitted descriptors are documented; unrelated panel art is preserved.
- [x] Native live widgets and browser previews render every title correctly
      at normal and reduced zoom, including holes/counters in letters,
      digits, hyphens, and available themes. Theme toggles and graphics
      context restoration do not leave missing, stale, or duplicate titles.
- [x] Existing patch/preset contracts, geometry, and representative audio
      checks pass unchanged apart from legitimate display-name expectations.
- [x] All 14 manuals build and pass metadata/name validation; refreshed
      native panels and affected PDF pages have been visually inspected.
- [x] Manuals use the new names through shared text styling, with no
      remaining old module-logo images or replacement per-module logo
      image sets. Obsolete assets are deleted, references/dependencies are
      updated, and any retained legacy assets have a documented purpose.
- [x] A clean build of all 14 manuals succeeds after asset removal, with
      no missing-image placeholders or reliance on cached logo files.
      Covers, headers, and affected figures pass rendered-page review.
- [x] Package validation includes the revised SVGs and required notices.
      Ordinary builds work without artwork-generation dependencies installed.
- [x] Completion evidence records commands, platforms, manual observations,
      font decision, exceptions, and remaining limitations. Archive only
      after implementation acceptance, not after committing this plan.

## Validation Commands

Run from the repository root. Planning-only edits require Markdown link,
path, inventory, and diff review; they do not require a C++ or TeX build:

```shell
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

The design exporter now exists and writes only under `specs/assets/012/`.
Its check mode verifies frozen source hashes and exact generated bytes
without mutation. Runtime integration is a separate implementation step:

```shell
python3 specs/assets/012/build-artwork.py
python3 specs/assets/012/build-artwork.py --check
```

Existing implementation checks below require the prepared Rack SDK/tree
at the repository's default `../..`, matching runtime resources, compiler,
Make, Python/Pillow, and a graphical desktop for captures. Manual checks
also require the TeX tools and Poppler documented in
[CONTRIBUTING.md](../CONTRIBUTING.md) and [manual/README.md](../manual/README.md).
Packaging requires `jq`, `tar`, and `zstd` plus the SDK's platform tools.

```shell
make -j2 all test-rack
make -C tools/capture capture
make -C tools/capture test
make -C tools/capture test-native
make -C tools/capture screenshots
make -C tools/capture drawings
make -C manual clean
make -C manual
python3 scripts/validate.py manuals manual/.build
make -j2 dist
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

After visually reviewing the captures before publication export, inspect
the manuals and native Rack session separately. Audit retired asset paths
with `git ls-files manual` and repository-wide reference searches; distinguish
intentional historical mentions from live build inputs. Inspect clean-build
logs for missing images and verify rendered pages, not just PDF existence.
Load existing patches and presets, save/reload representative modules,
and verify unchanged controls
and state. Use 002's supported-platform builds and package checks; native
capture tooling currently supports macOS/Linux, not Windows. Available
theme toggles do not prove dark artwork exists while 008 remains pending.

Validate exactly the package produced for the current platform, avoiding
stale archives from previous builds. This command contains a placeholder
that must be replaced with the actual artifact path:

```shell
python3 scripts/validate.py package dist/<built-package>.vcvplugin
```

## Planning And Completion Evidence

2026-10-01: Reviewed the active manifest/registration, capture inventory and
commands, manual identity macros and validation, compatibility requirements,
and Fourier's actual vector-title implementation. The user requested the
specification and its commit/push; no product rename or artwork change is
implemented by the planning commit. Implementation acceptance remains
pending.

Planning validation on 2026-10-01:

-   `python3 -m json.tool plugin.json > /dev/null`: passed.
-   Python standard-library review of this spec and the spec index: all
    relative file links resolve; all 16 unique table slugs match both the
    active manifest and capture inventory.
-   `git diff --check`: passed. Reviewed the specification and index diff
    for scope, Markdown style, existing command names, and proposed-tool
    labeling.
-   No C++ build, native rendering, manual build, or package checks were
    run for this documentation-only change. Font selection, generated
    title SVGs, and implementation verification remain outstanding.

Record subsequent implementation evidence here without marking the feature
complete prematurely.

2026-10-01 clarification: Manual logo migration and asset removal are
required implementation work. Current shared covers already use panel
captures and the Arhythmetic Units wordmark, while legacy `Logo.svg` and
`Logo.pdf` files remain in all 14 manual image directories. Replace any
remaining consumers with styled text and remove unused copies rather than
preserving or regenerating separate module-logo images.
Planning checks: relative links and `git diff --check` passed; inspected
manual image references and confirmed the existing `make -C manual clean`
target. No assets were changed or manuals built for this documentation edit.

### Prepared Design Evidence

2026-10-01 follow-up: The user requested artwork before implementation,
including spec 008's light/dark modes and a record of control movements.
The [handoff](assets/012/README.md) fixes all 16 identities with 16 built-in
ImageGen concept pairs, 32 dimensionally exact SVGs, a pinned licensed font,
native panel previews, and exact palettes/title bounds. The SVGs resolve
concept-image omissions and spacing errors; they are the implementation
source of truth. No runtime panels, constructors, or published manual
captures were changed by this design preparation.

Validation performed:

-   `python3 specs/assets/012/build-artwork.py --check`: all 32 SVGs and
    their index match deterministic regeneration and frozen source hashes.
-   Native rendering used the existing `tools/capture/.build/capture`
    executable with separate temporary light/dark asset roots, the local
    Rack runtime, and `tools/capture/modules.json`. Both complete inventory
    runs passed live/preview construction, preference-toggle drawing, and
    graphics-context restoration. This validates each supplied palette,
    not production theme-switching integration. The initial sandboxed run
    timed out; native desktop execution outside the sandbox succeeded.
-   All 606 captured controls matched their baseline coordinates and sizes
    in both palette runs. Another 69 screw/display/editor widgets were
    recorded from source. All target positions equal the originals.
-   All 32 panels were visually inspected in native review sheets. Dark
    footer/output-label contrast defects were corrected and rerendered.
    Generated concept errors are explicitly listed in the handoff.
-   Font outlines were produced with Apple Swift 6.3.3/CoreText using the
    bundled Liberation Sans Bold 1.07.4 file. The license, checksum, path
    data, source text, and reproduction commands accompany the artwork.

See [validation.json](assets/012/validation.json) for per-panel results,
final SVG hashes, and platform details. Spec 006's missing Echo sliders and
007's envelope-label decisions remain with those specs. Dim-room/themed
component review, live global theme integration, minimum-Rack checks,
manual builds, audio regression tests, and packaging remain implementation
acceptance work. No release or runtime behavior is claimed by these assets.


## Implementation Evidence — October 1, 2026

Implemented all 16 identities and 32 SVG variants. The shared
`createThemedPanel` helper uses Rack's two-path factory; every port and screw
uses the native themed component. All production `.cpp` edits are confined
to those substitutions. No DSP, IDs, parameters, JSON, preset data, panel
sizes or control positions changed. Rack 2.4.0 is the minimum; version 2.1.0
and all established manual URLs remain unchanged.

The [integrated native gallery](assets/012/IMPLEMENTED.md) shows actual
production controls. [Machine-readable evidence](assets/012/implementation.json)
records input/library hashes, all model results and the verified minimum
SDK/runtime download hashes. Design previews remain separately identified
as pre-integration evidence. SVG whitespace is normalized; original frozen
source hashes are retained. Ordinary builds require no artwork generator.

### Validation Actually Run

-   `python3 specs/assets/012/build-artwork.py --check --installed`: 32
    byte-exact runtime panels, 16 active/name mappings, 14 manual names,
    pinned font hash, outline text, title bounds and source hashes pass.
-   `make -j2 all test-rack`: plugin and all four suites pass on macOS
    arm64 with the local Rack 2.6.0 headers/library/resources. Assertion
    counts: 608, 4,747, 67 and 464. Saved patches/presets and the existing
    representative audio fixture remain unchanged.
-   `make -j2 all test-rack RACK_DIR=/tmp/potato-rack-2.4/Rack-SDK`:
    the same build and 5,886 assertions pass with official Rack 2.4.0.
    Added an explicit `<plugin.hpp>` include to the contract test because
    2.4's umbrella does not transitively expose the plugin registry.
-   `make -C tools/capture capture`: all 16 live widgets and null previews
    pass light/dark/light selection, initial dark construction, context
    recreation, state/history checks and immutable geometry checks.
    All 606 control/display bounds equal the frozen movement register.
    Additional captures cover 75%/150% zoom and 50% room dimming outside
    the mouse spotlight; 288 images per host include the 32 restored views.
-   The post-capture audio probe compares 512 frames across 16 channels on
    2A03, 106, GBS and SuperEcho against untouched twins, with eight theme
    changes and two simultaneous widgets: exact sample equality. Both
    instances receive identical host add/sample-rate events. This corrected
    an initial harness mismatch; no production audio change was required.
-   `make -C tools/capture capture BUILD=.build/minimum
    RACK_DIR=/tmp/potato-rack-2.4/Rack-SDK`: all native checks also pass
    against 2.4.0 and its matching extracted runtime graphics. SDK/resource
    downloads and extraction remain outside the repository.
-   `make -C tools/capture test` and `make -C tools/capture test-native`:
    export rollback, stale/missing/blank/incorrect geometry, identical theme
    rejection, exact dark crops, single-module isolation, native missing
    light/dark panels and renderer failure checks pass.
-   `python3 tools/capture/export_screenshots.py
    tools/capture/.build/captures manual` and
    `python3 tools/capture/draw_panels.py tools/capture/.build/captures`:
    refreshed all 14 dark covers and all named wireframe diagrams. Export
    defaults to Dark; `--theme Light` remains an explicit option.
-   `make -C manual clean` then `make -C manual -j2`: all 14 PDFs build
    from sources without cached obsolete assets. `python3
    manual/latex/test-build.py` passes all failure/recovery checks.
-   `python3 scripts/validate.py manuals manual/.build`: names, versions,
    metadata, complete inventory and meaningful text pass. Used bundled
    Poppler's `pdftotext` on PATH. Rendered and inspected all 115 pages,
    including covers, headers, panel maps and retained YM2612 figures.
-   `make -j2 dist` and `python3 scripts/validate.py package
    dist/KautenjaDSP-PotatoChips-2.1.0-mac-arm64.vcvplugin`: package bytes,
    32 panel files, manifest, presets and notices pass.
-   `git diff --check`, local Markdown links, unchanged manifest identity,
    slugs/tags/manual URLs/version and production-diff audit pass.

### Visual And Publication Decisions

All titles and descriptors use the pinned Liberation Sans Bold outlines;
manual titles use existing shared LaTeX typography. New names appear in
README guidance, browser metadata, all manuals, and wireframes. Former names
remain in metadata and explicit lookup notes. Rack 2.6's local browser
source indexes model descriptions; no unsupported alias field was added.

The user selected dark-theme manual captures. New wireframe titles are
text; control symbols, groups and coordinates are unchanged. Audited all
repository consumers before removing 56 unused files: 28 module-logo
SVG/PDFs, 16 AY envelope-mode SVG/PDFs, six Sony chip images, two obsolete
ADSR envelope exports and four VU/on-off exports. Retained both used YM2612
envelope PNGs, Voice 2612's Operators PDF and editable SVG, all Panel PNGs,
and the shared Arhythmetic Units wordmark. The earlier KautenjaDSP PDF
cleanup was already present. Font provenance and its OFL notice are included
in the packaged license inventory; older illustration provenance is retained.

Native review includes every module in both themes, narrow titles, passive
blanks, slider handles, lit/unlit defaults, waveform traces and the algorithm
display. No clipped titles, moved controls or context-cache artifacts were
observed. The reduced-zoom harness clip rectangle was corrected to use
module coordinates before final review. No additional creative decisions or
control-layout changes were needed.

Validation is macOS arm64/Retina 2x, Rack 2.4.0 and 2.6.0. Linux/Windows
native capture and an audio-device listening session were not run; existing
cross-platform CI remains the other platform gate. Dim captures reproduce
Rack's outside-spotlight dimming and emissive layer order. Adjacent Echo FIR
slider and Contour RR-label issues remain owned by 006/007 and are accurately
shown/described in these manuals. No release upload or VCV Library publication
is implied. Built PDFs/packages and temporary review pages stay ignored.
