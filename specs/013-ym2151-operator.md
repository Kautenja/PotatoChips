# Yamaha YM2151 Operator

Build Operator 2151 as a compact polyphonic FM building block with external
audio-rate phase modulation and native OPM envelopes, feedback, detune, and
LFO behavior. Users can patch independently tuned operators into their own
networks, including mixed 2151/2612 patches.

Created: 2026-10-01
Status: PLANNED
Issue: None assigned; this is separate from the completed Voice 2151 #79.

## Goal And Behavior Examples

The module's value is external operator patching, supported by the 2151's
modulation controls. Merely hiding three operators from Voice 2151 does not
satisfy this spec. Use the display name `Operator 2151` and new stable slug
`YM2151Operator`; preserve every existing module's identity and behavior.

-   With no cables, the default patch produces a sustained sine. Connect a
    pitch sequence and gate to obtain a playable, enveloped single operator.
-   Connect one Operator 2151 audio output to another's PM input. Raise the
    carrier's PM amount to create FM timbres; independently change the
    modulator's pitch, multiplier, and decay to control spectrum over time.
-   Use three or four operators for branching, summed, or feedback networks
    beyond the full voice's eight fixed algorithms. External mixers provide
    summing; cable feedback includes Rack and resampling delay.
-   Patch Operator 2612 into Operator 2151, or the reverse. Both remain useful
    independently; cross-patching does not promise identical PM calibration.
-   Apply 16-channel pitch and gate cables with a mono PM cable. Each lane
    has independent synthesis state while receiving the same modulator.
-   Use the OPM LFO's noise waveform for irregular pitch or level modulation.
    This is distinct from the audio noise generator, which is out of scope.

## Existing Code And Ownership

Read [spec 011](archive/011-ym2151-synth-voice.md),
[Voice 2151](../src/YM2151.cpp), its
[adapter](../src/dsp/yamaha_ym2151/voice.hpp), and
[Operator 2612](../src/MiniBoss.cpp) before implementation.
The current adapter clocks a whole chip, schedules register writes, and
filters stereo output. It has no external operator PM input. Operator 2612's
[feedback operator](../src/dsp/yamaha_ym2612/feedback_operator.hpp) illustrates
external PM and internal feedback, but its envelope/core behavior must not
be copied over the OPM semantics.

Use the existing pinned [ymfm core](../dep/ymfm/provenance.json), including
its documented C++11 compatibility changes. Its
[operator API](../dep/ymfm/ymfm_fm.h) exposes phase, envelope clocking and
volume calculation; its [channel implementation](../dep/ymfm/ymfm_fm.ipp)
provides the feedback and operator ordering reference. These are starting
points for a prototype, not proof that isolated clocking already works.
Reuse the pinned Nuked-OPM reference where its hardware interface permits.
Do not add a second production engine or update dependency revisions.

Keep reusable DSP under `src/dsp/yamaha_ym2151/`, independent of Rack/UI.
Prefer a first-party adapter over imported-code edits. Any necessary narrow
ymfm extension must preserve attribution, document its reason, update local
provenance hashes, and pass unchanged Voice 2151 regression fixtures.
Do not make per-sample writes through undocumented memory layouts or use
preprocessor access-control overrides.

This work is independent of [010](010-nuked-opn2-engines.md). Reuse the build,
manual, capture, theme, and lettering conventions established by archived
specs 001-005, 008, 011, and 012. No existing compatibility contract is relaxed.

## Prototype Before Panel Work

Prove an isolated operator with external PM before freezing panel geometry.
Retain reproducible prototype commands, measurements, and the chosen design
under `specs/assets/013/` and in this spec's completion evidence.

1.  Exercise the pinned core's operator API with native phase/envelope/LFO
    clocks and native operator-1 feedback. Compare zero-external-PM output
    against a complete chip configured for algorithm 7 with only operator 1
    audible. Include key-on/off, feedback 0-7, detune, and LFO modulation.
2.  Inject a signed external phase offset at each native operator evaluation.
    Keep it separate from pitch-register updates and internal LFO PM. Verify
    that zero PM preserves the reference and negative PM reverses polarity.
3.  Establish host-to-native PM conversion and native-to-host audio conversion
    at 44.1, 48, and 96 kHz. Measure frequency response, aliasing and latency
    for one module and two/four-module serial chains. Evaluate the existing
    FIR against a shorter-delay reconstruction approach; do not inherit it
    without measuring repeated filtering in chains.
