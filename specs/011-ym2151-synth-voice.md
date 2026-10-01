# Yamaha YM2151 Synth Voice

Created: 2026-10-01
Status: IN PROGRESS
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
and global themes with [008](archive/008-native-light-and-dark-themes.md).

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

- [x] A pinned, attributed core and documented prototype decision establish
      accurate control mapping, C++11 integration, and feasible 16-voice cost.
- [x] The new registered module implements the interface, isolated voices,
      event contract, clocking, noise, and outputs described above.
- [ ] Core/adapter and real-module tests pass with reference evidence;
      applicable existing DSP suites and supported-platform builds pass.
- [ ] Native Rack confirms four-voice playing, 16-voice stress, patch/preset
      reload, both themes, live switching, and browser preview safety.
- [x] Panel, original presets/debug patch, manual/capture, README, changelog,
      dependency notices, and packaging/publication inventories are complete.
- [x] Existing module behavior and patch compatibility remain intact, and
      disabled modules remain disabled.
- [ ] #79 has a verified resolution comment with accessible implementation
      commit links, is closed as completed, and its final state is recorded.

## Validation Commands

Run from the repository root with a prepared Rack 2 SDK/tree, matching
runtime, a C compiler, C++11 production/C++14 test compiler, Make, and the
vendored Catch2 v3.16.0. Current commands:

```shell
make -j2
make -j2 test test-rack
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

The originally proposed SCons commands are superseded by spec 002
Make-based targets. The implemented equivalents compile the dependency
explicitly and keep SDK-dependent tests separate:

```shell
make test/dsp/yamaha_ym2151/test_voice
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

Implemented locally on 2026-10-01. Keep this spec open for supported-platform
CI, the remaining interactive checks below, and public commit/issue closure.
The implementation is not a release or a claim that #79 is resolved remotely.

### Core Decision And Prototype

Selected ymfm at `81aec25ccbb98f4873a255f7551ac4dadac59b4a`, BSD-3-Clause.
Production includes only its OPM translation unit. Two mechanical C++11
changes replace `make_unique` and remove assertion-bearing `constexpr`;
all synthesis logic and tables are unchanged. Original/local checksums and
exact edits are in [provenance](../dep/ymfm/provenance.json). Nuked-OPM at
`f209e6ed3712032b641d53ce8fb24824eae6adc3` remains an unmodified C test
reference, never a production engine. Both license texts are packaged.

Target: Apple M1 Pro, macOS 26.6.2 arm64, Apple Clang 21.0.0,
production C++11, single processing thread. Chosen module budget is less
than 50% median callback time at 16 lanes, leaving at least half for host and
other modules. Record p99 and maximum separately; desktop scheduling can
produce outliers. No claim of a hard real-time OS guarantee.

The initial silent Nuked lower-bound probe already cost 183.71% of one
thread's budget at 16 independent chips. A repeated probe measured
12.23% / 47.43% / 181.26% at 1 / 4 / 16 chips. Active ymfm measured
0.36% / 1.36% / 5.43%, before host/resampling work. This establishes why
Nuked was rejected even before active-note costs; independent active-note
reference tests subsequently verify its decoded output. Prototype sources
are retained in `assets/011/prototype-*.cpp`.

```shell
clang -O3 -c dep/Nuked-OPM/opm.c -o /tmp/opm.o
clang++ -O3 -std=c++11 -Idep/Nuked-OPM specs/assets/011/prototype-nuked.cpp /tmp/opm.o -o /tmp/opm-benchmark
/tmp/opm-benchmark
clang++ -O3 -std=c++11 -pedantic-errors -Idep/ymfm specs/assets/011/prototype-ymfm.cpp dep/ymfm/ymfm_opm.cpp -o /tmp/ymfm-benchmark
/tmp/ymfm-benchmark
```

### Frozen Interface And Timing

Display name **Voice 2151**, slug `YM2151`, 60 HP. No existing module IDs
changed. The actual current inventory is 17 manifest/registered models,
15 sound manuals and two blanks; removed experimental modules stay removed.
The historical 19-entry planning count is superseded.

The complete ranges, defaults, units, quantization, +8 V full-range additive
CV scale and normalled behavior are in the new
[control reference](../manual/YM2151/sections/controls.tex). Frozen IDs:

