# Yamaha YM2414 Voice And Operator

Add an OPZ four-operator voice and a single OPZ operator, preserving the
chip's waveform, frequency and envelope behavior while exposing external
phase modulation only in the modular operator.

Created: 2026-10-01
Status: PLANNED
Planning baseline: `8d5264ea`.
Issue: None assigned.

## Goal And Behavior Examples

Use display names `Voice 2414` and `Operator 2414` with new stable slugs
`YM2414` and `YM2414Operator`.

-   Build a four-operator bass with different native waveforms and fine
    frequency settings, using the chip's eight connection algorithms.
-   Hold a modulator at a fixed frequency while the carrier tracks V/oct.
    The changing frequency relationship changes the spectrum across notes.
-   Patch Operator 2414 into another chip's operator. Its waveform and
    envelope remain OPZ-specific while the cable defines the connection.
-   Change one polyphonic lane's modulation without changing another lane's
    LFO phase, envelope or feedback history.

## Evidence And Ownership

The [Yamaha TX81Z manual][tx81z] documents eight operator waveforms and
ratio/fixed frequency operation. It describes an instrument built around
OPZ, not every raw chip register. The [ymfm OPZ header][opz-header] and
[implementation][opz-source] expose additional chip behavior, including
multiplexed writes and two LFO banks; some fields are explicitly uncertain.
Do not equate a TX81Z preset parameter with a raw chip field without a
verified mapping, or advertise a complete TX81Z emulator. In the reviewed
source, channel volume is still unimplemented and output routing is described
as a guess. Fixed-frequency code exists despite stale introductory comments
saying otherwise. Inspect executable paths and verify them experimentally;
listing YM2414 as supported is not proof of a complete faithful backend.

Read [Voice 2151](../src/YM2151.cpp), its
[adapter](../src/dsp/yamaha_ym2151/voice.hpp),
[013](013-ym2151-operator.md), and the
[existing ymfm provenance](../dep/ymfm/provenance.json). This spec owns both
OPZ modules and their artifacts. Reuse the existing clock/queue/testing
patterns where appropriate; OPM register layout and pitch mapping are not
proof of OPZ behavior.

Prototype ymfm OPZ as the production candidate. Verify availability at the
existing pinned revision before importing `ymfm_opz.h/.cpp`; otherwise
record a narrowly scoped revision decision and compatibility evidence. Keep
C++11 production support, original notices, source hashes, local patch
records and packaged dependency texts. Prefer first-party adapters to core
edits. A shared-core change must preserve existing Voice 2151 fixtures.

## Voice 2414: Native Four-Operator Channel

One Rack lane represents one independently clocked OPZ channel context with
four operators, the eight native algorithms, and native operator-1 feedback.
Use common channel V/oct, Gate and Retrigger inputs. Operator ratio/fixed
selection controls pitch as the chip permits; do not add four independent
V/oct or gate inputs to the voice. Keep native operator evaluation order,
quantization, envelope progression, key scaling and feedback delays.

All eight native waveforms must be selectable per operator. Preserve the
actual frequency tables and fine steps rather than substituting continuous
floating-point ratios. In fixed mode, an operator's oscillator frequency
does not track channel pitch. Determine separately whether channel key code
still affects its envelope/key scaling; freeze this from source and reference
fixtures, not from an assumption that fixed mode ignores all note data.

Expose both native LFO banks with their rate, waveform, AM/PM depth and sync
controls, and channel sensitivities. Preserve per-operator AM enable and
shared-versus-local state. Bank selection and shared-address writes must not
overwrite the other bank. Provide native envelope shift and reverb-rate
behavior when verified; reverb-rate is an envelope function, not an inserted
reverb effect. Preserve documented DT1/DT2 semantics in ratio/fixed modes.

The prototype must resolve the chip's output controls, channel volume and
noise scope. Expose verified channel-appropriate audio noise as a saved Voice
option if the selected native channel supports it; otherwise fix it off and
record the channel restriction explicitly. Do not borrow OPM channel-8 noise
behavior merely because similar fields appear in a header. Chip timers, CT
pins, undocumented preset loading and unused test bits are not front-panel
features. All fidelity claims must name the channel configuration covered.

Provide Left and Right polyphonic outputs with verified native routing.
Describe any raw routing asymmetry; do not invent continuous chip panning.
Keep a separate host Level control after chip channel volume. Default to a
sine carrier and silent modulators in a native algorithm, feedback off,
ratio mode, multiplier 1, fine offset zero and AM/PM depths zero. Gate is
low when disconnected, so a new Voice is silent until triggered.

## Operator 2414: Modular OPZ Operator

Expose one native operator with a feedback path corresponding to the chip's
feedback-source operator, all eight waveforms, ratio/fixed frequency controls,
native envelope/key scaling and verified OPZ LFO controls. The sole Audio
output is mono per polyphonic lane. This is an extraction of the chip's
operator math and clocks, not a generic FM oscillator with an OPZ label.