4.  Benchmark one and four instances, each at 1/4/16 lanes, using 64/256-frame
    blocks at all three rates. Include silent, sustained, maximum internal
    feedback, external PM, and connected-chain workloads. Record compiler,
    flags, CPU, OS, worker count, warmup, repetitions, memory, median, p95,
    and worst observed callback time as a fraction of the audio deadline.

On the same documented machine, a single 16-lane instance must cost no more
than Voice 2151 at matching rate/block conditions. The four-instance,
16-lane chain must have median callback cost below 50% of the deadline and
p95 below 75% with one worker. Use at least 300 measured repetitions after
2,048 warmup frames. Timing thresholds are a prototype gate, not brittle
shared-CI assertions or a guarantee for every computer.

Added signal-path delay must be at most 1 ms per module and 4 ms for a
four-module chain at each tested rate; include input conversion, output
filtering, and actual Rack cable scheduling in the chain measurement.
Measure conversion delay with a known signal and disclose any frequency
dependence; do not infer latency from gate attack alone. Internal feedback
must retain native timing and must not traverse the output reconstruction
filter. External cable feedback is intentionally a different signal path.

Freeze reconstruction coefficients, PM input bandwidth, output calibration,
and numerical reference tolerances from these measurements before panel
implementation. Record rejected options and the tradeoff. If a gate cannot
be met without altering native behavior or the stated musical scope, report
the result and revise the design explicitly; do not silently weaken it or
ship a control-rate substitute for audio-rate PM.

## Synthesis And Signal Contract

### Native Behavior

-   Model one OPM operator with operator-1 feedback per polyphonic lane.
    Preserve native sine quantization, envelope rates, key scaling, multiplier,
    DT1/DT2, feedback levels and LFO behavior.
-   Use the Voice 2151 clock of 3,579,545 Hz and native evaluation rate
    clock/64. Host-rate changes preserve musical pitch and envelope timing.
-   Use 0 V = C4 and the existing 1/64-semitone pitch mapping, clamped from
    C#0 through C#8 minus 1/64 semitone before applying native multiplier and
    detune. Do not add arbitrary continuous tuning inside the chip algorithm.
-   Give each lane independent phase, envelope, feedback history and LFO
    state. Rate changes preserve state; resetting the module restores the
    documented deterministic startup state.
-   Retain native key-on phase/envelope semantics. No click-prevention mode,
    hard-sync input, artificial envelope loop, or SSG-EG mode is introduced.

### External PM

Label the input and attenuverter `PM` and describe it as phase modulation.
Do not call it a linear-Hz or exponential pitch input. Evaluate PM every host
sample, including samples between control-divider ticks, then convert it to
the native evaluation timeline with the prototype's documented method.

At amount +1, +5 V means +1 cycle of phase offset; -5 V means -1 cycle.
The amount ranges from -1 to +1 with default 0. Clamp external input to
[-10, +10] V before scaling, treat non-finite samples as zero, and wrap phase
with defined arithmetic. Perform signed scaling without overflow or undefined
shifts. A constant offset of one cycle is periodic; it is the changing offset
that creates modulation. Document this in the manual and test it directly.

Sum the scaled external offset with native feedback at the phase evaluation
point without feeding the final output gain into internal feedback. Turning
PM amount to zero or disconnecting PM must recover the unmodulated path,
without stale modulation history. No hidden slew limiter may turn audio PM
into slow CV. Filtering required for sample-rate conversion must have an
explicit measured bandwidth and latency.

### Gates, Polyphony And Output

Use 1-16 lanes determined by the widest input. Broadcast mono cables, and
use zero for missing lanes on a shorter poly cable. With Gate disconnected,
the operator sustains; a connected Gate controls its envelope. Removing a
Gate cable returns to sustained mode. Retrigger while sustained or gate-high
performs an ordered native key-off/key-on; retrigger while gate-low does not
sound a note. A simultaneous gate rise/retrigger creates one attack, and a
simultaneous gate fall/retrigger releases. Use the shared 2 V / 0.01 V gate
hysteresis and sample both event inputs every host frame.

Keep event delivery bounded, ordered, and independent of divided CV scans.
If native scheduling requires a queue, document capacity/overload behavior,
reserve release capacity, and test overload without a stuck gate. Reset removed
lanes so shrink/grow cannot restore old feedback, envelopes or LFO history.

