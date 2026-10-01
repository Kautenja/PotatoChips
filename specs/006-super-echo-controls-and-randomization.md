# Super Echo FIR Controls And Randomization

Created: 2026-10-01
Status: PLANNED
Issues: [#96](https://github.com/Kautenja/PotatoChips/issues/96),
[#97](https://github.com/Kautenja/PotatoChips/issues/97)
Planning baseline: `937b72d4` (product source unchanged from `33fb1554`).

Restore the eight missing FIR coefficient sliders and make module
randomization preserve bypass, stereo echo mix, and stereo input gain.
Resolve the two reports with focused regression and native Rack evidence.

## Reports And Source Evidence

Both issues were open when read on October 1, 2026, including all comments:

| Issue | Report | Required Outcome |
| --- | --- | --- |
| #96: Super Echo FIR coefficient sliders not rendering | Reported against plugin v2.0.0 in March 2022; an August 2023 comment reports the same symptom in VCV Rack Free 2.4.0. The thread includes two screenshots. | All eight FIR sliders render and work in the real module and browser preview. |
| #97: SuperEcho paramQuantities[...]->randomizeEnabled = false; | Requests that the common Ctrl-R randomization exclude BYPASS, ECHO MIX left/right, and INPUT GAIN left/right. No comments at planning time. | Exactly those five controls retain their values during module randomization; sound-shaping controls remain randomizable. |

[SuperEchoWidget](../src/SuperEcho.cpp) currently adds each FIR input and
attenuverter but comments out the `LEDLightSliderHorizontal` construction,
snapping, and `addParam` calls. Commit
[`33fb1554`](https://github.com/Kautenja/PotatoChips/commit/33fb15540617038fea42c79be38452cb32c03933)
made that change in March 2025 with a missing-symbol TODO. This explains
missing controls in the current checkout; it is later than the original
report and does not establish the original rendering defect's cause.

The inspected local Rack headers define `SvgSlider`, `LightSlider`, and
`VCVLightSlider`/`LEDLightSlider`, but no `LEDLightSliderHorizontal`.
`SvgSlider` supports explicit horizontal handle positions. A namespace
qualification alone therefore cannot restore the removed type. Verify
the selected supported SDK during implementation.

The module configures all eight coefficient parameters and all five
protected controls, but does not set their `randomizeEnabled` flags.
Rack's `Module::onRandomize(const RandomizeEvent&)` checks that flag before
randomizing each quantity. Direct `ParamQuantity::randomize()` does not
check it, so tests must exercise the real module/engine randomization path.
The bypass widget is a nonmomentary switch, not an automatically protected
momentary button. Set the policy in module construction so it also holds
before any widget is created.

The 60 existing Super Echo presets contain 480 FIR coefficient entries,
all integral and within `[-128, 127]`. Use them as compatibility fixtures;
this inventory does not rule out fractional values in users' saved patches.

These are source-level findings. The issue text/comments were retrieved,
but the historical screenshot attachments could not be loaded during
planning. No native reproduction, fix, or visual verification has yet run.

## Scope And Dependencies

Own the relevant constructor/widget code in `src/SuperEcho.cpp`, a small
horizontal slider helper and assets only if needed, focused Rack tests,
`manual/SuperEcho/` updates, and an unreleased changelog entry. Use existing
`presets/SuperEcho/` and `patches/debug/SuperEcho.vcv` fixtures.

This fix need not wait for the broad modernization work. Reuse
[002](002-build-tests-and-ci.md)'s harness and
[005](005-production-panel-captures-and-figures.md)'s native capture tooling
if implemented. Otherwise add only a focused SDK-backed test/inspection
target, documenting its prerequisites. If Rack-only tests are added under
`test/rack/` before 002, exclude them from SConstruct's recursive standalone
test discovery and keep the existing DSP suites working.

[003](003-source-organization-and-rack-integration.md) must preserve these
fixes during header/theme changes; [004](004-manual-content-and-publication-style.md)
owns the broader manual rewrite. This spec owns both issue dispositions.

## Requirements

### Restore The FIR Sliders (#96)

1.  Reproduce the current absence in a fresh module and null-module browser
    preview. Record Rack/plugin versions, OS, zoom, and a native before
    capture. Retain the historical reports as context rather than claiming
    to have reproduced an unavailable old release.
2.  Implement a supported horizontal light-slider composition using Rack
    APIs and real control rendering. Add exactly one parameter widget per
    coefficient, bound to `PARAM_FIR_COEFFICIENT + i`, with its RGB light
    starting at `LIGHT_FIR_COEFFICIENT + 3 * i`, for `i` from 0 through 7.
    Preserve existing row geometry and panel size, using the intended
    `(147, 29 + 43 * i)` placement as the starting reference. Keep handles,
    tracks, light positions, hit regions, and drag direction aligned.
3.  Keep the signed range `[-128, 127]`, defaults `[127, 0, 0, 0, 0, 0, 0, 0]`,
    and coefficient order. Restore integer interaction with Rack parameter
    metadata (`snapEnabled`) in module construction, not only a widget flag.
    Verify endpoints and zero without changing the asymmetric signed range.
    Review any effect on legacy fractional stored values explicitly; do not
    rewrite preset files or migrate saved parameters as part of this fix.
4.  Tracks and handles must remain visible with all lights off, zero CV,
    and bypass enabled. Positive/negative CV must still drive the existing
    green/red indicators. Preserve FIR CV normalling, attenuverters, light
    cadence, and DSP interpretation; do not change processing to make a
    screenshot look active.
5.  Preserve normal Rack interactions: drag, numeric entry, reset, value
    display, module randomize, undo/redo, preset loading, and patch reload.
    Validate the browser preview without dereferencing a null module.
    Verify supported theme behavior, including light/dark if 003 has landed,
    and normal/dim room lighting at more than one Rack zoom level.

### Preserve Five Controls During Randomization (#97)

1.  After configuring their quantities, set `randomizeEnabled = false` for
    exactly this set, unconditionally in `SuperEcho` construction:

    | Parameter | Existing Numeric ID |
    | --- | --- |
    | `PARAM_MIX + 0` (left) | 2 |
    | `PARAM_MIX + 1` (right) | 3 |
    | `PARAM_GAIN + 0` (left) | 20 |
    | `PARAM_GAIN + 1` (right) | 21 |
    | `PARAM_BYPASS` | 22 |

2.  Keep delay, feedback, the eight FIR coefficients (IDs 4-11), and the
    eight FIR attenuverters (IDs 12-19) randomizable. Use symbolic IDs in the
    implementation. Do not override the entire randomizer or randomize and
    then restore protected values; use Rack's standard policy mechanism.
3.  Protection applies to the module Randomize command/shortcut, including
    a module without an attached widget. The panel BYPASS parameter is
    distinct from Rack's host bypass state. Preserve ordinary manual edits,
    CV processing, reset, preset/patch loading, and undo/redo; exclusion from
    randomization must not lock the parameters or reset them to defaults.

## Behavior Examples

-   With no input cables, all eight FIR handles are visible. Dragging the
    third handle to its maximum changes coefficient 3 to 127; the other
    seven coefficients retain their values. Undo restores the prior value.
-   Set mix left/right to -64/96, gain left/right to 0.5/1.5, and BYPASS to
    1. Module Randomize leaves those five values exactly unchanged while
    delay/feedback/FIR shaping can change. Repeat with BYPASS at 0.
-   Load an existing signed-coefficient Super Echo preset. The sliders show
    its stored values in order and audio follows the same FIR processing;
    the preset requires no format conversion.

## Compatibility And Non-Goals

Preserve plugin/module slugs, all parameter/port/light IDs and counts,
existing defaults, JSON meanings, saved presets, and C++11 support. Keep
the current audio path, delay lengths, sample-rate behavior, feedback,
clipping, polyphony, normalling, and bypass processing unchanged.

No echo-engine redesign, preset catalog rewrite, new DSP feature, general
theme/branding rollout, unrelated manual correction campaign, or broad
test-framework migration. Any unrelated defect found while testing needs
its own recorded scope. A planning commit must not auto-close either issue.

## Regression And Visual Checks

-   Add a focused SDK-backed fixture against the real `SuperEcho` class.
    Verify all 23 parameter identities/ranges/defaults, the exact five-value
    exclusion set, and integer coefficient edits. Construct without a widget
    to catch policy configured only through UI initialization.
-   Snapshot the five protected values, invoke the actual module/engine
    randomization path repeatedly, and assert exact preservation for both
    bypass states and asymmetric stereo settings. Verify the remaining
    quantities are enabled and exercised. Use controlled RNG/test fixtures
    or a bounded deterministic sequence; do not require every unprotected
    value to change on each draw, which can legitimately repeat a value.
-   Check a production widget has eight distinct coefficient controls,
    correct parameter/light bindings, horizontal geometry, working hit
    regions, and safe preview construction. A widget-count assertion alone
    is insufficient: render and inspect all eight sliders with lights off
    and with positive/negative CV, including endpoint/zero handle positions.
-   Round-trip an existing preset/patch containing signed coefficients,
    check reset and Randomize undo/redo, and compare representative mono
    and polyphonic audio before/after with identical fixed parameters. This
    checks that restoring UI controls did not alter the DSP signal path.

## Acceptance Criteria

- [ ] #96 has a current before reproduction and reviewed after rendering;
      all eight controls work in the live module and safe browser preview.
- [ ] Binding, range/default/order, integer editing, light-off visibility,
      CV indicators, zoom, lighting, and applicable theme checks pass.
- [ ] #97 preserves exactly the five requested parameters during actual
      module randomization before and after widget creation; both bypass
      states and distinct left/right values are covered.
- [ ] Other shaping controls remain randomizable; manual edits, reset,
      undo/redo, presets, and patch restoration still work.
- [ ] Focused regressions and the Rack build pass; compatibility/audio
      checks show no unintended processing or persistence changes.
- [ ] Super Echo's manual and changelog describe the restored controls and
      randomization policy. Any updated figure is generated/reviewed from
      the fixed widget; do not substitute a drawing for rendering evidence.
- [ ] Fix commits are recorded, each issue receives its own resolution
      comment with an accessible fix commit reference, and each verified
      issue is closed as completed. Comment URLs and final states are saved.

## Validation Commands

Run from the repository root with a prepared Rack 2 tree/SDK and its matching
runtime. These commands exist today:

```shell
make -j2
git diff --check
```

Provide these narrowly scoped targets during implementation (they do not
exist at planning time). `super-echo` runs SDK-backed regression checks;
`inspect-super-echo` renders the real widget and preview in a desktop OpenGL
session. Use 002/005 infrastructure behind them when available:

```shell
make -C test/rack super-echo RACK_DIR="$(pwd)/../.."
make -C test/rack inspect-super-echo RACK_DIR="$(pwd)/../.."
```

Also run the applicable DSP tests (`scons -j2 test` before 002, `make -j2 test`
after it), then `make -C manual/SuperEcho` when changing the manual. Inspect
the resulting PDF; legacy recipes can mask compilation errors. When 005's
tool is available, refresh the reviewed asset with
`make -C tools/capture screenshots MODULE=SuperEcho`, then rebuild the PDF.
Record exact native versions, regression commands, image paths, and manual
Rack actions. Missing graphical access leaves #96's visual gate unverified.

## Issue Comments And Closure

The user explicitly authorized resolution comments and closing issues #96
and #97 once resolved in the October 1, 2026 request. Carry that authorization
into implementation; no further confirmation is needed for these two actions
after their acceptance checks pass. This planning task does not post a
resolution comment or close an issue.

1.  Re-read each issue and its comments before acting, so later reports or
    an existing resolution are accounted for. Assess the issues independently:
    a verified #97 fix can be completed while #96's visual check is pending.
2.  Commit the tested implementation and identify the actual fixing commit
    for each issue, using its full SHA and canonical GitHub commit URL.
    The spec-only commit is not a fix reference. Verify the reference is
    accessible in the repository. Follow the authorized push/merge workflow;
    this spec does not independently authorize a push or release. If the
    commit is only local, record the pending publication and keep the issue
    open until an accessible reference is available.
3.  Post one issue-specific comment describing the resolved behavior, the
    fixing commit link, regression/native verification actually completed,
    and any relevant availability limit. For #96, include native rendering
    evidence and the tested Rack version. For #97, name all five exclusions
    and confirm that shaping controls still randomize. Do not claim a VCV
    Library release merely because the fix is committed.
4.  Once the comment succeeds, close that resolved issue with reason
    `completed`. Verify the returned/read-back state. Avoid duplicate comments
    on retries; recover the earlier comment URL before posting again.
5.  Record both issue comment URLs, fixing SHAs, closure results, validation,
    and date below. Mark the spec `COMPLETE` and archive it only after both
    issues and all acceptance criteria are complete. If remote actions fail,
    retain the committed fix and report the remaining handoff explicitly.

For an authenticated GitHub CLI, the following are completion-stage commands,
not commands to run while authoring this spec. Prepare the two body files
with the verified issue-specific summaries and real commit URLs first:

```shell
gh issue comment 96 --repo Kautenja/PotatoChips --body-file /tmp/potatochips-96-resolution.md
gh issue close 96 --repo Kautenja/PotatoChips --reason completed
gh issue comment 97 --repo Kautenja/PotatoChips --body-file /tmp/potatochips-97-resolution.md
gh issue close 97 --repo Kautenja/PotatoChips --reason completed
gh issue view 96 --repo Kautenja/PotatoChips --json state,stateReason,comments,url
gh issue view 97 --repo Kautenja/PotatoChips --json state,stateReason,comments,url
```

## Completion Evidence

Planning evidence: both issue bodies and all comments read on 2026-10-01;
module, widget, history, preset, manual, and local Rack APIs inspected.
Implementation and executable/native checks remain pending.

| Issue | Fix Commit | Verification | Resolution Comment | Final State |
| --- | --- | --- | --- | --- |
| #96 | Pending | Native before/after and interaction checks pending | Pending | Open at planning time |
| #97 | Pending | Module-randomization regression pending | Pending | Open at planning time |
