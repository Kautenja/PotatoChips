# Super ADSR Release Behavior

Created: 2026-10-01
Status: PLANNED
Issue: [#98](https://github.com/Kautenja/PotatoChips/issues/98)
Planning baseline: `4d162a02` (product source unchanged from `33fb1554`).

Resolve the reported missing release in Super ADSR. Reproduce the gate-off
waveform, correct any demonstrated release defect, and align the panel and
manual with the envelope's sustain-rate and fixed key-off behavior. Verify
the result before updating and closing the issue.

## Report And Source Evidence

Issue #98, "SuperADSR release not working", was opened on September 12,
2023. It was open with no comments when read on October 1, 2026. The author
asks whether the observed behavior is intentional. The attached screenshot
shows a gate and envelope on an oscilloscope: the envelope decays while the
gate is high and falls sharply to zero at gate-off. The fifth slider is
labeled `RR`. The report provides no patch, exact parameter values, Rack
version, or sample rate. A static screenshot cannot establish tail duration
or prove that the release stage is absent.

The current sources expose several distinctions that the fix must preserve:

-   [SuperADSR.cpp](../src/SuperADSR.cpp) configures the fifth slider as
    "Sustain Rate", with range 0-31 and default 20. It sends `31 - value`
    to `setSustainRate()`. There is no adjustable release parameter.
-   [ADSR::run()](../src/dsp/sony_s_dsp/adsr.hpp) enters `Release` when the
    gate is low, unless a trigger takes priority or the envelope is already
    off. Release subtracts 8 from the internal envelope each call and clamps
    to zero in `Off`; it does not consult the sustain-rate setting.
-   The DSP comments and [shared constants](../src/dsp/sony_s_dsp/common.hpp)
    describe a 32 kHz chip clock. The module currently calls `run()` once
    per Rack sample. Thus the maximum 256-call release corresponds to 8 ms
    at 32 kHz, about 5.33 ms at 48 kHz, and about 2.67 ms at 96 kHz. These
    are source-derived durations, not native Rack measurements. Determine
    whether this timing discrepancy contributes to the report.
-   The legacy manual called the fifth slider "Release Rate (RR)" while
    describing decay during a held gate and a very short, uncontrollable
    key-off release. The 004 rewrite now calls it sustain rate and explains
    the fixed release in [the manual](../manual/SuperADSR/manual.tex).
    Panel terminology and verified runtime fixes remain owned by this spec.
-   Gate detection uses hysteresis: high at 2 V, low at 0.01 V, with the
    previous state retained between thresholds. RETRIG is a separate rising
    event; its priority over gate-off needs explicit regression coverage.
-   [The current ADSR test](../test/dsp/sony_s_dsp/test_adsr.cpp) checks only
    the eight-byte object size. It does not verify envelope transitions or
    release. Historical commit
    [`535cd150`](https://github.com/Kautenja/PotatoChips/commit/535cd150281d7c28b08a0b8e8b3ee9064bb4c2ea)
    fixed inconsistent release output scaling and zero clamping; preserve
    those corrections.

A temporary C++11 probe against the unchanged DSP reached the peak with
attack 15 and amplitude 127, then held trigger/gate false. At sustain-rate
values 0 and 31, both runs reached `Off` with output zero after 256 calls.
This establishes that the core has a working release path for that case;
it does not reproduce the complete Rack report or establish its root cause.

## Scope And Dependencies

Own the issue-specific changes in `src/SuperADSR.cpp`,
`src/dsp/sony_s_dsp/adsr.hpp` if necessary, focused DSP/Rack regressions,
`patches/debug/SuperADSR.vcv`, `res/SuperADSR.svg`, the Super ADSR manual and
its affected figures, and an unreleased changelog entry.

This work can precede the modernization specs. Reuse
[002](002-build-tests-and-ci.md)'s harness when available; otherwise keep
the current SCons tests working and add only the necessary Rack fixture.
Exclude SDK-dependent fixtures from SConstruct's standalone test discovery.
[003](003-source-organization-and-rack-integration.md) must preserve the
fix; [004](004-manual-content-and-publication-style.md) carries the verified
wording into its rewrite; [005](005-production-panel-captures-and-figures.md)
can supply refreshed production captures. This spec owns #98's resolution.

## Requirements

### Reproduce And Identify The Defect

1.  Re-read the issue and comments. Build the current plugin and record the
    commit, Rack version, OS, sample rate, exact slider values, gate levels,
    pulse duration, and oscilloscope time scale. Use a repeatable patch with
    the gate and OUT visible together and RETRIG initially disconnected.
2.  Recreate the reported short tail using a nonzero sustain level. Sweep
    the fifth slider through 0, 20, and 31. Observe held-gate decay and
    gate-off separately, zooming into the first 10 ms after gate-off. Do not
    mistake an envelope already at zero for a missing release stage.
3.  Measure core release calls and module release time at 44.1, 48, and
    96 kHz. Compare the actual gate state with the DSP stage and output.
    Record whether the problem is gate handling, stage/output processing,
    clocking, misleading labeling, or a combination, with evidence for
    each conclusion. Treat the report's old plugin version as unknown.
4.  Preserve the minimal reproduction as a debug fixture with documented
    settings and before/after results. Missing historical settings are not
    a reason to skip current reproduction. If additional information is
    needed, post a focused question on #98 and retain the unresolved gap.

### Correct The Release Without Changing The Control's Meaning

1.  A gate falling from an active envelope must initiate the fixed linear
    key-off release from its current level and reach zero/`Off`. It must
    not remain stuck, wrap below zero, skip directly to zero from a high
    level, or scale differently merely because the stage changed. Respect
    the existing quantized output, including repeated adjacent values.
2.  Keep sustain rate controlling decay while the gate remains high. Do not
    reinterpret parameter IDs 8 and 9 as conventional release-time knobs.
    The core contract remains a decrement of 8 per envelope tick, clamped
    at zero, independent of sustain rate. Separate this contract from the
    Rack clock that schedules those ticks.
3.  Fix a demonstrated host-clock defect at the module integration boundary
    where practical. Before changing timing, document the chosen clock and
    compare all envelope stages with the existing implementation. A 32 kHz
    chip-clock correction must produce the same duration in seconds across
    host rates, within one chip tick plus one host sample, and must not lose
    gate/retrigger edges between ticks. Preserve fractional clock progress
    appropriately on host-rate changes. Do not silently change all stage
    times as an incidental consequence of repairing release.
4.  Characterize and preserve intentional trigger/retrigger priority,
    including retrigger during release and a retrigger coincident with
    gate-off. Correct a demonstrated ordering defect with explicit expected
    behavior and regression evidence. RETRIG alone must not accidentally
    latch a permanently high gate.
5.  Keep both lanes and every polyphonic voice independent. Preserve signed
    amplitude behavior and `INV = -OUT`, including negative/zero amplitude,
    cable disconnection, reset, patch reload, and sample-rate changes.

If native reproduction shows the fixed release is working and the reported
problem is the misleading `RR` control, resolve the interface/documentation
defect and explain the measured behavior on #98. Do not invent a DSP failure
or claim an adjustable release was added. Any remaining demonstrated release
failure keeps the issue open.

### Align The Panel And Operating Guide

Use "Sustain Rate (SR)" consistently in the runtime panel, parameter help,
manual control list, and affected interface/envelope figures. Explain that
SR changes decay during a held gate; gate-off invokes a short fixed linear
release whose duration depends on the envelope level at key-off. Document
the measured timing and supported gate/retrigger behavior without promising
a conventional adjustable ADSR release. Explain the old `RR` label briefly
so existing patches and screenshots remain understandable.

Update source artwork and required exports together. Review the production
panel and rendered manual, including both lanes. Add a changelog entry that
distinguishes label corrections from any verified processing/timing change.

## Behavior Examples And Regressions

| Scenario | Required Evidence |
| --- | --- |
| Gate falls during attack, decay, or nonzero sustain | Release starts from the current level, decreases monotonically in magnitude at fixed amplitude, reaches zero, and stays off without a new trigger. |
| Sustain rate 0 versus 31 in the DSP | Held-gate behavior differs; release from the same internal level takes the same tick count. Account for the inverted slider mapping in Rack. |
| Gate falls after reaching zero, or amplitude is zero | Output remains zero and the state settles correctly; no divide-by-zero or false inference that release is broken. |
| Positive and negative amplitude, OUT and INV | Existing scaling and polarity hold through gate-off; no historical stage-dependent scaling regression. |
| Retrigger held gate, retrigger during release, simultaneous retrigger/gate-off | Documented event priority holds, with no stuck envelope or lost edge. |
| Mono and 16-channel inputs across both lanes | One voice's key-off/retrigger does not change another voice; channel count and disconnected-input behavior remain compatible. |
| Multiple host rates, rate change during release, reset, saved-patch reload | Timing follows the documented clock decision; no stale gate or unexpected output jump. |

Extend the existing pure DSP suite with deterministic stage/level tests,
including a bounded release-to-off test and the historical scaling case.
Exercise voltage thresholds and time scheduling through the real Rack module
in a focused SDK-backed fixture. Use native Rack scope checks for the report
itself; a passing core test or plugin build alone cannot resolve it.

## Compatibility And Non-Goals

Preserve the `SuperADSR` slug, ten parameter IDs/ranges/defaults, four input
and four output IDs, light IDs, saved JSON, and existing patch meanings.
Record any intentional timing change and its musical effect explicitly.
Keep C++11 support, emulator attribution, and bounded allocation-free
processing. Do not change shared trigger semantics to fix this one module.

No new adjustable release parameter/mode, conventional ADSR redesign, broad
emulator rewrite, general test migration, branding rollout, or release
publication. A spec-only commit must not auto-close #98.

## Acceptance Criteria

- [ ] Current Rack reproduction, exact settings, zoomed gate-off evidence,
      and the identified cause are recorded; unknown historical details
      are distinguished from verified current behavior.
- [ ] Every demonstrated release defect is fixed with a regression that
      detects the original failure. Fixed-release and sustain-rate behavior
      are verified separately, including stage transitions and zero clamp.
- [ ] Clocking has an explicit evidence-backed decision; any timing change
      has cross-rate and saved-patch compatibility evidence.
- [ ] Both lanes, polyphony, polarity, thresholds, retrigger ordering,
      disconnect/reset/reload, and host-rate changes pass focused checks.
- [ ] The panel and rendered manual consistently explain SR and the fixed
      release; affected figures and changelog agree with verified behavior.
- [ ] Relevant automated tests and the plugin build pass; native Rack
      verification is complete with no unresolved symptom from #98.
- [ ] #98 has a resolution comment with the actual fixing commit reference
      and verification results, is closed as completed, and its comment URL
      and final state are recorded below.

## Validation Commands

Run from the repository root with the prepared Rack 2 tree/SDK, matching
runtime, C++11 compiler, SCons, pinned Catch2 v2, and manual prerequisites.
These commands exist today:

```shell
scons test/dsp/sony_s_dsp/test_adsr.cpp
scons test/dsp/trigger/test_threshold.cpp
make -j2
make -C manual/SuperADSR
git diff --check
```

Add this focused target during implementation if 002 has not supplied an
equivalent module fixture; it does not exist at planning time:

```shell
make -C test/rack super-adsr RACK_DIR="$(pwd)/../.."
```

Run the broader applicable DSP suite (`scons -j2 test`, or `make -j2 test`
after 002), and record the exact replacement commands if paths change.
Inspect `manual/SuperADSR/.build/manual.pdf`; the shared build rejects TeX
failures. Record native Rack actions, scope measurements, and capture paths
separately. Missing runtime access leaves native acceptance outstanding.

## Issue Updates And Closure

The October 1, 2026 user request explicitly authorizes comments with updates
on #98 and closing it once resolved. Carry that authorization into
implementation without requesting confirmation again. Post meaningful
reproduction findings, focused requests for missing evidence, or a verified
resolution; avoid repetitive progress comments. Planning alone is not a fix.

1.  Re-read the issue/comments before each update or closure and account for
    new reports or an existing resolution. Avoid duplicate posts on retries.
2.  Commit the verified implementation. The resolution comment must explain
    the cause, corrected behavior, SR versus release semantics, actual tests
    and native measurements, and the full fixing SHA with its canonical
    GitHub commit URL. Use the real fix commit, not this planning commit.
3.  Verify the fixing reference is accessible upstream. Follow the existing
    authorized push/merge workflow; this spec does not authorize a push or
    release. If the fix is still local, report that status and leave closure
    pending. Do not claim VCV Library availability merely from a commit.
4.  After acceptance passes and the resolution comment succeeds, close #98
    with reason `completed` and read back the final state. Record the comment
    URL, fixing commit, results, date, and any availability limits here.
    Remote failures leave the corresponding acceptance item outstanding.
5.  Mark this spec `COMPLETE` and archive it according to
    [AGENTS.md](../AGENTS.md#planning-and-completion) only when all criteria,
    including issue follow-through, are satisfied.

For an authenticated GitHub CLI, prepare the factual comment body first.
Commenting is permitted during implementation; the close command is only
for the verified resolution stage:

```shell
gh issue comment 98 --repo Kautenja/PotatoChips --body-file /tmp/potatochips-98-update.md
gh issue close 98 --repo Kautenja/PotatoChips --reason completed
gh issue view 98 --repo Kautenja/PotatoChips --json state,stateReason,comments,url
```

## Completion Evidence

Planning: issue body, comments, and screenshot inspected on 2026-10-01;
module, DSP, trigger, manual, tests, and historical release fix reviewed.
A temporary probe compiled with `c++ -std=c++11 -Isrc` passed both
256-call release checks described above. No production code changed and
no native Rack reproduction or fix has been completed.

| Issue | Fix Commit | Verification | Resolution Comment | Final State |
| --- | --- | --- | --- | --- |
| #98 | Pending | Core planning probe only; full regression/native checks pending | Pending | Open at planning time |