Provide one mono-per-lane audio output, nominally up to +/-5 V at full Level,
with a final finite safety bound of +/-10 V. Freeze the exact native-sample
scale in the prototype and test it; do not normalize individual notes or
feedback settings. Polyphonic lanes are separate channels, never summed.

## Controls And Defaults

Use native integer settings and meaningful tooltips. The envelope's TL and
SL labels must explain attenuation versus loudness; higher native values
mean quieter output. The Level control is a separate post-synthesis gain.

| Control | Range / Default | CV |
| --- | --- | --- |
| Tune | -4 to +4 octaves / 0 | Dedicated 1 V/oct input |
| PM amount | -1 to +1 / 0 | Dedicated audio-rate PM input |
| Multiplier | Native 0-15 (0 means 0.5x) / 1 | Yes |
| DT1 | Native 0-7 / 0 | Yes |
| DT2 | Native 0-3 / 0 | Yes |
| Feedback | Native 0-7 / 0 | Yes |
| Attack rate | 0-31 / 31 | Yes |
| Total level | 0-127 attenuation / 0 | Yes |
| Decay rate | 0-31 / 0 | Yes |
| Sustain level | 0-15 attenuation / 0 | Yes |
| Sustain rate | 0-31 / 0 | Yes |
| Release rate | 0-15 / 15 | Yes |
| Key rate scaling | 0-3 / 0 | No |
| LFO rate | 0-255 / 0 | Yes |
| LFO waveform | Saw, square, triangle, noise / saw | No |
| AM depth | 0-127 / 0 | Yes |
| PM depth | 0-127 / 0 | Yes |
| AM sensitivity | 0-3 / 0 | No |
| PM sensitivity | 0-7 / 0 | No |
| AM enable | Off/on / off | No |
| Level | 0-1 / 1 | Yes |

Add Gate and Retrigger inputs. Integer-control CV is additive: +10 V adds
one full parameter span, negative voltages subtract; clamp then round to the
nearest native value. Level CV follows the same additive full-span rule.
Non-finite CV contributes zero. Scan these controls at most every 16 host
frames, refreshing immediately for a new lane. This divider excludes PM and
key events. Test exact endpoints and half-step rounding. Waveform names and
native DT1/DT2 intervals belong in tooltips and the manual.

Freeze parameter/port/light IDs, defaults and JSON before creating presets.
Use Rack parameter serialization unless genuinely additional state is needed;
any extra state must be versioned and validated. Reset, duplicate, randomize,
preset loading and reload must not crash, leave stale state, or exceed bounds.

## Presentation And Integration

Target a readable 24-30 HP panel, materially smaller than Voice 2151's 60 HP.
Use grouped envelope, tuning/feedback, LFO and connection areas, native Rack
light/dark preference, shared controls, outlined titles and the established
footer. Verify labels at normal Rack zoom and with cables present. Choose
and record final width after a layout check; do not shrink controls to force
an arbitrary width. Create original vector assets using existing tooling.

Proposed production paths are `src/YM2151Operator.cpp`,
`src/dsp/yamaha_ym2151/operator.hpp`, `res/YM2151Operator.svg`,
`res/YM2151Operator-dark.svg`, `presets/YM2151Operator/`,
`patches/debug/YM2151Operator.vcv`, and `manual/YM2151Operator/`.
Add model registration, manifest metadata, build inputs, dependency/package
validation, capture inventories, and focused test targets. Update README,
changelog, contributor/manual inventories and relevant licensing scope only
when the implementation exists. At the current baseline, the addition means
18 registered models, 16 sound-module manuals and two blanks; derive actual
counts again if other work lands first.

Provide three original presets: clean sustained operator, percussive feedback,
and internally modulated sustain. Supply a debug/example patch demonstrating
two-operator PM, four-operator chaining, mixed 2151/2612 modulation, and
polyphony. Clearly label required companion modules. The manual needs a
native dark capture, vector panel guide, voltage/parameter reference, internal
versus cable-feedback explanation, and practical patch instructions.

## Validation And Acceptance

