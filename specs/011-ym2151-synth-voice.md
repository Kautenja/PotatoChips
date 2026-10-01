# Yamaha YM2151 Synth Voice

Created: 2026-10-01
Status: PLANNED
Issue: [#79](https://github.com/Kautenja/PotatoChips/issues/79)
Planning baseline: `a6e7f086` (product source unchanged from `33fb1554`).

Add a new polyphonic four-operator FM synth voice based on the Yamaha
YM2151 (OPM). Expose its distinctive modulation, detune, and noise behavior
through a usable Rack module, with verified emulation, panel, manual, and
presets before resolving #79. Keep existing modules and patches compatible.

## Report And Source Evidence

Issue #79, "Yamaha YM2151", was opened on November 18, 2020. It proposes
a module similar in purpose to Boss Fight and points to Nuked-OPM and MAME
as possible emulation sources. The maintainer's sole reply supports adding
it and mentions familiarity with MAME. The issue was open with one comment
when reviewed on October 1, 2026. Its video is a timbral illustration, not
a reference recording with known registers, clock, or output processing.

Local evidence:

-   [CHANGELOG.md](../CHANGELOG.md) lists YM2151 under `1.13.0 (TBD)`, but
    [plugin.json](../plugin.json), [plugin.cpp](../src/plugin.cpp), and
    [plugin.hpp](../src/plugin.hpp) contain no YM2151 model. The current
    source search finds only a shared phase-table comment mentioning it.
    The historical roadmap is not evidence of an implemented module.
-   [BossFight.cpp](../src/BossFight.cpp) demonstrates four operator control
    groups, algorithm display, polyphony, and gate/retrigger handling. Its
    YM2612 code also implements independent operator pitch and simplified
    looping envelopes. Reusing that implementation with a new label would
    not establish YM2151 behavior.
-   [ChipModule](../src/engine/chip_module.hpp) assumes a Blargg/BLIP interface.
    A new clocked FM core need not fit that base class. Keep its reusable
    adapter free of Rack types and use a focused host wrapper.
-   [Makefile](../Makefile) and [standalone Make rules](../mk/standalone.mk) need explicit
    source integration for a vendored C core. Neither currently builds a
    YM2151 dependency or focused test suite.

Upstream research baselines:

| Candidate | Reviewed Reference | Integration Implication |
| --- | --- | --- |
| Nuked-OPM | [Commit `f209e6ed`][nuked-commit], [C API][nuked-header] | Per-instance clock, register, reset, and decoded-output interface; source notices specify LGPL-2.1-or-later. Start the prototype here. |
| Modern MAME/ymfm | [MAME wrapper][mame-wrapper], [ymfm commit `81aec25c`][ymfm-commit] | MAME now wraps ymfm. Its [README][ymfm-readme] specifies C++14; it is not a drop-in for this project's C++11 build. |

The [OPM register map][opm-registers] distinguishes shared channel pitch and
modulation sensitivity from per-operator envelopes, detune, and multipliers.
Its [implementation][opm-source] documents four LFO waveforms and special
noise behavior on the final operator of channel 8. These differences inform
the proposed module below. No core has been vendored, compiled, benchmarked,
or audibly validated during planning.

## Scope And Ownership

Implement one new module with working display name and proposed stable
slug `YM2151`. Finalize the display name before panel/publication work;
freeze the slug and Rack parameter/port/light IDs before distributing
patches. Proposed paths are `src/YM2151.cpp`, `src/dsp/yamaha_ym2151/`,
`dep/Nuked-OPM/`, `manual/YM2151/`, `presets/YM2151/`, and
`patches/debug/YM2151.vcv`. These do not exist at planning time. Record any
agreed path/name changes here and update consumers together.

Own the chip adapter, host module, artwork, feature tests, presets, targeted
manual, registration, and issue follow-through. Coordinate dependency
provenance/public metadata with [001](archive/001-licensing-and-project-documentation.md),
build/test/package integration with [002](archive/002-build-tests-and-ci.md),
helper locations/branding with
[003](archive/003-source-organization-and-rack-integration.md),
manual conventions with [004](archive/004-manual-content-and-publication-style.md),
native panel captures with [005](archive/005-production-panel-captures-and-figures.md),
and global themes with [008](008-native-light-and-dark-themes.md).

This spec explicitly adds a new chip/module beyond the modernization plan.
It does not depend on [010](010-nuked-opn2-engines.md)'s selectable YM2612
engines, nor own [009](009-ym2612-ssg-retriggering.md)'s trigger fix. Reuse
proven harnesses and event conventions where useful without coupling the
cores or changing Boss Fight/Mini Boss behavior.

## Requirements

### Establish A Viable Core And Voice Model

1.  Prototype the pinned Nuked-OPM core with deterministic notes, envelopes,
    modulation, and noise before final panel construction. Record the exact
    vendored SHA, source URL, license texts, local patches, and build inputs.
    Preserve attribution and package dependency notices with source and
    binaries. Compile the C source explicitly and retain C++11 support for
    project code. Do not fetch an unpinned branch during a build.
2.  Measure one, four, and sixteen active Rack voices early. If this core
    cannot meet a documented real-time budget, evaluate a narrowly scoped
    alternative with traceable provenance, matching feature tests, and
    demonstrated toolchain compatibility. Record the decision and evidence
    here before implementation proceeds. Do not silently raise the project
    language standard or reduce the promised polyphony. Ship one core;
    an engine chooser is outside this request.
3.  Model one four-operator channel per Rack polyphonic lane, up to 16 lanes.
    Start with independent chip state per lane and use hardware channel 8
    (zero-based 7) so each lane can use its noise operator. Silence unused
    hardware channels. Treat this as a deliberate modular adaptation of
    an eight-channel chip, not an eight-part workstation inside every lane.
4.  Preserve independent LFO, noise, envelope, and register state per lane
    and module instance. Packing lanes into one chip would share global
    state and restrict noise; any optimization must preserve the stated
    behavior and pass isolation tests. Inactive lanes must not incur full
    continuous synthesis cost, and reactivation must not expose stale notes.

### Deliver A Focused Musical Interface

Provide four operator groups and a common pitch/gate/modulation section.
Use native ranges and document any inversion for intuitive level controls.
The minimum musical controls are:

| Area | Required Controls |
| --- | --- |
| Voice | Common V/oct and tune, gate, retrigger, eight algorithms, operator-1 feedback, output level. |
| Each operator | Attack, first decay, sustain rate, sustain level, release, total level, key-rate scaling, multiplier, DT1, DT2, and AM enable. |
| LFO | Frequency, saw/square/triangle/noise waveform, separate AM/PM depths, channel AM/PM sensitivities. |
| Noise | Enable and rate for the channel-8 final operator, with its envelope and level still effective. |
| Output | Polyphonic left/right outputs with hardware left/right/both routing; default both. No artificial stereo widening. |

Give numerical synthesis controls knobs and polyphonic CV where practical;
at minimum expose CV for algorithm, feedback, LFO frequency/depths, noise
rate, output level, and each operator's envelope rates/levels and multiplier.
Detune, scaling, waveform, AM-enable, and routing may use discrete controls
without dedicated CV. Record a complete control/port table with final
ranges, defaults, quantization, units, CV scale, and normalled behavior
before freezing the panel. Every visible control must affect the production
path; an advanced context option must have a clear label and saved state.

Use a common channel pitch with operator multipliers/detune. Do not inherit
Boss Fight's independent operator V/oct, per-operator PM sensitivity,
looping SSG control, or soft-reset extension merely by copying its layout.
Use the YM2151 envelope/key-on behavior and document rate-zero cases. Show
algorithm/operator numbering consistently with the core's register mapping.

Define an audible default for a connected gate and a silent gate-off state
after release. Include original presets demonstrating a simple FM tone,
a modulated sustained sound, and noise percussion, plus a minimal debug
patch. Do not require game ROMs, proprietary voice banks, or a video rip.

For example, a four-channel pitch/gate cable produces four independent
notes; a mono modulation CV applies to each lane. Changing one lane's noise
rate must not change another lane. With both output routes enabled, each
jack carries its own full-level signal; unplugging the right jack does not
double the left signal. A held-gate retrigger performs a defined key-off/
key-on sequence once rather than depending on a slow CV-divider boundary.

### Make Timing And Rack Integration Explicit

1.  Choose and document a fixed hardware master clock from a traceable
    reference. Derive the core clock/output cadence and V/oct-to-key-code/
    key-fraction mapping from that clock. Keep C4 at 0 V; quantify register
    pitch resolution and safe limits. Do not reuse the project's 768 kHz
    constant or YM2612 empirical tuning correction without derivation.
2.  Use fractional host/core scheduling and a documented resampling/filter
    path. Verify pitch, envelope timing, LFO, and noise across 44.1, 48, and
    96 kHz and active sample-rate changes. Respect the core's decoded DAC
    output and its update cadence; avoid arbitrary bit crushing or reading
    an internal operator sample in place of the chip output.
3.  Schedule register address/data writes according to the selected core's
    timing/busy contract. Bound queued work and memory. Preserve ordering
    for key masks, split pitch values, and AM/PM depth writes sharing one
    register. Coalesce only replaceable controls, never accepted key events;
    specify and test the supported event-rate/latency limit and overload
    policy rather than claiming unlimited bus bandwidth.
4.  Sample gate/retrigger inputs each host frame with the established
    2 V/0.01 V hysteresis. A sampled low followed by a one-frame high is
    recognized at every CV-divider offset. Simultaneous gate rise/retrigger
    keys on once; held-high retrigger does not repeat; gate fall takes
    precedence over retrigger and releases; retrigger while gate is low
    does not start a note. Register latency must be bounded and measured.
5.  Determine lane count from connected polyphonic inputs, minimum one,
    maximum 16; broadcast mono CV using Rack's polyphonic input semantics.
    Specify handling of shorter poly cables and disconnections. Clear
    removed lanes, initialize new ones safely, and match both output counts.
6.  Keep audio work bounded, with no new allocations, file access, logging,
    or locks in processing. Account for core-reset cost during lifecycle
    changes. Synchronize any UI/engine custom state and keep drawing off the
    audio thread. Define output gain/headroom from measured core range;
    verify finite bounded voltages and no startup/reset bursts.
7.  Add model declaration, registration, manifest tags/description/manual
    mapping, and assets together. Preserve all existing slugs, IDs, custom
    JSON, and presets. Verify reset, randomize, duplicate, save/load, and
    browser previews; handle missing/malformed custom JSON safely.

### Complete The Module's Presentation

Create an original readable panel using project controls and branding,
with algorithm guidance and unambiguous operator grouping. Support native
Rack light/dark panels according to 008, including null-module previews,
live theme changes, displays, and controls. Reuse its helper when available;
coordinate minimum-Rack metadata if this module lands first.

Write the manual with patch examples, CV/rate tables, chip versus modular
voice behavior, noise routing, pitch limits, output scaling, and measured
CPU tradeoffs. Add a production-rendered panel capture and control reference
using 005 when available. Integrate the manual into collection builds and
release-artifact checks; update README and add an unreleased changelog entry.
Reconcile the old `1.13.0 (TBD)` item without claiming that version shipped.

Extend enabled-model, theme, capture, and publication inventories when the
module is added. Relative to the planning baseline, it makes 19 manifest
entries, 17 enabled models, and 15 sound-module manuals. Keep both existing
blanks and any remaining disabled modules in their current categories. Derive
final counts from the actual manifest if other work lands first.

## Verification And Acceptance

Use deterministic register/event fixtures through the real adapter/module,
plus independent reference-core renders for synthesis comparisons. Test all
eight algorithms, operator order, feedback boundaries, envelope/rate-zero
cases, multiplier and detune extremes, four LFO waveforms, separate AM/PM
depth changes, AM enable, noise enable/rate, and stereo routing. Retain
fixture provenance and numeric/audio tolerances; listening to the issue's
video alone does not prove emulation correctness.

Exercise 1/4/16 lanes, two concurrent instances, mono-CV broadcast, uneven
input widths, lane shrink/grow, short pulses at all divider offsets, heavy
register traffic, reset, save/reload, and sample-rate changes. Measure CPU
and memory for 44.1/48/96 kHz at representative 64/256-frame workloads,
recording compiler flags, machine, worker settings, repeated-run median,
tail, and maximum observed time. Establish the target-machine callback
budget during the prototype and verify native Rack meets it at 16 voices.

- [ ] A pinned, attributed core and documented prototype decision establish
      accurate control mapping, C++11 integration, and feasible 16-voice cost.
- [ ] The new registered module implements the interface, isolated voices,
      event contract, clocking, noise, and outputs described above.
- [ ] Core/adapter and real-module tests pass with reference evidence;
      applicable existing DSP suites and supported-platform builds pass.
- [ ] Native Rack confirms four-voice playing, 16-voice stress, patch/preset
      reload, both themes, live switching, and browser preview safety.
- [ ] Panel, original presets/debug patch, manual/capture, README, changelog,
      dependency notices, and packaging/publication inventories are complete.
- [ ] Existing module behavior and patch compatibility remain intact, and
      disabled modules remain disabled.
- [ ] #79 has a verified resolution comment with accessible implementation
      commit links, is closed as completed, and its final state is recorded.

## Validation Commands

Run from the repository root with a prepared Rack 2 SDK/tree, matching
runtime, C/C++11 compiler, SCons, and pinned Catch2 v2. Current commands:

```shell
make -j2
scons -j2 test
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

Implement these proposed targets, or record exact equivalents from 002;
none exists at planning time. Wire dependency sources into standalone DSP
tests explicitly and exclude SDK-dependent tests from SCons discovery:

```shell
scons test/dsp/yamaha_ym2151/test_voice.cpp
make -C test/rack ym2151 RACK_DIR="$(pwd)/../.."
make -C test/rack benchmark-ym2151 RACK_DIR="$(pwd)/../.."
make -C manual/YM2151
```

Run the existing `make -C manual` collection target after integrating the
new manual. Inspect the generated PDF and production captures; legacy TeX
recipes can mask errors. Record exact native capture, package, platform-build, and
Rack-session commands/actions once their infrastructure exists. Missing
native access, reference results, or performance evidence leaves the
corresponding acceptance criteria pending.

## Issue Updates And Closure

The October 1, 2026 user request authorizes comments on #79 and closure
after verified resolution, without another confirmation. Re-read the issue
and comments before posting. Use updates for substantive prototype findings,
scope decisions, or verified results; avoid repetitive progress comments.

Commit the tested implementation and identify its full SHA and canonical
GitHub URL. Follow the authorized push/merge workflow; this spec does not
authorize pushing or releasing. Keep closure pending while required commits
are only local. After acceptance passes, post a resolution comment describing
the new module, selected core, musical scope, native/test/performance
evidence, limitations, and accessible implementation commit references.
Distinguish source availability from a VCV Library release. This planning
commit is not the implementation reference.

After the comment succeeds, close #79 with reason `completed` and read back
its state. Record the comment URL, fixing SHAs, date, and validation below.
Failed remote actions remain outstanding. Mark `COMPLETE` and archive under
[AGENTS.md](../AGENTS.md#planning-and-completion) only after all acceptance
and issue follow-through are satisfied.

Prepare a factual comment body file before using the authenticated CLI:

```shell
gh issue comment 79 --repo Kautenja/PotatoChips --body-file /tmp/potatochips-79-update.md
gh issue close 79 --repo Kautenja/PotatoChips --reason completed
gh issue view 79 --repo Kautenja/PotatoChips --json state,stateReason,comments,url
```

## Non-Goals

No YM2164/YM2413 module, eight-part sequencer/workstation, MIDI voice-bank
importer, ROM/sample dependency, CSM/timer/IRQ control panel, new Boss Fight
engine, or changes to declined features in #61/#84/#86/#94. Do not add
hardware-inaccurate looping envelopes or independent operator pitches to
this first module. No release/version selection or closure from planning.

## Completion Evidence

Planning: issue/comment, local registration/DSP/build/manual paths, and
upstream core candidates reviewed on 2026-10-01. Prototype, final core/
panel decisions, implementation, and executable/native verification pending.

| Issue | Implementation Commit | Verification | Resolution Comment | Final State |
| --- | --- | --- | --- | --- |
| #79 | Pending | Source review only; prototype/reference/native checks pending | Pending | Open at planning time |

[nuked-commit]: https://github.com/nukeykt/Nuked-OPM/commit/f209e6ed3712032b641d53ce8fb24824eae6adc3
[nuked-header]: https://github.com/nukeykt/Nuked-OPM/blob/f209e6ed3712032b641d53ce8fb24824eae6adc3/opm.h
[mame-wrapper]: https://github.com/mamedev/mame/blob/master/src/devices/sound/ymopm.cpp
[ymfm-commit]: https://github.com/aaronsgiles/ymfm/commit/81aec25ccbb98f4873a255f7551ac4dadac59b4a
[ymfm-readme]: https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/README.md
[opm-registers]: https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_opm.h
[opm-source]: https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_opm.cpp
