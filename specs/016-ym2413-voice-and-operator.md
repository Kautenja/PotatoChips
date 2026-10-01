# Yamaha YM2413 Voice And Operator

Add an OPLL voice with its native instrument bank, editable user instrument
and rhythm mode, plus a single-operator module that exposes OPLL synthesis
for external modular connections.

Created: 2026-10-01
Status: PLANNED
Planning baseline: `8d5264ea`.
Issue: None assigned.

## Goal And Behavior Examples

Use display names `Voice 2413` and `Operator 2413`, with new stable slugs
`YM2413` and `YM2413Operator`.

-   Sequence the Voice's instrument selection through the native melodic
    sounds, then choose instrument 0 and edit its two operators.
-   Play a chord with one user instrument. Every lane receives that patch;
    the module does not claim multiple user instruments on one physical chip.
-   Switch to Rhythm mode and trigger the five native drum sounds, retaining
    the pitch/noise interactions between their shared channels.
-   Patch two Operators together for external FM and use their native
    waveform, envelope and scaling controls to depart from the preset bank.

## Evidence And Ownership

The [Yamaha application manual][manual] and [register datasheet][datasheet]
describe the melodic instrument selector, user instrument and rhythm bank.
The [ymfm OPLL implementation][opl] provides a candidate operator model,
including the envelope depress stage and separate melody/rhythm output
paths. [emu2413][reference] is a candidate independent comparison engine.
Pin all implementation/reference revisions before generating fixtures.

Read [Voice 2151](../src/YM2151.cpp), the existing
[Operator 2612](../src/MiniBoss.cpp), and
[013](013-ym2151-operator.md). Reuse integration and modular conventions,
not their chip-specific envelopes, multiplier tables or feedback timing.
This spec owns both OPLL modules and their tests/assets. Coordinate the
shared OPL-family import with [014](014-ymf262-voice-and-operator.md) so it
has one provenance record and one production translation unit.

Prefer the OPLL implementation in the existing
[ymfm revision](../dep/ymfm/provenance.json), after verifying the required
files and behavior at that revision. Record any necessary revision change,
C++11 compatibility edits, original/local hashes and attribution; test
existing OPM behavior and any landed OPL3 module. Audit provenance of the
instrument data separately from source code, and record which measured or
reconstructed YM2413 bank is selected. Do not silently substitute VRC7,
YM2423, YMF281 or a different YM2413 revision's timbres.

## Voice 2413: Native Melodic Channel And Rhythm Bank

Each Rack lane owns an independent OPLL context. Melody mode exposes one
native two-operator channel with fixed modulator-to-carrier topology. Rhythm
mode exposes the chip's coupled percussion cluster. This is a playable voice,
not a nine-channel MIDI workstation; duplication of chip-global state across
Rack lanes is an explicit polyphonic host adaptation.

Melody mode has an instrument selector 0-15: one user instrument plus the
15 native preset instruments. The preset bank is immutable. Editable operator
controls write only the shared user-instrument registers, and only instrument
0 uses them. Keep edits when changing away from 0 and make inactive controls
visibly inactive. Do not make factory sounds secretly editable or copy a
preset into the user bank without an explicit user action. A copy-to-user
button is outside the initial scope.

Provide common V/oct, Gate, Retrigger, four-bit channel Volume attenuation,
and the native channel Sustain switch. Preserve the actual meaning of that
switch, including release behavior; it is distinct from operator envelope
type and sustain level. Instrument CV is discrete, not waveform morphing.
Switching instruments while held follows the native register behavior and
does not force an artificial retrigger or crossfade inside the chip.

User-instrument editing must retain asymmetric roles: modulator TL is a
six-bit field, carrier amplitude uses channel Volume, and feedback belongs
to the modulator. Both operators retain native waveform, multiplier, envelope,
key scaling and AM/vibrato flags. There is no algorithm selector, continuous
pan, arbitrary detune, variable LFO rate or generic four-stage ADSR replacement.

Rhythm mode provides Bass Drum, Snare, Tom, Cymbal and Hi-Hat gate inputs,
their native level fields, and pitch controls/CV for the three participating
channels. Keep fixed rhythm patch data, shared noise and phase relationships,
and register sharing. Label the effects of changing a channel pitch on more
than one drum. The melodic Gate is inactive in this mode, and unconnected
drum gates remain low. Do not model the cluster as independent samples.

Provide polyphonic Melody and Rhythm outputs corresponding to the native
output paths; these are not left/right stereo channels. Default to Melody,
instrument 0, a sine carrier with effectively muted modulator, minimum channel
attenuation, modulation off and Gate low when unpatched. Level is a separate
post-synthesis gain. Document external mixing when both paths are needed.

Mode selection is saved and has no CV input. A change releases old keys and
applies the destination's retained settings/current gates using a bounded,
documented transition. Old rhythm triggers must not leak into melodic mode
or vice versa. Keep custom-instrument settings through mode changes/reset
according to frozen Rack defaults, with no hidden mutable global bank.

## Operator 2413: One Native Operator

