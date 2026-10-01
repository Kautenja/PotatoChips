# Production Panel Captures And Reference Figures

Created: 2026-10-01
Status: PLANNED

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
smoke coverage but no invented manual. Disabled `SuperSampler` and
`SuperSynth` are not required publication assets and remain disabled.

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

- [ ] All 14 manuals have reviewed production PNGs and vector panel guides;
      both blank widgets have smoke coverage and disabled entries are excluded.
- [ ] Geometry, fixtures, crop rules, and module mapping are documented;
      failed/incomplete rendering preserves existing tracked assets.
- [ ] Light/dark themes and previews are reviewed, including custom displays;
      captured controls match current constructors and runtime SVGs.
- [ ] Callouts agree with manual sections and source positions; captures
      and complete rebuilt manuals are reviewed at readable resolution.
- [ ] README/covers use the same PNGs; stale exports have no remaining
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

Capture tooling, fixtures, panel drawings, PNGs, and rendered review are
pending. Preserve the existing assets until replacements are validated.