Use [013's external PM contract](013-ym2151-operator.md#external-pm):
amount -1 to +1/default 0; at +1, +5 V is +1 phase cycle; input clamps to
+/-10 V and non-finite input contributes zero. Evaluate every host frame,
convert onto the native timeline, wrap with defined signed arithmetic, and
sum with native feedback at phase evaluation. Post-synthesis Level must not
change internal feedback. No modulation input may silently become divided
pitch-register updates. Fixed frequency does not disable external PM.

With Gate disconnected, sustain the native operator; connected Gate controls
its envelope. Start with a clean sine, fastest attack, no decay, minimum
attenuation and feedback/modulation off. Retrigger in sustained mode or with
Gate high performs ordered native key-off/key-on. No audio noise, invented
SSG envelope, hard sync or arbitrary waveform editor in the Operator.

## Controls And Prototype Decisions

Use native register ranges and tooltips showing physical meaning. The table
is the proposed control inventory; derive exact frequency displays and any
uncertain raw-field interpretation before freezing the implementation.

| Group | Required Controls |
| --- | --- |
| Channel | Tune, eight-way algorithm (Voice), feedback 0-7, native channel volume/routing (Voice), host Level |
| Each operator | Waveform 0-7; ratio/fixed mode; multiplier/coarse, fine, fixed range/frequency; DT1/DT2 with mode-specific meanings |
| Envelope | AR/D1R/D2R 0-31, RR 0-15, SL 0-15 and TL 0-127 attenuation, key rate scaling 0-3, AM enable |
| Extended envelope | Verified envelope shift and reverb-rate settings, named as envelope behavior |
| Each LFO bank | Native rate, four waveforms, separate AM/PM depths, sync setting and channel AM/PM sensitivities |
| Modular operator | Audio-rate PM and attenuverter, V/oct, Gate, Retrigger, Audio |

Provide CV for waveform, multiplier/coarse, fine, TL, AR/D1R/D2R/SL/RR,
feedback, LFO rate/depth, Level and Voice algorithm. Ratio/fixed and extended
settings can be saved switches/menu controls; preserve each mode's values.
Fixed-frequency controls use their own saved values rather than reinterpreting
a ratio knob silently. Tune spans -4 to +4 octaves/default 0. Level spans
0-1/default 1. Additive control CV uses +10 V for a full parameter span,
clamps then rounds native integers, and treats non-finite CV as zero.
Ordinary control scans are at most 16 host frames apart, refreshed for new
lanes. PM and gate/retrigger events are read every frame.

Before panel implementation, record all native field mappings, raw ranges,
defaults, update order, port/parameter IDs, units and inactive-control behavior
in this spec. Resolve the header's uncertain fine, envelope, routing and
preset-related comments against the implementation and instrument/hardware
fixtures. Unresolved behavior must remain an explicit limitation or block
its control; an inert knob or guessed claim is not acceptance evidence.

Select and document a hardware-supported master clock and its provenance;
derive operator/envelope/LFO clocks, ratio/fixed conversion and register
bandwidth from that clock. Unlike OPM, do not copy a clock value or tuning
formula merely because the core shares infrastructure. Map 0 V to C4,
quantize/clamp at native pitch limits, and measure error including fixed
frequency extrema. Preserve musical timing and state across 44.1/48/96 kHz.

Every lane has independent synthesis state. The widest input sets 1-16 lanes;
mono broadcasts, missing lanes of shorter poly inputs read zero, removed
lanes reset, and all outputs share the lane count. Gates use 2 V/0.01 V
hysteresis; coincident rise/retrigger attacks once, fall/retrigger releases,
and retrigger with a connected low Gate cannot start a note. Never copy
phase-reset or retrigger semantics from another Yamaha family.

Multiplexed register addresses need distinct desired-state storage for their
logical fields. Preserve atomic/order-sensitive writes and accepted key
edges in bounded preallocated queues; coalesce only replaceable controls,
reserve release capacity and document overload behavior and worst latency.
Freeze fixed voltage calibration, nominal single-carrier output near +/-5 V,
finite +/-10 V safety bounds, conversion bandwidth and latency. No dynamic
note normalization, callback allocation, locks, file I/O, logging or drawing.

## Verification And Acceptance

Retain prototype commands, decisions and fixture provenance under
`specs/assets/015/`. Zero-external-PM Operator output must match an isolated
native feedback-source operator with the remaining carriers muted. Compare
all eight algorithms/waveforms, ratio and fixed settings, frequency extremes,
feedback, envelope shift/reverb, LFO banks and routing. Instrument-level
fixtures must document the translation into registers, firmware/clock,
gain and analog-output treatment; a TX81Z recording alone is not a bit-exact
chip reference. Unmodified ymfm comparisons verify adapter integration;
independent hardware or independently derived fixtures establish stronger
claims. Freeze tolerances before judging results.

Exercise PM sign/depth/wrapping, DC cycle offsets, impulses between CV scans,
native envelope transients and two/four-operator external networks. Measure
converter aliases separately from native chip aliases, and include actual
Rack cable scheduling in latency tests. Added delay must be at most 1 ms
per Operator and 4 ms for a four-module chain.

Benchmark both modules at 1/4/16 lanes, 44.1/48/96 kHz and 64/256-frame
blocks, with two concurrent instances and dense modulation. Record CPU/OS,
compiler flags, one-worker configuration, memory, median, p95 and maximum
across at least 300 repetitions after 2,048 warmup frames. On the recorded
target machine, a 16-lane Voice and, separately, four connected 16-lane
Operators must each use less than 50% median and 75% p95 callback time.
Do not silently reduce polyphony or replace native behavior to meet a gate.

- [ ] Prototype resolves native clock, frequency tables, multiplexed writes,
      extended envelopes, both LFO banks and channel output/noise scope.
- [ ] Voice and Operator pass native-reference and external-PM tests with
      explicit provenance, tolerances and limits on fidelity claims.
- [ ] CPU, event latency, conversion quality, memory and chain delay meet
      recorded prototype gates before panel completion.
- [ ] Rack tests cover all divider offsets, mode changes, non-finite inputs,
      malformed JSON, reset/randomize/duplicate, presets, rates and lanes.
- [ ] Existing modules retain audio and saved-patch behavior; supported
      Linux x64, macOS arm64 and Windows x64 builds/tests/packages pass.
- [ ] Native sessions verify 16 lanes, fixed/ratio sounds, PM chains, audible
      output, both themes, live switching, null previews and cable access.
- [ ] Original assets, manuals, captures, presets, debug patches, license
      records and inventory changes are complete and visually reviewed.

## Integration And Validation Commands

Proposed paths: `src/YM2414.cpp`, `src/YM2414Operator.cpp`,
`src/dsp/yamaha_ym2414/`, light/dark `res/<slug>` panels,
`presets/<slug>/`, `patches/debug/<slug>.vcv`, and `manual/<slug>/`.
Create original Voice presets covering ratio bass, fixed-frequency modulation
and multiple waveforms; Operator presets cover clean, percussive and modulated
states. Do not bundle factory voice banks. Include mixed 2151/2612/2414
examples with required companions clearly identified.

Use established native themes, title outlines, manuals and production
captures. Validate readable normal-zoom control grouping before final width;
keep the Operator substantially smaller than the Voice. Freeze IDs before
presets. Use Rack parameter serialization, version/validate extra state and
provide synchronized UI/engine handoffs. Add registrations, manifest entries,
README/changelog and model/test/manual/capture/package inventories together
when implemented; preserve existing slugs and formats.

Run from the repository root with [CONTRIBUTING.md](../CONTRIBUTING.md)'s
prerequisites, Rack at `../..`, Clang for instrumentation, and the documented
manual/capture tools. Existing aggregate commands:

```shell
make -j2 all test test-rack RACK_DIR="$(pwd)/../.."
make check-build
python3 scripts/validate.py dependencies
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

Implement these proposed focused targets in the existing harness; they do
not exist merely because this spec names them:

```shell
make test/dsp/yamaha_ym2414/test_voice test/dsp/yamaha_ym2414/test_operator
make test/dsp/yamaha_ym2414/test_voice test/dsp/yamaha_ym2414/test_operator TEST_MODE=asan-ubsan CXX=clang++
make -C test/rack ym2414 ym2414-operator RACK_DIR="$(pwd)/../.."
make -C test/rack benchmark-ym2414 RACK_DIR="$(pwd)/../.."
for module in YM2414 YM2414Operator; do
    make -C tools/capture capture MODULE="$module"
    python3 tools/capture/export_screenshots.py tools/capture/.build/captures manual --module "$module"
    python3 tools/capture/draw_panels.py tools/capture/.build/captures --module "$module"
    make -C "manual/$module"
done
make -C manual
python3 scripts/validate.py manuals manual/.build
make -j2 dist RACK_DIR="$(pwd)/../.."
```

Run `python3 scripts/validate.py package` with each actual generated package
path and record the exact command/result. Add prototype/reference commands
and evidence here; archive only after verified acceptance under AGENTS.md.

## Non-Goals And Planning Evidence

No complete TX81Z/DX11 firmware, MIDI/SysEx importer, factory preset bank,
sequencer, host reverb, arbitrary waveform editor, whole eight-channel chip
front panel, timer/IRQ/CT interface, existing-module redesign or release.
Native channel context duplicated for Rack polyphony is an intentional host
adaptation; external PM is a separate extension with measured cable delay.

Research and repository review occurred on 2026-10-01. No OPZ prototype,
reference render, build, performance measurement or listening test has run.
This remains PLANNED. The current user request authorizes committing and
pushing the spec, not implementing modules, messaging issues or releasing.

Planning-document checks on 2026-10-01: relative links/anchors, shell-block
syntax, unique spec numbers, planned-status metadata and whitespace passed.
Implementation commands above are future acceptance work, not results from
this documentation change.

[tx81z]: https://usa.yamaha.com/files/download/other_assets/9/316769/TX81ZE.pdf
[opz-header]: https://github.com/aaronsgiles/ymfm/blob/main/src/ymfm_opz.h
[opz-source]: https://github.com/aaronsgiles/ymfm/blob/main/src/ymfm_opz.cpp
