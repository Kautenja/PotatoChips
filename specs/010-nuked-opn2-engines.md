# Selectable Nuked-OPN2 Engines

Created: 2026-10-01
Status: PLANNED
Issue: [#83](https://github.com/Kautenja/PotatoChips/issues/83)
Planning baseline: `f7d07f36` (product source unchanged from `33fb1554`).

Add optional Nuked-OPN2 YM2612 and YM3438 engines to Mini Boss and Boss
Fight. Preserve the current engine as the default, expose the audible
differences through a saved context-menu choice, and verify control mapping,
polyphony, and real-time cost before resolving #83.

## Report And Source Evidence

Issue #83 requests Nuked-OPN2, particularly the YM2612 DAC/ladder effect
missing from the current implementation. The December 2020 discussion
suggests three choices: the existing engine, Nuked YM2612, and Nuked YM3438.
The reporter also accepts a narrower DAC-only approach. This spec selects
the full-engine approach; a DAC approximation alone does not satisfy its
acceptance criteria. The issue was open with five comments when reviewed
on October 1, 2026.

Relevant local evidence:

-   [MiniBoss.cpp](../src/MiniBoss.cpp) renders a `FeedbackOperator` per
    polyphonic voice, including external audio-rate phase modulation.
    [BossFight.cpp](../src/BossFight.cpp) renders a `Voice4Op` with independent
    operator pitches, gates, envelope controls, and modulation sensitivities.
    Both support 16 voices and save `prevent_clicks` for the soft-reset menu.
-   [FeedbackOperator](../src/dsp/yamaha_ym2612/feedback_operator.hpp),
    [Voice4Op](../src/dsp/yamaha_ym2612/voice4op.hpp), and
    [Operator](../src/dsp/yamaha_ym2612/operator.hpp) adapt MAME-derived code.
    The modules scale the operator output and clamp to signed 14-bit audio;
    that clamp is not a model of the YM2612 output DAC's ladder behavior.
-   The current looping-envelope switch is a simplified behavior, not a
    selector for all hardware SSG modes. [009](009-ym2612-ssg-retriggering.md)
    separately owns its retrigger defect. Changing engines does not resolve
    #82 or excuse a regression in the existing engine.
-   [Makefile](../Makefile) discovers top-level module/DSP C++ sources;
    [standalone Make rules](../mk/standalone.mk) discover the existing DSP test
    directories. Neither
    currently integrates the upstream C core. The shared `CLOCK_RATE` and
    existing operator tuning must not be assumed to be a hardware master
    clock suitable for this core.

Use upstream commit
[`335747d78cb0abbc3b55b004e62dad9763140115`](https://github.com/nukeykt/Nuked-OPN2/commit/335747d78cb0abbc3b55b004e62dad9763140115)
as the research baseline; record the exact reviewed revision actually
vendored. Its [header][upstream-header] and [implementation][upstream-source]
show a clocked register interface, six channels, and a process-global chip
type. `OPN2_SetChipType()` takes no chip pointer: switching it per module
would let concurrent Rack instances interfere. The pin output is time
multiplexed; YM2612 mode changes its output behavior, including an offset.
Upstream labels that DAC emulation unverified. Do not claim verified
hardware accuracy from a comparison with this implementation alone.

No adapter prototype, audio comparison, or performance measurement has run
during planning. The control-mapping requirements below need experimental
verification, especially external modulation and independent operators.

## Scope And Dependencies

Own the optional engines, their adapter, focused tests/benchmarks, menu and
saved selection, targeted manuals, and unreleased changelog entry. Keep
the reusable adapter independent of Rack. Vendor the reviewed core under
`dep/Nuked-OPN2/` with its origin, exact commit, license, and documented local
patches. Preserve its LGPL-2.1-or-later notices and existing MAME attribution;
include required dependency texts in source and distributed plugin packages.
Coordinate the dependency inventory with
[001](archive/001-licensing-and-project-documentation.md).

Reuse [002](archive/002-build-tests-and-ci.md)'s build/test infrastructure when
available; otherwise extend the existing harness narrowly, including C
compilation/linking and SDK-test isolation. Keep builds reproducible without
fetching a moving upstream branch. Coordinate source moves with
[003](archive/003-source-organization-and-rack-integration.md), operating guidance
with [004](archive/004-manual-content-and-publication-style.md), and any needed
captures with [005](archive/005-production-panel-captures-and-figures.md).

## Requirements

### Prove The Control Mapping First

Prototype both modules before polishing the menu. Record how every existing
control reaches each engine, the units and update cadence, and intentional
sound differences. Exercise these particularly difficult mappings:

| Existing Behavior | Required Investigation And Result |
| --- | --- |
| Mini Boss external FM and feedback | Preserve audio-rate phase modulation and its depth/polarity, alongside internal feedback. The upstream public register interface does not directly expose external operator modulation. |
| Boss Fight algorithms and independent operators | Map all eight algorithms, operator ordering, independent pitch/key events, and per-operator AMS/FMS. Investigate channel-3 special frequency mode; ordinary channel-wide controls alone do not represent all current controls. |
| Loop switch and soft reset | Preserve the module's documented looping/retrigger contract and both `prevent_clicks` settings. Explain the mapping to the core's SSG and key-on behavior without silently redefining the saved boolean. |
| Volume and saturation | Preserve meaningful control ranges and voltage conventions. Check ordering against the current gain-before-clamp path; post-DAC gain alone is not automatically equivalent to Boss Fight's saturation control. |
| Pitch, multiplier, and LFO | Retain knob/CV units, V/oct tracking, multiplier zero's half-frequency meaning, and usable rates. Derive clock/tuning conversion instead of copying the current empirical tuning factor into a different core. |

Prefer adapter code over core modifications. Where a module behavior needs
an extension, keep it narrow, documented, and testable against unmodified
upstream behavior when the extension is disabled. Both optional engines
must actually use Nuked synthesis and envelopes; do not relabel the old
oscillator plus a bitcrusher as Nuked. Do not ship inert controls, reduced
polyphony, or an undocumented substitute for external FM. If the prototype
cannot meet this contract at acceptable cost, record the evidence and
revise the scope explicitly before claiming implementation complete.

### Integrate Clocking, Writes, And Output

1.  Give every polyphonic voice independent chip/adapter state. Eliminate
    process-global mode changes from the rendering path, using a minimal
    per-instance mode patch or independently isolated fixed-mode cores.
    Test mixed engines across modules and concurrent Rack engine workers.
    A mutex around a global chip selector is not an audio-thread solution.
2.  Derive the master-clock-to-core-clock relationship from the pinned
    implementation. Advance fractional host/core time without cumulative
    rounding drift at 44.1, 48, and 96 kHz. Document the selected master
    clock, latency, pitch tolerance, filtering, and resampling method.
    Test sustained notes and sample-rate changes; do not assume the existing
    BLIP clock quantization is appropriate without measuring it.
3.  Respect address/data strobes, write timing, and register bandwidth.
    Use bounded, preallocated scheduling with an explicit ordering policy
    for simultaneous controls and key events. Coalesce only replaceable
    parameter updates, never key edges. Verify worst-case polyphonic CV
    traffic without silently overwriting pending writes or starving notes.
4.  Reconstruct audio from the core's output pins across the channel scan,
    with defined inactive-channel, panning, filtering, and mono conversion
    behavior. Reading an internal pre-DAC channel accumulator would bypass
    the requested effect. Separate the output DAC model from the chip's
    PCM/DAC input feature, which is outside this task.
5.  Match each mode against the pinned unmodified core using independent
    register/event fixtures where hardware control mappings apply. Then
    test module-specific extensions separately. Include quiet decays and
    zero crossings that distinguish YM2612 output behavior from YM3438;
    account for gain/duty-cycle differences when evaluating the effect.

### Save And Switch Engines Safely

Add an "Emulation Engine" context submenu to both modules with these stable
choices and proposed JSON values under `emulation_engine`:

| Menu Choice | Saved Value | Default |
| --- | --- | --- |
| Existing (MAME-derived) | `legacy` | Yes |
| Nuked-OPN2 YM2612 | `nuked_ym2612` | No |
| Nuked-OPN2 YM3438 | `nuked_ym3438` | No |

Missing, malformed, and unknown values load the existing engine. Preserve
`prevent_clicks`, all model slugs, Rack IDs, ranges/defaults, normalling,
and old patch/preset meanings. The existing mode must retain its audio
behavior at the implementation baseline, including any landed 009 fix.
Keep fixed regression renders for that mode; new engines may intentionally
sound different but must retain the controls' documented purpose.

Use a synchronized UI-to-engine handoff. Prepare storage outside the audio
callback and apply selection at a defined processing boundary. Rebuild the
selected engine from current controls and gate state; do not copy opaque
state between cores. Bound discontinuities with a short, specified output
transition and document whether active envelopes/tails restart. Avoid
allocation, blocking, or unbounded initialization in `process()`. Test
rapid menu changes, reset, sample-rate changes, duplicate/copy/paste,
save/reload, and undo/redo where Rack supports the action. Persist selection
without a data race and display the selected item consistently.

For example, an old Mini Boss patch opens with the same existing engine.
Choosing YM2612, saving, and reopening retains that choice. Another module
using YM3438 remains unaffected while the first switches engines. A held
Boss Fight chord survives the defined transition without a stuck gate or
out-of-range burst; any envelope restart follows the documented policy.

### Verify Cost And Document Behavior

Measure both modules at 1, 4, and 16 voices, all engines, and mixed-engine
instances. Use 44.1/48/96 kHz and representative 64/256-frame workloads.
Record hardware, compiler/build flags, worker settings, memory use, median,
tail, and worst observed processing time over repeated runs. Establish and
record the supported-machine callback budget during the prototype, then
verify the implementation against it in native Rack. Do not claim universal
real-time performance from one computer or reduce polyphony to pass.
Inactive engines must not add continuous synthesis work to the legacy path.

Update both module manuals with engine selection, saved/default behavior,
sound and CPU tradeoffs, modulation/loop mapping, and switching behavior.
Correct touched descriptions that equate existing 14-bit clipping with a
hardware DAC model. Describe measured differences without promising perfect
hardware emulation. Keep panel controls and theme behavior unchanged.

## Non-Goals

No new module slugs, PCM input, restored full SSG-mode UI, hardware presets,
hard-sync feature from declined #94, unfinished sampler/synth activation,
or broad chip/DSP replacement. Do not remove the existing engine, select a
release version, publish a release, or close #83 from this planning commit.

## Acceptance Criteria

- [ ] Both modules offer three persistent choices; old and malformed patch
      data safely select the existing engine with preserved legacy output.
- [ ] The prototype establishes complete control mapping, including Mini
      Boss external FM, independent Boss Fight operators, looping, soft
      reset, volume/saturation, and 16-voice operation.
- [ ] Pinned-core comparisons validate clock/write/output integration, and
      renders demonstrate the intended YM2612/YM3438 output differences.
- [ ] Deterministic tests cover mixed concurrent modes, voice isolation,
      register bursts, reset, active switching, reload, and sample-rate
      changes. 009's event/loop regressions remain passing when available;
      equivalent contract coverage is required for the new engines.
- [ ] Native Rack verifies both modules, all modes, four-voice playing,
      16-voice stress, control mapping, save/reload, and engine transitions.
      CPU/memory evidence satisfies the recorded prototype budget.
- [ ] Applicable tests and supported-platform plugin builds pass; dependency
      notices are packaged, local core patches documented, and both manuals
      and changelog match verified behavior.
- [ ] #83 receives a resolution comment linking accessible implementation
      commits and verification, is closed as completed, and its final state
      and comment URL are recorded here.

## Validation Commands

Run from the repository root with a prepared Rack 2 SDK/tree, matching Rack
runtime, C/C++11 toolchain, SCons, and pinned Catch2 v2. Current commands:

```shell
make -j2
scons -j2 test
git diff --check
```

Implement these proposed targets, or record exact equivalents supplied by
002. None exists at planning time. Explicitly wire the vendored C source
into the focused standalone suite; exclude SDK-dependent module tests from
standalone discovery. The benchmark target must exercise production paths.

```shell
scons test/dsp/yamaha_ym2612/test_nuked_opn2.cpp
make -C test/rack ym2612-engines RACK_DIR="$(pwd)/../.."
make -C test/rack benchmark-ym2612-engines RACK_DIR="$(pwd)/../.."
```

Run 009's focused targets once implemented. Build changed manuals with
`make -C manual/MiniBoss` and `make -C manual/BossFight`; inspect the resulting
`build/manual.pdf` files because current recipes can mask TeX failures.
Record render fixtures, comparisons/tolerances, native steps, performance
results, platform builds, and package-license inspection separately. Missing
native access or an unmeasured control mapping leaves acceptance pending.

## Issue Updates And Closure

The October 1, 2026 user request authorizes useful comments on #83 and
closure after resolution, without another confirmation. Re-read its body
and comments before posting; avoid duplicate updates. Share substantive
prototype findings or verified results, not repetitive progress messages.

After acceptance passes, commit the implementation and identify its full
SHA and canonical GitHub link. Follow the authorized push/merge workflow;
this spec does not authorize pushing or releasing. Keep closure pending
while required commits are only local. Post a resolution comment describing
the delivered engines, default/compatibility policy, verified controls,
audio and performance evidence, limitations, and accessible implementation
commit links. Distinguish source availability from a VCV Library release;
the spec commit is not the implementation reference.

After the comment succeeds, close #83 with reason `completed`, read back
its state, and record the comment URL and evidence below. Failed remote
actions remain outstanding. Only then mark `COMPLETE` and archive according
to [AGENTS.md](../AGENTS.md#planning-and-completion).

Prepare a factual comment body file before using the authenticated CLI:

```shell
gh issue comment 83 --repo Kautenja/PotatoChips --body-file /tmp/potatochips-83-update.md
gh issue close 83 --repo Kautenja/PotatoChips --reason completed
gh issue view 83 --repo Kautenja/PotatoChips --json state,stateReason,comments,url
```

## Completion Evidence

Planning: issue and five comments, local module/DSP/build/manual paths, and
pinned upstream API/source reviewed on 2026-10-01. Adapter feasibility,
implementation, executable/native validation, and issue resolution pending.

| Issue | Implementation Commit | Verification | Resolution Comment | Final State |
| --- | --- | --- | --- | --- |
| #83 | Pending | Source review only; prototype/audio/performance checks pending | Pending | Open at planning time |

[upstream-header]: https://github.com/nukeykt/Nuked-OPN2/blob/335747d78cb0abbc3b55b004e62dad9763140115/ym3438.h
[upstream-source]: https://github.com/nukeykt/Nuked-OPN2/blob/335747d78cb0abbc3b55b004e62dad9763140115/ym3438.c