Expose one OPLL operator with selectable Modulator or Carrier role, default
Modulator. The role preserves chip-specific key-off/envelope behavior,
attenuation resolution and feedback availability. Modulator exposes native
TL and feedback; Carrier uses the native channel-volume path and has no
internal feedback. Keep inactive controls disabled and their saved settings
retained. The role change is a saved, non-CV choice with a bounded key reset.

Both roles expose the two native waveforms, multiplier, AR/DR/SL/RR,
envelope type, key scale level/rate and AM/vibrato flags. Tap the selected
operator's native output rather than passing it through a second audible
operator. Preserve OPLL envelope depress/retrigger timing and quantization.
Zero-feedback Modulator and Carrier need not have identical envelope behavior.

Use [013's PM convention](013-ym2151-operator.md#external-pm): amount
-1 to +1/default 0, +5 V gives +1 phase cycle at amount +1, external voltage
clamps to +/-10 V and non-finite samples contribute zero. Evaluate each host
frame and convert to native evaluation time; combine with native feedback
where the selected role allows it. Use defined signed phase arithmetic.
Level is outside internal feedback. External PM is available in both roles
and is expressly a modular extension, not an OPLL register feature.

Provide V/oct, Gate, Retrigger, PM and one polyphonic Audio output. With no
Gate cable the default clean sine sustains; connected low Gate releases.
No preset selector, rhythm noise, arbitrary waveforms, hard sync or added
looping envelope in the Operator. Native tremolo/vibrato rates and depths
stay fixed; do not copy OPL3's selectable-depth controls onto OPLL.

## Controls, Clocking And Host Contract

| Control | Native Range / Default For The Operator |
| --- | --- |
| Waveform | Sine/rectified native waveform, 0-1 / 0 |
| Multiplier | 0-15 / 1; display the actual native multiplier table |
| Modulator total level | 0-63 attenuation / 0 |
| Carrier volume | 0-15 attenuation / 0 |
| Feedback | 0-7 / 0, Modulator only |
| AR, DR, SL, RR | Each 0-15 / 15, 0, 0, 15 |
| Envelope type, key scale rate | Native binary settings / sustained, 0 |
| Key scale level | 0-3 / 0 |
| AM and vibrato | Off/on independently / off |
| Channel Sustain | Off/on / off; preserve applicable role semantics |
| Tune, Level | -4 to +4 octaves / 0; post-gain 0-1 / 1 |

Expose CV for waveform, multiplier, applicable attenuation, AR/DR/SL/RR,
feedback and Level, plus Voice instrument selection and channel Volume.
Other switches/scaling can be saved controls without CV. Additive CV uses
+10 V for a full parameter span; clamp then round native integer values.
Non-finite CV contributes zero. Ordinary controls scan at most every 16 host
frames; event inputs and external PM are sampled every frame. Freeze all
IDs, defaults, role/mode dependencies and exact voltage mappings before
panel work and presets.

Use a nominal 3,579,545 Hz master clock and verify the native clock/72
relationship against the selected core. Derive block/F-number mapping with
0 V = C4, native quantization and documented pitch clamps. Do not alter chip
frequency tables to force continuous equal temperament. Preserve operator,
envelope, LFO and noise timing across host rates. Choose and measure the
output reconstruction separately from the chip's DAC/quantization model;
an isolated operator tap is not an emulated physical DAC pin.

The widest input selects 1-16 lanes; mono broadcasts and missing channels
of shorter poly cables read zero. Outputs share the same width, lane removal
clears state, and newly active lanes receive current controls immediately.
Each lane has independent bank selection, phase, envelopes, LFO/noise and
feedback; the single editable patch is a module parameter set, with per-lane
CV realized through independent contexts.

Use 2 V/0.01 V gate hysteresis. A coincident gate-rise/retrigger attacks once,
gate-fall/retrigger releases, and a retrigger with connected Gate low cannot
start a note. Sustain-without-cable applies only to the Operator. Preserve
native key transitions and depress stages. Bound bus scheduling and overload,
coalesce only replaceable controls, reserve release capacity and measure
latency without discarding accepted events.

Define fixed native-output scaling, nominal single-carrier output near
+/-5 V, and finite +/-10 V safety bounds. No dynamic normalization or invented
saturation before the chip output stage. No processing-thread allocation,
locks, file access, logging, drawing or unbounded work. Synchronize any UI
state explicitly; custom JSON is versioned, validated and safely defaulted.

## Prototype And Acceptance

Retain fixtures, reference revisions and exact generation commands under
`specs/assets/016/`. Compare all preset instruments, user-register boundaries,
channel Sustain, both waveforms, native multiplier values, role-dependent
envelopes/depress stages and all coupled percussion combinations. Verify
preset switching while held and custom edits while a preset is selected.
Use unmodified pinned-core integration comparisons plus independent emu2413
or hardware fixtures; document bank/clock differences and freeze tolerances
before evaluating results. Never label same-core agreement independent proof.

For Operators, compare each role's native envelope and phase/output tap with
an independently instrumented native reference, then test external PM using
analytic/independent offline fixtures. Include sign, DC cycle offsets,
wrapping, between-divider impulses, feedback, two/four-module chains and
conversion aliases. Measure chip aliases separately from resampling errors.

Benchmark both modules at 1/4/16 lanes, 44.1/48/96 kHz, 64/256-frame blocks,
all modes and two concurrent instances. Record CPU/OS/compiler/worker count,
memory and at least 300 repetitions after 2,048 warmup frames. Require a
16-lane Voice and, separately, four connected 16-lane Operators to use below
50% median and 75% p95 of a one-worker callback on the recorded machine.
Report maximum too. Added delay is at most 1 ms per Operator and 4 ms for
four in series, including actual Rack scheduling. These are prototype gates,
not universal performance promises or shared-CI timing assertions.

- [ ] Bank provenance, core/reference pins and native control/clock mappings
      are documented; required notices and C++11 builds are reproducible.
- [ ] Voice presets/custom patch/rhythm behavior and both Operator roles
      pass native comparisons and event/envelope edge cases.
- [ ] External PM, conversion quality, latency, CPU and memory meet the
      stated gates without replacing native behavior.
- [ ] Rack tests cover all divider offsets, role/mode/instrument changes,
      reset, rates, lanes, malformed inputs/JSON and preset/duplicate reload.
- [ ] Existing module regressions and Linux x64/macOS arm64/Windows x64
      builds, tests and package validation pass.
- [ ] Native Rack verifies 16 lanes, audible preset/custom/drum/PM examples,
      light/dark panels, live theme changes, previews and cable access.
- [ ] Both manuals, original presets/debug patches, panels/captures and all
      inventories agree; PDF pages and package notices are reviewed.

## Integration And Validation Commands

Proposed paths: `src/YM2413.cpp`, `src/YM2413Operator.cpp`,
`src/dsp/yamaha_ym2413/`, paired light/dark `res/<slug>` panels,
`presets/<slug>/`, `patches/debug/<slug>.vcv` and `manual/<slug>/`.
Provide original custom-instrument/rhythm Voice examples and clean/percussive/
externally modulated Operator examples. Factory timbre selection may be used
in example patches, with the native instrument-bank provenance retained.

Follow established themes, title outlines, production captures and manual
builds. Keep the Operator materially smaller than the Voice without shrinking
labels below normal-zoom readability. Document chip-global duplication,
custom-instrument restrictions, drum coupling, attenuation, output paths,
modulation rate and cable delay. Freeze IDs before presets; update registration,
manifest, README/changelog and test/manual/capture/package inventories only
with implementation. Preserve all existing slugs, IDs and patch behavior.

From the repository root, with [CONTRIBUTING.md](../CONTRIBUTING.md)'s build
prerequisites, Rack at `../..`, Clang and the manual/capture dependencies,
run the existing aggregate commands after integration:

```shell
make -j2 all test test-rack RACK_DIR="$(pwd)/../.."
make check-build
python3 scripts/validate.py dependencies
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

The following targets/paths are proposed implementation deliverables:

```shell
make test/dsp/yamaha_ym2413/test_voice test/dsp/yamaha_ym2413/test_operator
make test/dsp/yamaha_ym2413/test_voice test/dsp/yamaha_ym2413/test_operator TEST_MODE=asan-ubsan CXX=clang++
make -C test/rack ym2413 ym2413-operator RACK_DIR="$(pwd)/../.."
make -C test/rack benchmark-ym2413 RACK_DIR="$(pwd)/../.."
for module in YM2413 YM2413Operator; do
    make -C tools/capture capture MODULE="$module"
    python3 tools/capture/export_screenshots.py tools/capture/.build/captures manual --module "$module"
    python3 tools/capture/draw_panels.py tools/capture/.build/captures --module "$module"
    make -C "manual/$module"
done
make -C manual
python3 scripts/validate.py manuals manual/.build
make -j2 dist RACK_DIR="$(pwd)/../.."
```

Run `python3 scripts/validate.py package` with each generated artifact's
actual path and record the invocation/result. Add prototype and fixture
commands and completion evidence here before archival under AGENTS.md.

## Non-Goals And Planning Evidence

No VRC7/other-OPLL variants, MIDI sequencer, imported game music, generic
editable factory bank, arbitrary voice routing, extra LFO parameters,
full nine-part workstation, existing-module changes or release selection.

On 2026-10-01 the repository and primary chip/core sources were reviewed.
No new DSP, prototype, native session, build or listening test has run;
acceptance remains pending and status is PLANNED. Committing and pushing
these planning documents is authorized by the current request. Issue
messages, module implementation and publication are separate work.

Planning-document checks on 2026-10-01: relative links/anchors, shell-block
syntax, unique spec numbers, planned-status metadata and whitespace passed.
Implementation commands above are future acceptance work, not results from
this documentation change.

[manual]: https://map.grauw.nl/resources/sound/yamaha_ym2413.pdf
[datasheet]: https://www.bitsavers.org/components/yamaha/YM2413_199606.pdf
[opl]: https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_opl.h
[reference]: https://github.com/digital-sound-antiques/emu2413