| IDs | Meaning |
| --- | --- |
| Params 0-12 | Tune, algorithm, feedback, level, LFO frequency, AMD, PMD, AMS, PMS, waveform, noise enable, noise rate, route |
| Params 13 + 11n through 23 + 11n | Operator n+1: AR, D1, SR, SL, RR, level, multiplier, KS, DT1, DT2, AM enable; n=0..3 |
| Inputs 0-9 | V/oct, gate, retrigger, algorithm, feedback, LFO frequency, AMD, PMD, noise rate, output level |
| Inputs 10 + 7n through 16 + 7n | Operator n+1: AR, D1, SR, SL, RR, level, multiplier CV |
| Outputs 0-1 | Left, right; independently full-level, both routed by default |

Master clock 3,579,545 Hz is traced to ymfm's OPM phase-table derivation in
`ymfm_fm.ipp`; native rate is clock/64. Register KC zero corresponds to C#0;
C4 is 47 semitones above it, KC 0x3e / KF 0. Fraction resolution is 1/64
semitone. Clamp to C#0 through C#8 minus 1/64 semitone. The FIR is a causal
32-tap Blackman low pass with cutoff min(20 kHz, 0.4 host rate), followed by
fractional linear resampling. Filter group delay is 15.5 native samples;
output gain is 10/32768 V per decoded PCM unit times LEVEL, bounded at +/-10 V.
The measured full-level single carrier is about 2.5 V peak before LEVEL.

One write transaction per native tick honors ymfm's 64-master-clock busy
contract. Pitch code/fraction are snapshotted as a two-write transaction;
AM/PM depth uses separate pending slots for the shared physical register.
A fixed 256-slot control queue coalesces each replaceable register. A
64-entry key FIFO preserves every accepted event. Rising/retrigger events
reserve one release slot; overload rejects the whole new event and counts
it. Sustained 1 kHz retrigger input is supported; unlimited audio-rate bus
traffic is not. At steady state the tested retrigger reached both key writes
within three 48 kHz frames despite control traffic. First notes wait for
initial configuration; the worst initial control set is under 0.7 ms.
The full FIFO drains within about 1.15 ms plus initial configuration.

Gates and retriggers sample every host frame at 2 V / 0.01 V hysteresis,
with the required coincidence/fall precedence. Mono CV broadcasts; missing
lanes on a short poly cable are zero; the widest input sets 1-16 lanes.
Removed lanes restore a constructor-prepared snapshot, including LFO/noise,
without allocation. Rate changes preserve chip state. UI reads only an
atomic algorithm index; all saved state uses Rack's parameter serializer.
The audio callback does not allocate, log, lock or access files. Constructor
allocations prepare ymfm channels/operators and reset snapshots.

### Validation Run

-   `make -j2`: passed, C++11 plugin on local Rack 2.6 headers/library.
-   `make -j2 test test-rack`: all local suites passed, including unchanged
    existing-module parameter/audio fixtures and concurrent spec 009 tests.
-   `make test/dsp/yamaha_ym2151/test_voice`: passed, eight cases including
    all algorithms, AM/PM and four waveforms, noise, detune/multiplier
    extremes, zero attack, envelope behavior, clocking, reset, bus overload
    and lane isolation. Independent pinned Nuked renders compare sine pitch
    within 0.3%, sine RMS within 3%, algorithm RMS within 10%, and filtered
    modulation/noise/envelope RMS within 10% or 0.002 normalized units.
    The reference noise stream is filtered offline using the documented FIR
    before comparison; comparing raw DAC noise against filtered output would
    conflate the reconstruction filter with core behavior. These are numeric
    feature checks, not a bit-identical-core claim.
-   `make test/dsp/yamaha_ym2151/test_voice TEST_MODE=asan-ubsan CXX=clang++`:
    passed. Adapter and production ymfm are instrumented; the independent
    unmodified Nuked C reference is compiled ordinarily.
-   `make -C test/rack ym2151 RACK_DIR="$(pwd)/../.."`: passed. Every divider
    offset at 44.1/48/96 kHz, gate/retrigger precedence, mono broadcast,
    uneven poly cables, shrink/grow, reset, all three preset reloads,
    standard JSON duplicate/restore, randomize and concurrent instances.
