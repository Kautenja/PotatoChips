# Yamaha YMF262 Voice And Operator

Add an OPL3 voice that preserves native channel connections and rhythm
behavior, and a single operator for externally patched phase modulation.
These are two new modules, not replacements for the existing Yamaha voices.

Created: 2026-10-01
Status: PLANNED
Planning baseline: `8d5264ea`.
Issue: None assigned.

## Goal And Behavior Examples

Use display names `Voice 262` and `Operator 262`, with new stable slugs
`YMF262` and `YMF262Operator`. The shortened display names follow Voice 2612;
tooltips, metadata and manuals must identify the full YMF262 chip number.

-   Play a polyphonic two-operator bass, then select four-operator mode and
    explore its native connection choices and eight operator waveforms.
-   Select Rhythm mode and patch five gates to the native percussion bank.
    Changing shared channel pitches affects the linked drums as on OPL3.
-   Patch Operator 262 into another operator's PM input. Change waveform,
    multiplier and decay independently to build a modular FM instrument.
-   Compare internal feedback with a cable feedback loop. The internal path
    keeps chip timing; the cable path includes host and conversion delay.

## Evidence And Ownership

The [Yamaha datasheet][datasheet] describes OPL3's waveform selection,
channel pairing and output architecture. The [ymfm register implementation][opl]
provides the operator/register mapping. Use those together, checking diagrams
against register behavior rather than transferring OPM/OPN algorithms.
[Nuked-OPL3][nuked] is a candidate independent test reference.

Locally, [Voice 2151](../src/YM2151.cpp) and its
[adapter](../src/dsp/yamaha_ym2151/voice.hpp) establish native clocking and
per-lane integration; [Operator 2612](../src/MiniBoss.cpp) establishes the
existing modular use case. [013](013-ym2151-operator.md) defines the proposed
external PM convention and chain measurements. This spec owns both OPL3
modules, their DSP, presentation, tests and documentation.

Prefer extending the existing BSD-licensed [ymfm import](../dep/ymfm/provenance.json)
with the OPL translation unit and required dependencies from the same pinned
revision. Verify those files exist and support the required features before
importing. Do not silently upgrade the shared core; record any necessary
revision decision and prove unchanged OPM regressions. Preserve C++11
production support, attribution, upstream/local checksums and package notices.

## Voice 262: Native Synthesis Unit

One Rack lane owns one independent OPL3 synthesis context. It exposes a
channel pair in melodic mode or the native percussion cluster in Rhythm
mode; it is not an eighteen-part MIDI workstation. Chip-global LFO/noise
state is shared within that context and independent between Rack lanes.

| Mode | Required Behavior |
| --- | --- |
| Two operators | One native channel with its two connection choices; only the first two operator groups are active. |
| Four operators | One legal native channel pair, four native connection choices, and the chip's key, frequency, feedback and output-routing dependencies. |
| Rhythm | Native channels 6-8 provide bass drum, snare, tom, cymbal and hi-hat, preserving shared phase/noise and pitch dependencies. |

Default to Two operators, serial connection, sine waveforms and no feedback.
Use a common V/oct, Gate and Retrigger for the melodic modes. Do not add
independent operator pitches or key controls to the voice. Four-operator
mode must respect paired-channel register ownership, including any ignored
fields. Do not expose an eight-algorithm OPM selector under an OPL3 label.

Rhythm mode has five named gate inputs and three channel-pitch controls/CV
inputs, plus the native per-operator sound controls. Unconnected drum gates
are low. The melodic Gate does not trigger every drum. Label shared controls
and preserve the native rhythm equations; do not approximate five drums as
five independent sine/noise oscillators. No artificial decay or sample bank.

Expose all four native output buses as polyphonic A, B, C and D outputs,
with the native binary routing bits. Default audible melodic routing to A+B.
There is no continuous pan invented inside the chip and no automatic sum
when an output is unplugged. Document a simple external stereo-mix patch.

Mode is a saved menu/selector choice, not audio-rate CV. On a change, silence
the old unit with a bounded transition, initialize the destination from its
retained controls, and apply current gates once. Release inactive keys and
clear obsolete feedback state. Freeze the exact transition duration and
register sequence during the prototype; switching must never create stuck
notes. Preserve inactive-mode settings in patches.

## Operator 262: Modular Synthesis Unit

Expose one melodic OPL3 operator with native modulator feedback, its waveform,
envelope, multiplier, key scaling, tremolo and vibrato. Take audio directly
from that operator before channel routing and final host gain. External PM
is a documented extension at the native phase-evaluation point. It must not
be implemented as pitch CV, a generic sine oscillator, or silent extra
operators whose envelopes change the result.

