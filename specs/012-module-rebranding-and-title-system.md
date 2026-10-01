# Module Rebranding And Shared Title System

Apply the proposed names to every active Potato Chips module and replace
the individual SVG title logos with a consistent typographic system. Keep
the modules recognizable and patches compatible while establishing a
reusable foundation for a later, broader graphical redesign.

Created: 2026-10-01
Status: PLANNED
Planning baseline: `8d1b6681` (16 active models).

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
-   [003](003-source-organization-and-rack-integration.md) retains general
    integration ownership. This spec owns intentional display-name and
    title-logo changes, superseding earlier title-preservation constraints
    only for that scope. Preserve the Arhythmetic Units footer treatment.
-   Coordinate public metadata and provenance with
    [001](001-licensing-and-project-documentation.md), manual updates with
    [004](004-manual-content-and-publication-style.md), captures with
    [005](005-production-panel-captures-and-figures.md), and native theme
    selection with [008](008-native-light-and-dark-themes.md). Spec 008 owns
    actual light/dark support; this spec does not make it a prerequisite.

## Requirements

### Preserve Compatibility And Behavior

1.  Change `name` fields and relevant public descriptions in `plugin.json`.
    Preserve plugin identity, module slugs, registration, tags except for
    justified descriptive corrections, and existing `manualUrl` asset names.
    Do not select a new release version as part of this work.
2.  Preserve C++ model identifiers, source/resource filenames, preset
    directories, manual directories, and historical patch filenames. Names
    visible to users can change without renaming these implementation paths.
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

1.  Use one geometric sans-serif family with a consistent title weight,
    uppercase treatment, spacing, alignment, and descriptor style. Select
    and record the exact font file/version, source, checksum, license, and
    outline-generation tool/version during implementation. Prefer a font
    that permits redistribution and derivative outlines. Similarity to
    Fourier is a visual direction, not a requirement to acquire Futura.
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
5.  Establish measured title-safe rectangles for every panel, accounting
    for screws, first-row controls, and existing illustrations. Base these
    on production SVGs/widgets, not the concept sheet. Pay particular
    attention to 6 HP Gaussian, the two 8 HP panels, and longer names.
    Use a small set of documented width classes and explicit exceptions;
    avoid arbitrary horizontal distortion or per-module custom lettering.
6.  Fit the complete new title legibly at normal Rack zoom. Target a minimum
    10 Rack-pixel title size and 7 Rack-pixel descriptor size. If a full
    descriptor cannot fit, use its chip/source identifier on the panel
    (for example `S-DSP`, `FME-7`, or `EDGES`) and retain the full descriptor
    in metadata/manuals. If there is no safe second line, omit the panel
    descriptor and record that exception. Do not move controls or shrink
    text below the reviewed minimum to force the full sentence onto a panel.
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
3.  Audit `manual/*/img/Logo.*` and similar legacy logo assets for actual
    consumers. Update still-used module logos through the same text source;
    identify unreferenced historical assets without bulk-renaming or
    deleting them. Covers currently use production panels and the shared
    Arhythmetic Units wordmark, so replacing a legacy logo alone is not
    sufficient to update a cover.
4.  Refresh production captures and affected panel guides using the existing
    005 tooling. Keep native images for all 16 models as review evidence;
    only the 14 sound modules currently have publication panel PNGs/manuals.
    Never substitute the AI concept sheet for a production capture.
5.  Reconcile capture inventories, manual validation, and fixture name
    expectations without weakening slug, state, geometry, or audio checks.
    Preserve serialized patch/preset data unless a visible annotation needs
    a targeted update; do not rewrite fixtures through a general re-save.

## Implementation Sequence

1.  Snapshot the active inventory, state contracts, panel dimensions,
    geometry, and current native captures. Audit all affected name strings
    and title SVG groups. Reconcile any concurrently completed specs.
2.  Select the font and document its provenance. Implement the small title
    source and exporter/checker, then prove the width rules on Facets,
    Operator 2612, Pocket APU, Gaussian, and both 8 HP modules.
3.  Apply the naming map and regenerate titles for the full inventory,
    including both blanks and all existing theme variants. Review contact
    sheets at native scale before updating publication assets.
4.  Update public prose, manuals, and captures together. Run compatibility,
    native rendering, manual, and package checks; record exact evidence here.

## Non-Goals

No DSP changes, new controls, repositioning, resized modules, new emulators,
renamed slugs, plugin rebrand, or release-version selection. No implementation
of specs 006-011 through this naming work. Do not reproduce the concept
sheet's knobs, screen colors, panel textures, palettes, or decorative motifs
in this phase. A later graphical rebrand can reuse the shared typography,
color roles, and title bounds to unify the entire panel family. Publishing,
release uploads, and VCV Library submission remain separate tasks.

## Acceptance Criteria

- [ ] All 16 names and descriptors match the mapping; both blanks are
      clearly passive, and all active models are accounted for.
- [ ] Browser, panel, README, and manual identities agree. Old-name guidance
      and historical PDF links remain usable without changing patch IDs.
- [ ] All title logos use the shared font/text source and reproducible SVG
      outlines, with documented font provenance and no runtime font need.
- [ ] Full names fit without collisions or illegible scaling. Compact or
      omitted descriptors are documented; unrelated panel art is preserved.
- [ ] Native live widgets and browser previews render every title correctly
      at normal and reduced zoom, including holes/counters in letters,
      digits, hyphens, and available themes. Theme toggles and graphics
      context restoration do not leave missing, stale, or duplicate titles.
- [ ] Existing patch/preset contracts, geometry, and representative audio
      checks pass unchanged apart from legitimate display-name expectations.
- [ ] All 14 manuals build and pass metadata/name validation; refreshed
      native panels and affected PDF pages have been visually inspected.
- [ ] Package validation includes the revised SVGs and required notices.
      Ordinary builds work without artwork-generation dependencies installed.
- [ ] Completion evidence records commands, platforms, manual observations,
      font decision, exceptions, and remaining limitations. Archive only
      after implementation acceptance, not after committing this plan.

## Validation Commands

Run from the repository root. Planning-only edits require Markdown link,
path, inventory, and diff review; they do not require a C++ or TeX build:

```shell
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

The implementation should provide this proposed interface, or record its
exact replacement here. It does not exist at planning time. `--write`
regenerates only owned title groups; `--check` validates without mutation:

```shell
python3 tools/branding/titles.py --write
python3 tools/branding/titles.py --check
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
make -C manual
python3 scripts/validate.py manuals manual/.build
make -j2 dist
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

After visually reviewing the captures before publication export, inspect
the manuals and native Rack session separately. Load existing patches and
presets, save/reload representative modules, and verify unchanged controls
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