-   `make -C test/rack benchmark-ym2151 RACK_DIR="$(pwd)/../.."`: passed.
    [Raw callback measurements](assets/011/benchmark.csv) contain 300
    repeats per 1/4/16-lane, 64/256-frame, 44.1/48/96 kHz workload after
    2,048 warmup frames. Compiler uses Rack's `-O3` and
    `-funsafe-math-optimizations`; one thread, no audio device or I/O.
    16-lane medians are about 10-12% of callback duration. Owned module
    storage including chip heap payload is 165,368 bytes, excluding Rack
    vectors, allocator bookkeeping and UI assets; it does not grow per frame.
-   Native `tools/capture` renderer: live and null-module light/dark panels,
    live preference switching and context recreation passed. Reviewed both
    themes; fixed label overlaps, then exported the production dark capture
    and geometry guide. Original panel/algorithm sources and approved
    geometry are retained in `assets/011/`.
-   Isolated official Rack Free 2.4.0 profiles loaded the packaged module
    plus 8vert, LFO, Merge and Scope. Four distinct pitch lanes rendered on
    Scope. A second dark-theme session showed Merge=16, active Scope traces
    and approximately 14.3% average / 18.6% maximum total Rack CPU in the
    observed frame at 48 kHz, one worker. No audio device was selected.
    Rack Pro 2.6.3 loaded the patch but its fresh profile required activation;
    no credentials were copied or entered. Temporary processes were closed.
-   `make -C manual/YM2151` and `make -C manual`: passed, 15 manuals. All nine
    new-manual pages rendered and reviewed; operator table moved to its own
    page to avoid a stranded final row. `scripts/validate.py manuals` passed
    with the bundled Poppler directory on PATH.
-   `python3 scripts/validate.py dependencies`: passed pinned hashes/notices.
-   `make -j2 dist` and `python3 scripts/validate.py package
    dist/KautenjaDSP-PotatoChips-2.1.0-mac-arm64.vcvplugin`: passed.
-   `make check-build`: passed after adding the new production translation
    unit to the existing disposable build fixture. Manifest JSON and
    `git diff --check`: passed.

Exact capture/export commands (native renderer needs desktop access):

```shell
make -C tools/capture capture MODULE=YM2151
python3 tools/capture/export_screenshots.py tools/capture/.build/captures manual --module YM2151
python3 tools/capture/draw_panels.py tools/capture/.build/captures --module YM2151
```

### Remaining Acceptance And Issue Follow-Through

Linux x64 and Windows x64 builds are not run locally; existing CI must verify
them after an authorized push. Interactive preset-menu reload, reset and
randomize/duplicate gestures were covered through the real-module harness,
not a complete GUI gesture tour. Audible audio-device listening remains
unperformed; native Scope confirms synthesis, not listening. No full hardware
capture or bit-exact reference claim is made.

The issue was re-read and remains open. The tested implementation is to be
committed locally; no push or release is authorized by this spec. Therefore
public fixing-commit links, the final resolution comment and issue closure
remain pending. Do not mark COMPLETE or archive until these remaining
acceptance items are satisfied.

| Issue | Implementation Commit | Verification | Resolution Comment | Final State |
| --- | --- | --- | --- | --- |
| #79 | Local implementation commit pending | macOS core/module/reference/native/package/manual checks passed; cross-platform and listening pending | Progress update pending; closure pending publication | Open, re-read 2026-10-01 |

[nuked-commit]: https://github.com/nukeykt/Nuked-OPM/commit/f209e6ed3712032b641d53ce8fb24824eae6adc3
[nuked-header]: https://github.com/nukeykt/Nuked-OPM/blob/f209e6ed3712032b641d53ce8fb24824eae6adc3/opm.h
[mame-wrapper]: https://github.com/mamedev/mame/blob/master/src/devices/sound/ymopm.cpp
[ymfm-commit]: https://github.com/aaronsgiles/ymfm/commit/81aec25ccbb98f4873a255f7551ac4dadac59b4a
[ymfm-readme]: https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/README.md
[opm-registers]: https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_opm.h
[opm-source]: https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_opm.cpp
