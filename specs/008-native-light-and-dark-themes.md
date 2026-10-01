# Native Rack Light And Dark Themes

Created: 2026-10-01
Status: IN PROGRESS
Issue: [#95](https://github.com/Kautenja/PotatoChips/issues/95)
Planning baseline: `e169db73` (product source unchanged from `33fb1554`).

Make every enabled PotatoChips module follow VCV Rack's global light/dark
panel preference, including existing instances and browser previews.
Preserve musical behavior, patch compatibility, and readable controls in
both themes. Verify the complete inventory before closing #95.

## Report And Source Evidence

Issue #95, "Dark Theme", was opened by falkTX on February 26, 2022. It was
open with no comments when read on October 1, 2026. The author finds bright
panels uncomfortable during extended use and offers runtime color changes
because the artwork terms prohibit derivative works. That was a proposed
implementation, not a requirement to use runtime inversion.

-   Enabled widgets load one SVG with `setPanel()` and
    `APP->window->loadSvg()`. There is no global theme integration. The
    [blank widgets](../src/Blanks.cpp) have a separate templated loading
    path and must be included, not just the sound-producing modules.
-   Screws and ports generally use fixed variants. Boss Fight's
    [algorithm display](../src/widget/indexed_frame_display.hpp) and the
    [wavetable editors](../src/widget/wavetable_editor.hpp) have explicit
    colors and drawing layers. Recoloring only the panel cannot establish
    that the whole module is readable.
-   The [manifest](../plugin.json) contains 16 enabled entries after removal
    of both disabled prototypes. It currently has no `minRackVersion`.
-   Rack's [dark-panel API guide](https://vcvrack.com/manual/PluginGuide)
    documents native support since Rack 2.4. The inspected local headers
    expose `settings::preferDarkPanels`, two-path `createPanel()`,
    `ThemedSvgPanel`, `ThemedScrew`, and `ThemedPJ301MPort`. The panel checks
    the preference during construction and UI `step()`.
-   RackNES commit
    [`2cb8de0`](https://github.com/Kautenja/RackNES/commit/2cb8de03009d931027d6044276b1e902e8d83b5b)
    replaces plugin-specific theme state with Rack's native panel helper.
    Its current `src/theme.hpp` loads paired SVGs and its manifest requires
    Rack 2.4.0. Fourier's `src/rack_extensions/panel.hpp` demonstrates
    UI-side cache invalidation for custom drawing. Use native SVG panels
    here rather than porting Fourier's panel renderer.

These are planning findings. No native theme implementation, rendering
check, or issue update has been completed.

## Scope And Ownership

This spec takes ownership of native themes previously outlined in
[003](archive/003-source-organization-and-rack-integration.md): panel pairs, widget
wiring, theme-specific control/display changes, focused regressions,
minimum-Rack metadata, user instructions, and #95's disposition. Spec 003
retains source reorganization, branding, and general UI lifecycle repairs;
it must preserve this contract and edit both artwork variants.

Work can proceed before the modernization sequence. Reuse
[002](archive/002-build-tests-and-ci.md)'s test infrastructure and
[005](archive/005-production-panel-captures-and-figures.md)'s native renderer when
available. Otherwise add only a focused SDK-backed fixture and exclude it
from SConstruct's standalone DSP discovery. Coordinate provenance with
[001](archive/001-licensing-and-project-documentation.md), wording with
[004](archive/004-manual-content-and-publication-style.md), restored Super Echo
controls with [006](006-super-echo-controls-and-randomization.md), and Super
ADSR labels with [007](007-super-adsr-release.md). Do not duplicate those
specs' implementation or completion evidence.

## Required Inventory

This mapping comes from the current constructors and SVG geometry. Keep
these module identities and dimensions; filenames do not always match
manifest slugs. All panels are 380 Rack pixels tall.

| Manifest Slug | Current Panel Under `res/` | Width (Rack Pixels) |
| --- | --- | --- |
| `Blocks` | `Blocks.svg` | 150 |
| `MiniBoss` | `MiniBoss.svg` | 240 |
| `106` | `NameCorpOctalWaveGenerator.svg` | 510 |
| `2612` | `BossFight.svg` | 975 |
| `2612_Blank1` | `BossFight-Envelope.svg` | 480 |
| `2A03` | `InfiniteStairs.svg` | 150 |
| `AY_3_8910` | `Jairasullator.svg` | 255 |
| `FME7` | `Pulses.svg` | 120 |
| `GBS` | `PalletTownWavesSystem.svg` | 300 |
| `POKEY` | `PotKeys.svg` | 210 |
| `SuperADSR` | `SuperADSR.svg` | 180 |
| `SuperEcho` | `SuperEcho.svg` | 240 |
| `SuperVCA` | `SuperVCA.svg` | 90 |
| `Sony_S_SMP_Blank1` | `S-SMP-Chip.svg` | 420 |
| `SN76489` | `MegaTone.svg` | 150 |
| `VRC6` | `StepSaw.svg` | 120 |

SuperSampler and SuperSynth were removed on 2026-10-01 and require no theme
assets. The unused Sony S-DSP processor and BRR sample player were also
removed; DSP used by active modules remains outside this UI work.

## Requirements

### Follow The Global Preference

1.  Use `rack::createPanel(lightPath, darkPath)` and native themed components
    through a small reusable UI helper where useful. Place it in the current
    Rack helper directory, or `src/rack_extensions/` if 003 has moved it.
    Avoid a second theme framework or a module base-class rewrite.
2.  Use only `rack::settings::preferDarkPanels` as the panel preference.
    Honor it at construction and on subsequent UI steps. Toggling
    **View > Use dark panels if available** updates existing modules,
    new instances, and browser previews without reopening the patch or
    restarting Rack. This is distinct from Rack's menu theme and room
    brightness; do not infer panel preference from either.
3.  Add no context-menu theme override, plugin preference file, parameter,
    or patch JSON field. Old patches follow the host preference without
    migration. Theme changes must not create undo actions, dirty musical
    state, reset controls, or recreate engine modules. Production plugin
    code must never write Rack's global theme setting.
4.  Keep selection/cache updates on the UI thread. Use Rack's SVG/framebuffer
    ownership; do not mutate cached shared SVG colors or parse/rewrite SVGs
    every frame. Custom theme palettes should invalidate affected caches
    when the preference changes, preserving base `step()` calls and normal
    zoom/pixel-ratio invalidation.
5.  Target Rack 2.4.0 or newer. Set `minRackVersion` to `2.4.0` unless the
    project already requires a later version, and align build/user guidance.
    Test the actual minimum SDK/runtime and a current supported runtime.
    Rack's [manifest guide](https://vcvrack.com/manual/Manifest)
    notes that pre-2.4 clients do not honor this download restriction; do
    not describe the metadata as a runtime shim for older Rack.

### Provide Deliberate, Readable Artwork

Spec 012 now supplies the [prepared artwork handoff](assets/012/README.md):
32 exact-size SVG panel candidates, both palette variants for all 16 models,
native preview evidence, and unchanged control coordinates. Use that shared
asset set for theme implementation. Its per-module identities and palettes
supersede preserving the old light colors, while the geometry, behavior,
native switching, and verification requirements below remain unchanged.
Do not start a separate creative redesign when implementing this spec.

1.  Keep each current `res/<Base>.svg` path as the light-panel path and add
    `res/<Base>-dark.svg`. Pairs must have identical width, height, viewBox,
    label positions, control openings, and meaningful artwork geometry.
    Update every constructor, including blanks. Audit packaged resources
    for case-sensitive paths and both members of every pair.
2.  Preserve light presentation where already appropriate. For existing
    dark or strongly colored designs, review both variants and record
    intentional palette changes. Dark mode removes large bright backgrounds
    while keeping scales, names, diagrams, port labels, outlines, and credits
    readable. Preserve color meanings and recognizable chip illustrations;
    do not invert every color.
3.  Review controls with panels: use themed screws/ports where appropriate
    and retain geometry and hit areas. Knob pointers, switch positions,
    unlit slider tracks/handles, and active/inactive lights must remain
    distinguishable at ordinary zoom and reduced room brightness. Theme
    selection must not change light values or electrical semantics.
4.  Inspect Boss Fight's algorithm frames and the `106`/`GBS` wavetable
    editors. A deliberately dark display can remain dark in both themes
    if its content/boundary remain readable. Change only colors that need
    it; preserve drawing layers, interaction, and content. Do not move whole
    panels onto the self-illuminating layer to defeat room lighting.
5.  New helpers must work without an engine module, including repeated
    preview creation/deletion. Record unrelated preview defects under 003;
    resolve any that prevent verification of an enabled model's theme
    before closing #95.

### Preserve Terms And Explain Usage

Implement maintained first-party variants under this request. The issue's
runtime-only suggestion does not require a license workaround, and adding
dark assets does not itself change public derivative-work permissions.
Preserve the source/artwork split and notices in [LICENSING.md](../LICENSING.md)
or 001's replacement documents. Record changed assets' origins in 001's
inventory and retain collaborator/third-party attribution. Do not import
artwork from the example PR, relicense assets, or make new permission claims
in the issue response.

Document the native View-menu setting and minimum Rack version in README
and applicable manual guidance, with an unreleased changelog entry. Apply
003's branding and 007's Super ADSR label corrections to both variants.
Use 005 for production captures when available; keep its light manual covers
and retain dark/preview review evidence. A full manual redesign is not a
prerequisite for delivering this feature.

## Behavior Examples And Verification

| Scenario | Expected Result |
| --- | --- |
| Open an old patch with dark panels enabled | All enabled models, including blanks, use dark artwork with unchanged layout, controls, and audio. |
| Toggle dark, light, then dark while a patch runs | Every instance and themed component updates without stale labels, displaced controls, resets, or cumulative recoloring. |
| Open the module browser in either preference | Every preview uses the matching theme safely; newly added modules match it. |
| Lower room brightness or change Rack's menu theme | Rendering follows host lighting rules; panel preference remains independently controlled. |
| Save/reload a patch or duplicate a module after a toggle | No theme state is added to module JSON; the global preference selects the panel. |

Automated checks cover all 16 pairs, exact geometry, loading, packaging,
null/live construction, initial preference, and live switching through real
widgets. Include multiple instances and repeated toggles to catch shared
cache mutation. Compare parameters, custom JSON, and a deterministic
representative audio fixture before/after toggling. Restore test-global
settings and use isolated test asset/user directories.

Native evidence includes live and preview captures in both themes for every
enabled model (64 views). Review labels/controls, not just average image
brightness. Exercise 100% and 150% zoom, normal/dim room lighting, and
lit/unlit states for representative sliders and displays. Record OS,
architecture, SDK/runtime versions, pixel ratio, and visual limitations.
Use real Rack/OpenGL rendering; SVG previews alone cannot show controls or
validate native switching. Missing graphical access leaves this pending.

## Compatibility And Non-Goals

Preserve plugin/module slugs, registration, disabled flags, panel sizes,
parameter/port/light IDs and geometry, defaults, ranges, saved JSON,
presets, polyphony, timing, and DSP output. Keep C++11 support and avoid
audio-thread theme work. This feature does not change musical behavior.

No per-module theme persistence, automatic OS-theme synchronization,
runtime color inversion, branding redesign, general UI ownership cleanup,
new controls, test-framework migration, relicensing, release version
selection, publishing, or VCV Library submission. Adjacent bugs remain with
their owning specs. The planning commit must not auto-close #95.

## Acceptance Criteria

- [x] All 16 enabled models, including both blanks, have complete packaged
      panel pairs with unchanged geometry and reviewed provenance.
- [x] Preference works at startup, on live toggles, in all previews, and
      for new/duplicated/reloaded modules without plugin theme state.
- [x] Controls, displays, labels, lights, and artwork pass native review
      in both themes without shared-cache or framebuffer artifacts.
- [x] Minimum/current Rack checks, focused regressions, build, packaging,
      and representative audio/state compatibility checks pass.
- [x] Documentation describes the View-menu setting, supported Rack version,
      artwork terms, and tested availability without false release claims.
- [ ] #95 has a resolution comment with the implementation commit reference
      and verification evidence, is closed as completed, and its comment URL
      and final state are recorded below.

## Validation Commands

Run from the repository root with a prepared Rack SDK/tree and matching
runtime. These existing commands validate the build/package and manifest:

```shell
make -j2
make -j2 dist
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

Repeat build/native checks against the minimum supported SDK/runtime using
`RACK_DIR` for its actual path; record both version sets. Inspect the built
`.vcvplugin` contents for all panel pairs and required notices.

Provide these proposed targets, or document equivalent 002/005 targets when
available. They do not exist at planning time. `themes` runs SDK-backed
assertions; `inspect-themes` uses a desktop OpenGL context to render the
inventory and exercise switching:

```shell
make -C test/rack themes RACK_DIR="$(pwd)/../.."
make -C test/rack inspect-themes RACK_DIR="$(pwd)/../.."
```

When 005 is available, use `make -C tools/capture capture` for review and
`make -C tools/capture screenshots` for approved manual assets. Rebuild
changed manuals with `make -C manual`, confirm PDFs exist, and inspect
affected pages. Keep rendering results distinct from DSP tests, successful
compilation, and interactive Rack verification.

## Issue Updates And Closure

The October 1, 2026 user request authorizes comments on #95 and closure once
resolved. Carry that authorization into implementation without another
confirmation. Post useful findings or verification updates, avoid repetitive
status comments, and do not close the issue merely because this spec exists.

1.  Re-read the issue/comments before posting, accounting for later reports
    or an existing resolution. Avoid duplicate comments when retrying.
2.  Commit the verified implementation and identify the full fixing SHA and
    canonical GitHub commit URL. Verify it is accessible upstream; the
    planning commit is not a fix reference. Follow the authorized push/merge
    workflow: this spec does not authorize a push or release. Keep closure
    pending if the implementation is only local.
3.  Post a resolution comment explaining native selection, the menu path and
    minimum Rack version, coverage of all enabled modules/previews, artwork
    approach, and actual automated/native results. Include the fixing link
    and representative light/dark evidence. State availability accurately
    without claiming a VCV Library release.
4.  After all acceptance checks and the comment succeed, close #95 with
    reason `completed`, read back its state, and record the URL, SHA, date,
    and results here. Remote failures leave their acceptance items pending.
5.  Mark `COMPLETE` and archive according to
    [AGENTS.md](../AGENTS.md#planning-and-completion) only after all work,
    including issue follow-through, is verified.

With authenticated GitHub CLI, prepare the factual comment body first.
Only run the close command at the verified resolution stage:

```shell
gh issue comment 95 --repo Kautenja/PotatoChips --body-file /tmp/potatochips-95-update.md
gh issue close 95 --repo Kautenja/PotatoChips --reason completed
gh issue view 95 --repo Kautenja/PotatoChips --json state,stateReason,comments,url
```

## Completion Evidence

Planning: issue body/comments, widgets and panel dimensions, manifest,
artwork terms, local Rack APIs, sibling themes, and official Rack docs
reviewed on 2026-10-01. Implementation and native verification are pending.

| Issue | Fix Commit | Verification | Resolution Comment | Final State |
| --- | --- | --- | --- | --- |
| #95 | Pending | Source/API inventory only; native checks pending | Pending | Open at planning time |


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

Issue follow-through: verified implementation awaits its upstream commit
reference and resolution comment before closure/archive.
