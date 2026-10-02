# Native Rack Light And Dark Themes

Created: 2026-10-01
Status: COMPLETE
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
    [blank widgets](../../src/Blanks.cpp) have a separate templated loading
    path and must be included, not just the sound-producing modules.
-   Screws and ports generally use fixed variants. Boss Fight's
    [algorithm display](../../src/widget/indexed_frame_display.hpp) and the
    [wavetable editors](../../src/widget/wavetable_editor.hpp) have explicit
    colors and drawing layers. Recoloring only the panel cannot establish
    that the whole module is readable.
-   The [manifest](../../plugin.json) contains 16 enabled entries after removal
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
[003](003-source-organization-and-rack-integration.md): panel pairs, widget
wiring, theme-specific control/display changes, focused regressions,
minimum-Rack metadata, user instructions, and #95's disposition. Spec 003
retains source reorganization, branding, and general UI lifecycle repairs;
it must preserve this contract and edit both artwork variants.

Work can proceed before the modernization sequence. Reuse
[002](002-build-tests-and-ci.md)'s test infrastructure and
[005](005-production-panel-captures-and-figures.md)'s native renderer when
available. Otherwise add only a focused SDK-backed fixture and exclude it
from SConstruct's standalone DSP discovery. Coordinate provenance with
[001](001-licensing-and-project-documentation.md), wording with
[004](004-manual-content-and-publication-style.md), restored Super Echo
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

Spec 012 now supplies the [prepared artwork handoff](../assets/012/README.md):
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
Preserve the source/artwork split and notices in [LICENSING.md](../../LICENSING.md)
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
- [x] #95 has a resolution comment with the implementation commit reference
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
    [AGENTS.md](../../AGENTS.md#planning-and-completion) only after all work,
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

All 16 modules now use Rack's two-path panel factory through
`createThemedPanel`, with native themed screws and ports. The manifest
requires Rack 2.4.0. Each theme uses a committed SVG; no runtime recoloring,
module preference, JSON field or audio-thread work was introduced.
Production `.cpp` diffs contain only panel/screw/port substitutions.

[Spec 012's shared validation record](012-module-rebranding-and-title-system.md#implementation-evidence--october-1-2026)
contains the complete commands, manual/artwork decisions and limits.
[Machine-readable native evidence](../assets/012/implementation.json)
records rendering inputs, minimum SDK/runtime hashes and per-model results.
The [integrated gallery](../assets/012/IMPLEMENTED.md) contains all 16 pairs.

Theme-specific checks on macOS arm64, Retina 2x:

-   `make -j2 all test-rack`: 5,886 assertions and the plugin build pass
    against both local Rack 2.6.0 and official 2.4.0 via `RACK_DIR`.
    The contract test now explicitly includes `<plugin.hpp>` for 2.4.
-   `make -C tools/capture capture` and the same command with
    `BUILD=.build/minimum RACK_DIR=/tmp/potato-rack-2.4/Rack-SDK`:
    live/null widgets, initial dark construction, repeated toggles,
    unchanged state/history/606 bounds and context recreation pass.
    Matching host resources were used in each run. All 288 views per
    host include 75%/150% zoom and dimming outside the mouse spotlight.
-   Four representative chips/effects compare sample-identical 16-channel
    audio against untouched twins through eight preference toggles.
    Both twins receive identical host initialization events. Existing
    patch, preset and audio fixtures also pass unchanged.
-   `make -C tools/capture test` and `test-native`: stale/missing/blank
    captures, identical themes, geometry failures, export rollback and
    missing light/dark SVG rejection pass. Publication crops are exactly
    the dark views. Both host batches pass full geometry/export checks.
-   `make -j2 dist` and `python3 scripts/validate.py package
    dist/KautenjaDSP-PotatoChips-2.1.0-mac-arm64.vcvplugin`: all 32 panels,
    resources and license notices match the packaged files.

All native module pairs were visually reviewed, including narrow headers,
blank illustrations, slider handles, lit/unlit defaults and display traces.
No control moves, clipped titles or cache artifacts were found. Linux and
Windows native rendering and audio-device listening were not run; the
existing cross-platform CI remains the other platform gate. This is source
implementation, not release or VCV Library publication.

### Issue Follow-Through

Implementation commit
[`7c4d14c140c3b9d815c7055b0869f65c6e62a403`](https://github.com/Kautenja/PotatoChips/commit/7c4d14c140c3b9d815c7055b0869f65c6e62a403)
was pushed to `origin/v2.0.2` and verified through GitHub's commit API.
Re-read #95 before posting; it remained open with no intervening comments.
The [resolution comment](https://github.com/Kautenja/PotatoChips/issues/95#issuecomment-5938819683)
records the native setting, minimum version, fixing commit, paired images,
validation and release availability. Closed as `COMPLETED` on October 1,
2026 and verified the returned `CLOSED`/`COMPLETED` state. This resolves
first-party native themes without changing artwork licensing.