Use the external PM voltage, polarity, finite-input, phase-wrap, gate and
chain contract in [013](013-ym2151-operator.md#external-pm): at amount +1,
+5 V is +1 cycle, amount is -1 to +1/default 0, input clamps to +/-10 V,
and non-finite input contributes zero. Evaluate every host sample, convert
to native evaluation time, and combine with native feedback without sending
post-gain audio through the internal feedback history. Zero PM must recover
the native single-operator reference. No rhythm/noise mode in this module.

A disconnected Gate means sustained key-on; a connected low Gate releases.
Expose Gate, Retrigger, V/oct, PM and mono-per-lane Audio. Default to a clean,
sustained sine. Do not add arbitrary LFO speed: OPL3's tremolo/vibrato clocks
and their two native depth settings are part of its identity.

## Control And Timing Contract

Both modules retain native integer operator fields. The following are raw
register ranges, not continuous ADSR times or linear loudness scales:

| Control | Range / Initial Operator Setting |
| --- | --- |
| Waveform | 0-7 / 0; show sourced waveform names or diagrams |
| Multiplier | 0-15 / 1; display the native nonuniform multiplier table |
| Total level | 0-63 attenuation / 0 |
| Attack, decay, release | Each 0-15 / 15, 0, 15 |
| Sustain level | 0-15 attenuation / 0; preserve the special final step |
| Envelope type | Native sustained/percussive / sustained |
| Key scale level, key scale rate | 0-3 and 0-1 / 0 |
| Tremolo and vibrato enable | Off/on independently / off |
| Feedback | 0-7 / 0; only at the native feedback source |
| Tremolo and vibrato depth | Each native shallow/deep / shallow |
| Tune, Level | -4 to +4 octaves / 0; post-synthesis 0-1 / 1 |

Provide CV for waveform, multiplier, TL, AR, DR, SL, RR, feedback and Level;
provide algorithm CV in melodic Voice modes. Switches/key scaling may use
saved controls without dedicated CV. Additive parameter CV uses +10 V for
one full span, clamps then rounds to the nearest integer, with non-finite
CV contributing zero. Scan at most every 16 host frames; PM and key events
always run each frame. Record every remaining default, control ID, port ID,
range and mode dependency before presets or panel geometry are frozen.

Use a 14,318,180 Hz nominal master clock and derive the native rate as
clock/288; verify that clock relationship against the selected core. Map
0 V to C4 using native block/F-number quantization. Document the realizable
pitch range and error, clamp before register conversion, and retain native
multiplier and key-scaling behavior. Host sample-rate changes must not change
pitch, envelope time or modulation speed. Reconstruction must preserve
chip-generated harmonics while controlling additional conversion aliases.

All inputs determine 1-16 lanes. Broadcast mono input; missing channels of
shorter poly cables read zero. Keep independent phase, envelopes, LFO/noise
and feedback per lane. Reset removed lanes and immediately configure new
ones. Voice melodic Gate is low when disconnected; operator Gate sustains.
Use 2 V/0.01 V hysteresis, one attack for coincident gate-rise/retrigger,
release precedence for gate-fall/retrigger, and no retriggered note while a
connected Gate is low. Test one-frame pulses at every CV-divider offset.

Respect native register timing with bounded preallocated scheduling. Coalesce
only replaceable controls, preserve accepted key events, reserve key-off
capacity, and document capacity, latency and overload behavior. Do not copy
the OPM adapter's bus delay without deriving OPL3's requirements.

Freeze fixed native-output-to-voltage scaling with nominal single-carrier
output near +/-5 V and final finite +/-10 V safety bounds. No per-note
normalization. Preserve native summing/clipping before host Level. No audio
thread allocation, locks, logging, drawing, file access or unbounded work.

## Prototype And Acceptance

Prototype both modules before final panel work. Retain scripts/fixtures under
`specs/assets/014/`, with exact commands and reviewed core/reference commits.
Compare native output with an unmodified pinned core; use independent
Nuked-OPL3 or hardware evidence for algorithm, envelope and rhythm claims.
A same-core comparison validates integration, not independent accuracy.

Exercise every waveform, melodic connection, feedback boundary, multiplier,
key-scaling mode, envelope edge/rate zero, LFO depth and rhythm interaction.
Test all four output routes and live mode transitions. For external PM,
measure positive/negative modulation, integer-cycle offsets, between-divider
impulses, spectra, conversion aliases and two/four-module chains. Freeze
numeric tolerances before judging results; separate native chip aliases from
resampler artifacts.

At 44.1/48/96 kHz and 64/256-frame blocks, benchmark 1/4/16 lanes and two
concurrent instances in every mode. On a documented target machine, require
16-lane Voice median below 50% and p95 below 75% of one worker's callback
budget. Require the same bounds for four connected 16-lane Operators combined.
Use at least 300 repetitions after 2,048 warmup frames; report memory, median,
p95 and maximum. Added delay must be at most 1 ms per Operator and 4 ms for
four in series, including actual Rack scheduling. These are prototype gates,
not universal hardware claims or timing assertions on shared CI.

- [ ] Pinned production/reference cores, licenses and C++11 build integration
      are recorded; existing OPM/OPN regression output stays unchanged.
- [ ] Both modules pass the native behavior, event, polyphony and PM contracts;
      all Voice modes and four buses have deterministic fixtures.
- [ ] Prototype establishes conversion quality, bounded event latency, CPU,
      memory and operator-chain delay within the stated budgets.
- [ ] Real-module tests cover malformed inputs/state, mode changes, reset,
      randomize, duplicate, preset reload, rate changes and lane shrink/grow.
- [ ] Linux x64, macOS arm64 and Windows x64 builds/tests/packages pass.
- [ ] Native Rack verifies 16 lanes, audible melodic/drum/PM examples, both
      themes, live theme changes, null previews and cable access.
- [ ] Both manuals, original presets, panels, captures, debug patches and
      inventories are complete, with reviewed PDF pages and package notices.

## Integration And Validation Commands

Proposed paths are `src/YMF262.cpp`, `src/YMF262Operator.cpp`,
`src/dsp/yamaha_ymf262/`, paired `res/<slug>.svg`/`<slug>-dark.svg`,
`presets/<slug>/`, `patches/debug/<slug>.vcv` and `manual/<slug>/`.
Provide original two-op/four-op/rhythm Voice presets and clean/percussive/PM
Operator presets. Manuals must distinguish native scope from modular PM and
explain shared drum controls, buses, CV rates, output scale and chain delay.
Follow the existing theme, title, capture and manual systems. Freeze a usable
panel width after checking labels and cables at normal zoom; the Operator
must be substantially smaller than the Voice.

Add both registrations and manifest entries only during implementation.
Update enabled-model/test/capture/package/manual inventories, README and the
unreleased changelog together. Preserve every existing slug, ID and JSON
meaning. Shared custom UI state needs an explicit thread-safe handoff;
version and validate additional patch state, with safe unknown-value defaults.

Run from the repository root with the prerequisites in
[CONTRIBUTING.md](../CONTRIBUTING.md), a prepared Rack tree at `../..`, Clang
for sanitizers, and the existing manual/capture dependencies. Current
aggregate commands, to include both modules once implemented:

```shell
make -j2 all test test-rack RACK_DIR="$(pwd)/../.."
make check-build
python3 scripts/validate.py dependencies
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

These focused targets and paths are proposed deliverables, not existing
commands. Add them to the existing harness without a new build system:

```shell
make test/dsp/yamaha_ymf262/test_voice test/dsp/yamaha_ymf262/test_operator
make test/dsp/yamaha_ymf262/test_voice test/dsp/yamaha_ymf262/test_operator TEST_MODE=asan-ubsan CXX=clang++
make -C test/rack ymf262 ymf262-operator RACK_DIR="$(pwd)/../.."
make -C test/rack benchmark-ymf262 RACK_DIR="$(pwd)/../.."
for module in YMF262 YMF262Operator; do
    make -C tools/capture capture MODULE="$module"
    python3 tools/capture/export_screenshots.py tools/capture/.build/captures manual --module "$module"
    python3 tools/capture/draw_panels.py tools/capture/.build/captures --module "$module"
    make -C "manual/$module"
done
make -C manual
python3 scripts/validate.py manuals manual/.build
make -j2 dist RACK_DIR="$(pwd)/../.."
```

Validate each actual artifact using `python3 scripts/validate.py package`
followed by its generated `.vcvplugin` path; record the exact invocation.
Record prototype/fixture commands, test results and manual observations here
before marking COMPLETE and archiving under AGENTS.md.

## Non-Goals And Planning Evidence

No full sound-card emulator, MIDI bank importer, OPL4 PCM, separate OPL2
compatibility engine, arbitrary voice algorithms, hard sync, artificial
looping envelopes, existing-module changes, or release/version selection.

On 2026-10-01, repository integration and upstream chip sources were reviewed
for this plan. No DSP prototype, hardware comparison, build or audio check
has run for these modules. All acceptance criteria remain pending. The user
authorized committing and pushing these planning documents; that does not
implement the modules or authorize releases or issue messages.

Planning-document checks on 2026-10-01: relative links/anchors, shell-block
syntax, unique spec numbers, planned-status metadata and whitespace passed.
Implementation commands above are future acceptance work, not results from
this documentation change.

[datasheet]: https://www.bitsavers.org/components/yamaha/YMF262_199110.pdf
[opl]: https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_opl.h
[nuked]: https://github.com/nukeykt/Nuked-OPL3
