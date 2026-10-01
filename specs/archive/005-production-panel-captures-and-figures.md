# Production Panel Captures And Reference Figures

Created: 2026-10-01
Status: COMPLETE

Generate the manuals' module screenshots from production Rack widgets and
maintain clear vector panel references beside the manual source.

## Evidence And Current Gaps

-   Manuals and README use `manual/<Module>/img/Module.svg`/`.pdf` and
    `Interface.svg`/`.pdf`, separate from the runtime SVG and C++ widget
    layout. Controls and indicators can drift from those static drawings.
-   There is no production capture tool. Boss Fight's algorithm display,
    wavetable editors, sliders, screws, lights, and panel loading must render
    through their actual code to provide a trustworthy screenshot.
-   The project has 14 manuals and panels of different widths. A single
    hard-coded two-module crop from either sibling cannot be copied intact.

References: RackNES's [schematic migration](https://github.com/Kautenja/RackNES/commit/5310d09),
[production captures](https://github.com/Kautenja/RackNES/commit/29156e5),
[asset cleanup](https://github.com/Kautenja/RackNES/commit/67937cc), and
[figure refinement](https://github.com/Kautenja/RackNES/commit/96af0ec);
`tools/capture/capture.cpp`, `export_screenshots.py`, and its README.
Fourier's [native inspector/exporter](https://github.com/Kautenja/ArhythmeticUnits-Fourier/commit/e5f1a43)
and `docs/latex/figures/panel-drawing.tex` provide the same separation of a
real cover screenshot and a vector control reference.

## Scope And Ownership

Own new `tools/capture/`, reviewed `manual/*/img/Panel.png`, per-module
`figures/panel-layout.tex`, and `manual/latex/panel-drawing.tex`. Coordinate
figure inputs/build hooks with [004](004-manual-content-and-publication-style.md)
and README image substitutions with [001](001-licensing-and-project-documentation.md).
Use final runtime branding from [003](003-source-organization-and-rack-integration.md)
and panel pairs/theme behavior from [008](008-native-light-and-dark-themes.md).

## Required Inventory

Use a reviewed mapping of manifest slug to manual directory and stable
release filename, rather than assuming they are identical:

| Manifest Slug | Manual Directory / PDF Basename |
| --- | --- |
| `Blocks` | `Blocks` |
| `MiniBoss` | `MiniBoss` |
| `106` | `NameCorpOctalWaveGenerator` |
| `2612` | `BossFight` |
| `2A03` | `InfiniteStairs` |
| `AY_3_8910` | `Jairasullator` |
| `FME7` | `Pulses` |
| `GBS` | `PalletTownWavesSystem` |
| `POKEY` | `PotKeys` |
| `SuperADSR` | `SuperADSR` |
| `SuperEcho` | `SuperEcho` |
| `SuperVCA` | `SuperVCA` |
| `SN76489` | `MegaTone` |
| `VRC6` | `StepSaw` |

The two enabled blanks (`2612_Blank1`, `Sony_S_SMP_Blank1`) need graphical
smoke coverage but no invented manual.

## Requirements

1.  Build a native Rack harness using production registered modules, widget
    constructors, runtime SVGs, controls, and display paths. Use an isolated
    asset/user directory, a matching SDK/library/resources set, and no audio
    device or user's saved patch. Do not maintain a parallel screenshot UI.
2.  Define deterministic per-module visual fixtures: sample rate, initial
    parameters/wavetables, optional test voltages/gates, and bounded processing
    duration. Prefer defaults; use minimal synthetic signals when required
    to show an envelope, meter, waveform, or effect state. Record deliberate
    nondefaults. No game ROM or sample fixture is needed for these active
    modules, and visual fixtures are not audio validation.
3.  Render light/dark live widgets and null-module browser previews. Include
    theme toggles owned by 008 and display/editor lifecycle checks owned by
    003. Wait for component framebuffers with a bounded retry; fail on
    initialization, missing asset, invalid geometry, incomplete rendering,
    or GL errors. Record unsupported platforms rather than writing a mockup.
4.  Provide `capture` to write ignored intermediate images and `screenshots`
    to validate/crop the complete set before updating tracked PNGs. Support
    a single-module selection for routine changes. Derive expected geometry
    from reviewed module dimensions/mapping, preserve native pixel density,
    and reject mismatches. Crop losslessly; do not rescale, paint over, or
    synthesize visible controls. A failed batch must preserve existing PNGs.
5.  Store one reviewed light-panel PNG per active manual, used by both cover
    and README. Keep dark/preview diagnostics and intermediate captures in
    ignored `.build/`. Document capture prerequisites, settings, exact
    commands, source revision, geometry, and visual review in the tool guide.
    Reproducibility means the same fixture and rendering path, not guaranteed
    byte-identical pixels across OS/font/graphics versions.
6.  Create shared TikZ primitives and per-module panel reference drawings
    with correct aspect ratio and recognizable knobs, sliders, ports,
    displays, groups, and numbered callouts at their actual positions.
    Match callout numbers to 004's control sections. Omit live meter values,
    algorithm animation, and wavetable sample content from the reference
    diagrams. Keep conceptual chip/audio illustrations separate and retain
    useful existing technical figures and attribution.
7.  Remove obsolete `Module`/`Interface` exports only after all consumers
    have moved and replacements are reviewed. Ordinary manual and CI builds
    consume committed PNGs and TeX figure sources with no native capture
    dependency. Regenerate affected screenshots and drawings when runtime
    layout changes, then rebuild the affected manuals sequentially.

## Behavior Examples

-   A Boss Fight capture shows its real algorithm display and controls;
    its manual reference diagram shows stable labels/callouts without
    pretending to be a live screenshot.
-   Refreshing only Pot Keys changes its reviewed PNG and leaves the other
    13 untouched. A crop-size mismatch fails before replacing any output.
-   A clean CI checkout builds every manual from committed image assets
    without Rack, OpenGL, Pillow, or a desktop session.

## Non-Goals

No AI-generated product screenshots, browser/CSS mockups, replacement UI
implementation, game/demo-ROM generator, runtime redesign, new modules,
or mandatory screenshot refresh during every PDF build. Do not copy the
sibling tools' module coordinates or assume their platform coverage here.

## Acceptance Criteria

- [x] All 14 manuals have reviewed production PNGs and vector panel guides;
      both blank widgets have smoke coverage and disabled entries are excluded.
- [x] Geometry, fixtures, crop rules, and module mapping are documented;
      failed/incomplete rendering preserves existing tracked assets.
- [ ] Light/dark themes and previews are reviewed, including custom displays;
      captured controls match current constructors and runtime SVGs.
- [x] Callouts agree with manual sections and source positions; captures
      and complete rebuilt manuals are reviewed at readable resolution.
- [x] README/covers use the same PNGs; stale exports have no remaining
      consumers; normal PDF builds run without capture dependencies.

## Validation

The following root commands are the proposed tool interface, to be created
by this spec. Native capture needs Rack 2 headers/library/resources, C++
compiler, a working OpenGL desktop session, Python 3, and Pillow. Substitute
an SDK path through `RACK_DIR` when outside the normal Rack checkout:

```shell
make -C tools/capture capture
make -C tools/capture screenshots
make -C tools/capture screenshots MODULE=PotKeys
make -C manual
git diff --check
git status --short
```

Inspect all light/dark/preview images and final cover/reference pages.
Exercise wrong geometry, missing assets, and failed renderer paths in a
disposable output directory, confirming no tracked PNG is replaced. Verify
the selected-module command updates only its intended file. Test ordinary
manual builds in an environment without the capture runtime. Record actual
OS/architecture and unavailable native checks separately from PDF success.

## Completion Evidence

### Completion And Archive Decision

2026-10-01: the user confirmed specs 001-005 are complete and requested
archiving. Marked this spec COMPLETE and moved it to `specs/archive/`.
This decision supersedes the earlier pending-status and acceptance notes
below. Historical checkboxes, test results, and limitations are retained;
no additional runtime, listening, CI, or publication checks are claimed.
Work explicitly owned by specs 006-012 remains with those specs.

Archive validation: checked relative links and heading anchors across the
specs and manual guide, confirmed all five archived statuses and updated
references, and ran `git diff --check`; all passed.

### Implemented And Verified On October 1, 2026

-   Added `tools/capture/` using production `init()` and registered widget
    constructors, an isolated user directory, fixed RNG seed, defaults at
    48 kHz and 4,800 process calls. No patch, audio device or ROM is used.
    `modules.json` reviews all 16 enabled identities and actual widths.
-   Captured all 16 live/null-module widgets, light/dark/light preference
    toggles and Rack context destroy/create events. All 96 retained images
    passed asset, geometry, framebuffer and GL checks. The 32 restored views
    matched their initial light pixels exactly. Reviewed actual algorithm,
    factory waveforms, control caches, sliders, lights, titles and blanks.
-   Addressed the user's additional artwork findings: all 18 registered
    panel footers initially used a 60-pixel-wide wordmark with a 370.5
    vertical center; the legibility follow-up below supersedes that size
    and placement. Pallet Town uses white for contrast.
    Blocks and Name Corp title contours now render as independent solids and
    holes, retaining their outlines without Rack's compound-path artifacts.
-   Produced 14 native-density 760-pixel-high `Panel.png` crops and 14 vector
    guides. Shared TikZ symbols use exported production bounds; reviewed
    numbered regions match each control reference. Corrected Boss Fight's
    mistaken slider wording. Super Echo's missing sliders are explicitly
    absent in both screenshot and guide; 006 still owns their repair.
-   README and covers share the PNGs. Removed the interim artwork note and
    all 56 obsolete Module/Interface SVG/PDF exports after replacement review.
    Retained conceptual operator/envelope/chip illustrations and attribution.
    Recorded capture/figure provenance without relicensing artwork or Rack
    component graphics.

### Validation

-   `make -C tools/capture capture`: passed on macOS 26.6.2 arm64, matching
    local Rack 2.6.0 headers/library/resources, OpenGL, 2x Retina density.
    The guide records the base revision; ignored `batch.json` records the
    exact source-input and library hashes, fixture and geometry per run.
-   Full `screenshots` export and `draw_panels.py`: passed for all 14 manuals.
    A real `make -C tools/capture screenshots MODULE=PotKeys` changed only
    Pot Keys' file timestamp and reproduced its full-batch PNG bytes exactly.
    The other 13 timestamps and all image hashes remained unchanged.
-   `make -C tools/capture test`: seven groups passed using real native
    images in a disposable publication tree: late-batch geometry mismatch,
    missing output, blank rendering, stale sources, destination-I/O rollback,
    selected-module isolation and complete native-density export.
-   `make -C tools/capture test-native`: wrong geometry and a missing panel
    failed through the native renderer in disposable directories. A renderer
    process returning failure preserved the prior intermediate batch. All
    existing publication PNGs remained byte-identical in every failure case.
-   `make -C manual`: passed after removing legacy exports. All 14 PDFs
    (115 pages) were rendered with Poppler and visually reviewed, including
    complete page layouts and readable cover/control-map details. Metadata,
    outlines, links, text extraction, source-relative outputs and identical
    collection copies passed. No unresolved references or overflow survived.
-   `python3 manual/latex/test-build.py`: all seven build-failure/recovery
    groups passed in a disposable source tree with no Rack/capture runtime.
    A separate
    copy containing only `plugin.json` and `manual/` built all 14 PDFs with
    `RACK_DIR=/nonexistent-sdk`; it had no `src/`, `tools/`, SDK or Rack library.
    The publication completeness validator passed with Poppler on PATH.
-   Documentation consumers, manifest mapping, source input paths and
    `git diff --check` passed. No DSP change or listening claim is made.

### Brand Legibility Follow-Up On October 1, 2026

-   Matched Fourier's native footer scale: 120.4554 by 11.2471 Rack pixels
    for the full mark. Panels below 12 HP use its unchanged square emblem,
    11.3938 by 11.2471 pixels, instead of shrinking the lettering. All 18
    registered panel SVGs center their selected mark at a top of 366.882
    pixels. Boss Fight moves its mark to the full panel's horizontal center;
    Pallet Town retains white fill. No control or title artwork changed.
-   `make -C tools/capture screenshots`: passed on the same macOS arm64 /
    Rack 2.6.0 / 2x Retina setup. Reviewed footer clearance on all 16 enabled
    panels, including both blanks and the tight 12 HP Super ADSR layout.
    Footer pixels matched across all six live/preview/preference/context
    views per panel. Disabled panels received the same SVG placement rule
    but remain outside the native capture inventory.
-   Refreshed all 14 publication PNGs. Pixel comparisons against the previous
    committed images confirmed that every change is confined to the footer;
    dimensions and all artwork above image row 733 remain unchanged. All 18
    SVGs parse and their content preceding the footer is byte-identical.
-   `make -C tools/capture test`: all seven export/rollback checks passed.
    `make -C manual`: all 14 PDFs rebuilt; all 115 pages were rendered and
    their layouts reviewed. Metadata, outlines, links and text checks passed,
    as did `python3 scripts/validate.py manuals manual/.build` with Poppler
    on PATH and `git diff --check`. This artwork-only follow-up adds no DSP
    or listening verification; the dark-panel dependency below remains.

### Remaining Dependency

Keep this spec IN PROGRESS solely for the actual light/dark artwork and
native selection review owned by 008. This implementation exercises the
preference in all live/preview widgets, but all currently use their existing
single SVG. Identical light/dark images are not evidence of dark-theme
support. After 008 supplies panel pairs and wiring, add its panel-selection
assertions here, regenerate the 64 theme/preview views and review them,
then mark the remaining criterion complete and archive this spec.

Linux native rendering is unverified; Windows capture is unsupported. Normal
manual builds are independent of those graphical-platform limitations.
No commit, push, issue update or release was requested for this turn.