Use focused DSP tests for native-reference behavior, phase wrapping, positive
and negative PM, zero/integer-cycle DC offsets, detune/multiplier extremes,
AM/PM depth independence, every LFO waveform, envelope timing, reset and
sample-rate changes. Use analytic sine-PM expectations for low-index spectra
and independent high-rate offline fixtures for conversion/aliasing checks.
Freeze fixture provenance, tolerances and the treatment of native chip aliases
before judging the candidate; do not generate expected data through the same
adapter under test. Nuked cannot directly validate an external PM extension;
limit its role to hardware-representable unmodulated/internal-modulation cases.

Rack tests must exercise every control-divider offset, gate/retrigger
precedence, PM impulses between divider ticks, disconnected inputs, all poly
broadcast rules, shrink/grow, malformed/non-finite inputs, serialization,
all presets, reset/randomize/duplicate, multiple instances and rate changes.
Audit and test absence of callback allocation, locks, I/O and unbounded work.
Shared UI data must follow existing thread-safe conventions.

- [ ] Prototype passes reference, PM, latency and CPU gates; decisions and
      exact reproduction commands are recorded before panel completion.
- [ ] Native operator behavior and the external PM contract pass focused DSP
      tests at 44.1/48/96 kHz, including instrumented adapter/core execution.
- [ ] Real-module integration, event, polyphony and saved-state tests pass;
      existing Voice 2151 and Operator/Voice 2612 behavior remains unchanged.
- [ ] Linux x64, macOS arm64 and Windows x64 builds/tests/packages pass through
      the existing supported-platform workflow or equivalent recorded runs.
- [ ] Native Rack sessions verify light/dark previews, live theme changes,
      cable interaction, preset reload, duplication, reset, and 16 lanes.
- [ ] Audible listening covers basic sine, envelope transients, deep PM,
      two/four-module chains and mixed 2612 patches at the three host rates;
      compare observations with spectra and measured delay, not Scope alone.
- [ ] Original assets, presets, debug patch, manual and all affected inventories
      agree; package notices and resources validate; PDF pages are reviewed.
- [ ] Completion evidence records dates, revisions, commands, numeric results,
      manual observations and limitations before archival under AGENTS.md.

### Exact Commands

Run from the repository root. Existing build prerequisites are in
[CONTRIBUTING.md](../CONTRIBUTING.md); the commands below assume the prepared
Rack tree at `../..`, a supported compiler, Clang for sanitizers, and the
manual/capture dependencies documented in their respective directories.

These existing aggregate commands must include the new module after wiring:

```shell
make -j2 all test test-rack RACK_DIR="$(pwd)/../.."
make check-build
python3 scripts/validate.py dependencies
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

The following focused aliases and paths are proposed deliverables, not
currently available commands. Add them to the existing Make-based harness;
keep all benchmarks opt-in and ordinary tests deterministic.

```shell
make test/dsp/yamaha_ym2151/test_operator
make test/dsp/yamaha_ym2151/test_operator TEST_MODE=asan-ubsan CXX=clang++
make -C test/rack ym2151-operator RACK_DIR="$(pwd)/../.."
make -C test/rack benchmark-ym2151-operator RACK_DIR="$(pwd)/../.."
make -C tools/capture capture MODULE=YM2151Operator
python3 tools/capture/export_screenshots.py tools/capture/.build/captures manual --module YM2151Operator
python3 tools/capture/draw_panels.py tools/capture/.build/captures --module YM2151Operator
make -C manual/YM2151Operator
make -C manual
python3 scripts/validate.py manuals manual/.build
make -j2 dist RACK_DIR="$(pwd)/../.."
```

Validate each actual package, substituting its platform-specific filename:

```shell
python3 scripts/validate.py package dist/KautenjaDSP-PotatoChips-<version>-<os>-<cpu>.vcvplugin
```

Record the exact substituted command, artifact and result. Add prototype and
fixture-generation commands here when their sources exist. Do not claim
native audio, platform coverage or acceptable performance from a build alone.

## Non-Goals And Publication

No audio noise mode, SSG/looping envelope, new waveform bank, hard sync,
through-zero linear FM, MIDI import, algorithm selector, LFO output, stereo
panning, expander protocol, alternate core, or changes to existing modules'
sound/patch formats. Arbitrary external networks are modular extensions;
there is no claim that they reproduce a complete hardware YM2151 channel
bit-for-bit or eliminate cable delay.

Writing or implementing this spec does not authorize issue creation, comments,
reopening #79, commits, pushes, release publication or VCV Library submission.
Follow subsequent explicit user instructions for those actions. This spec
remains PLANNED until implementation begins and has no implementation or
validation results yet.
