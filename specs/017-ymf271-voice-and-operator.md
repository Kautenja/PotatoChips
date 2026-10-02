# Yamaha YMF271 Voice And Operator

Add an OPX voice based on a native four-slot group and a single-slot
operator for modular patching. Preserve native FM connections, sample
playback and slot modulation instead of approximating OPX with an OPM voice.

Created: 2026-10-01
Status: PLANNED
Planning baseline: `8d5264ea`.
Issue: None assigned.

## Goal And Behavior Examples

Use display names `Voice 271` and `Operator 271`, with new stable slugs
`YMF271` and `YMF271Operator`. Metadata and manuals spell out YMF271/OPX.

-   Play a four-operator voice using the native algorithms, feedback paths
    and internal waveforms, with different native LFO settings per slot.
-   Use the group's three-operator-plus-PCM mode to layer a synthesized body
    with an original sampled transient. The group keeps native key/frequency
    synchronization rules instead of treating every slot as independent.
-   Select paired two-operator mode or four independent PCM slots, with
    appropriate group-local inputs and native output routing.
-   Patch one Operator 271 into another's PM input and compare its native
    internal waveform with an explicitly supported sample-oscillator path.
    External PM remains an extension even when the sound source is native.

## Evidence, Risk And Ownership

The [Yamaha datasheet][datasheet] describes 48 slots, four-slot grouping,
FM/PCM modes, four outputs, 8/12-bit samples and a nominal 44.1 kHz output
rate at 16.9344 MHz. PCM capability is restricted to groups 0, 4 and 8;
using one of those groups is necessary for the planned hybrid voice.

The current [MAME OPX implementation][mame-source] and
[header][mame-header] provide a candidate implementation, but document
unverified internal waveforms and gaps including PFM and alternate looping.
These are research limitations, not proof that the hardware lacks those
features. [ymfm][ymfm] currently lists OPX as unimplemented; it is not an
available OPX backend simply because the project already uses ymfm for OPM.

This spec owns both OPX modules, sample handling, prototype research,
reference fixtures and all presentation/integration work. Read
[Voice 2151](../src/YM2151.cpp),
[013](013-ym2151-operator.md),
[CONTRIBUTING.md](../CONTRIBUTING.md#correctness-and-real-time-behavior), and
[LICENSING.md](../LICENSING.md) before implementing.

Evaluate a pinned, narrowly extracted MAME core or another verified core;
record the exact commit, file-level licenses, required helpers, original/local
hashes and local changes. Keep reusable DSP independent of MAME devices,
Rack and UI. Do not import the entire emulator framework, upgrade the
plugin's C++ standard silently, or erase upstream notices. Current upstream
code may need substantive portability work; record its cost before selection.
A candidate's own output cannot serve as its independent accuracy reference.

## Research Gate Before UI Work

Retain prototype sources, commands, source excerpts by reference and measured
fixtures under `specs/assets/017/`. Resolve the following before freezing a
complete implementation contract or polishing panels:

1.  Map physical slots, register banks, evaluation order, sync broadcast,
    key ownership and output routing for one PCM-capable group. Confirm
    four-op, paired two-op, three-op-plus-PCM and single-slot modes.
2.  Verify all seven internal waveforms, algorithm connection/feedback
    tables and accumulation behavior against primary documentation and
    hardware captures or an independently justified reference.
3.  Resolve PFM/sample-as-FM-oscillator operation, including sample addressing,
    phase wrapping and modulation, separately from ordinary PCM playback.
    Do not assume a sample player automatically supports native FM input.
4.  Verify 8/12-bit decode, start/end/loop addressing, interpolation, pitch
    scaling, alternate-loop direction/edge behavior and key-off envelopes.
5.  Establish C++11 integration, bounded slot/core cost, event latency and
    per-instance sample memory at 16 Rack lanes.

A verified subset can support experiments, but unverified PFM, waveform or
loop behavior must not be presented as accurate or hidden behind inert
controls. If these gates cannot be met, keep the affected acceptance pending
and record a proposed scope revision in this spec. Do not silently redefine
OPX as FM-only or mark both modules complete from a prototype.

## Voice 271: Native Four-Slot Group

Each Rack lane owns one independent group-0 context, including the chip-global
state necessary to run it correctly. Native slot/register timing must remain
correct even if the implementation avoids computing unused groups. Expose
all four native sync modes:

| Mode | Synthesis And Input Ownership |
| --- | --- |
| Four operators | One native four-operator voice and its 16 algorithms; one common pitch/key group. |
| Two pairs | Two native two-operator voices with their four algorithms; independent pair inputs only where hardware allows. |
| Three operators + PCM | Native three-operator voice with eight algorithms plus the independent PCM slot. |
| Four single slots | Four PCM-capable slots with native individual pitch, envelope, key and routing behavior. |

Use slot labels S1-S4 and show the actual active connection diagram. Provide
four pitch/Gate/Retrigger input groups, with inactive inputs visibly disabled
according to mode; normal missing subordinate input groups to the primary
pitch/Gate only for active native-independent units, as explicitly documented
host normalling. Never let a secondary input override a native broadcast-owned
register. Freeze the exact bank-to-slot-to-port table during the prototype.

Per slot, expose native oscillator choice, multiplier/detune, envelope/key
scaling, total level, LFO rate/waveform/AM/PM sensitivity and output level
routing. Include verified feedback and accumulation controls only at their
native locations. Preserve native tables and arithmetic; do not substitute
generic waveforms or a conventional digital filter for unusual accumulation.

Provide polyphonic A/B/C/D outputs corresponding to the native output buses,
with separate native per-bus attenuation/routing controls and a post-synthesis
host Level. No implicit stereo pan law or output-disconnection summing.
Document an external stereo downmix. Default to Four operators, a simple
sine carrier with silent modulators, no feedback/modulation, and routing to
A+B. All unpatched Voice gates are low, so startup is silent.

Mode and sample-source selections are saved settings without audio-rate CV.
Changing mode releases the old groups, clears obsolete history and applies
current controls/gates once through a bounded transition. Retain per-mode
settings and sample selections. Ordinary native algorithm/register changes
must not introduce hidden retriggers. Define sample replacement separately.

## Operator 271: One Native Slot

Expose one slot from the PCM-capable group with its native oscillator,
envelope, pitch and LFO behavior. Support the seven internal waveforms and
PCM oscillator selection once the research gate establishes their semantics.
One polyphonic Audio output taps the slot before group output routing and
post-gain. Native envelope/LFO/phase evaluation order remains intact.

Provide external PM using [013's convention](013-ym2151-operator.md#external-pm):
amount -1 to +1/default 0, +5 V = +1 phase cycle at amount +1, input clamped
to +/-10 V, non-finite input treated as zero, and evaluation every host frame
with conversion to native time. Define signed wrapping and scaling before
adding feedback. Level must not feed back through the internal path.

For internal waveforms, the injection point is native phase evaluation.
For sample oscillators, first establish the hardware's sample/phase semantics;
then document the mapping of a phase cycle, loop length and non-looping
regions for the external extension. Do not turn PM into playback-rate CV
or call unsupported sample modulation native PFM. If no defensible mapping
exists, record a scope revision instead of exposing a nonfunctional PM port
in PCM mode. Acceptance requires an explicit, tested decision for both sources.

Only feedback representable by a single-slot native self-feedback path may
be called internal feedback. Multi-slot feedback algorithms belong to Voice;
external cable loops are available and include conversion/host delay. Do not
invent an OPM feedback model for algorithms that feed another slot back.
Verify accumulation behavior independently of feedback and gain.

Expose V/oct, Gate, Retrigger, PM and native sound controls. Disconnected Gate
sustains; connected low Gate releases. The default is an internal clean sine
with a sustained native envelope. Selecting PCM with no valid sample produces
silence and a clear status, never an unexpected replacement waveform.

## Sample Assets And Saved State

Use one immutable sample-memory image shared read-only by all lanes of a
module; playback address, interpolation, envelope and loop state remain
independent per slot/lane. Enforce the native 8 MiB address-space ceiling,
not 8 MiB multiplied by polyphony. Avoid unbounded sample libraries, disk
streaming and background mutation of active memory.

Include a small original test/preset bank with recorded authorship and
reproducible encoding. Also support user mono WAV import through a non-audio
worker, converting to a selected native 8-bit or packed 12-bit representation.
Reject unsupported channel/format/size combinations clearly. Store root pitch,
source rate, encoding and start/end/loop metadata; the native playback clock
and frequency registers determine playback, not a hidden host-rate sampler.
No factory ROMs, game dumps or proprietary sample banks are bundled.

Validate every address/range, packing boundary and loop before publishing a
sample image. Prepare immutable data and retire old allocations off the audio
thread using a bounded handoff. Sample replacement keys off affected slots,
applies a documented bounded output transition and starts the new image only
at a defined boundary; never dereference freed memory during playback.
Randomization does not import, replace or delete samples.

Use Rack's supported patch asset storage for portable user samples; verify
it on the minimum supported Rack version. Save content hash, encoding and
loop/root metadata in versioned state, with relative asset references rather
than private absolute paths. Duplication and preset export must preserve the
sample or explicitly report an unresolved asset. Missing/corrupt data mutes
only affected PCM slots, preserving FM voices and visible error status.
Test save/load after moving the patch and after removing the original WAV.
The prototype must confirm asset-size/export limits before shipping import.

## Clock, Controls And Real-Time Contract

Use a nominal 16,934,400 Hz master clock and native 44,100 Hz evaluation,
verifying the clock/384 relationship and sub-slot timing. Preserve native
pitch quantization, envelope/LFO times, interpolation and arithmetic at
44.1/48/96 kHz host rates. Map 0 V to C4 for internal oscillators and to the
sample's documented root mapping for PCM; never reinterpret arbitrary sample
content as a calibrated sine. Clamp raw frequency fields before conversion.

Freeze every control's raw range/default, physical display, port/parameter
ID, mode ownership and CV policy from the research results. Required CV:
pitch, algorithm for active FM groups, waveform for internal oscillators,
multiplier/detune, TL, envelope rates/levels, LFO rate/depth and host Level.
Keep sample addresses, loop edits, file selection and sync mode out of the
real-time CV path. Tune spans -4 to +4 octaves/default 0; Level is 0-1/default
1. Additive parameter CV uses +10 V for a full span, clamps then rounds native
fields and treats non-finite input as zero. Ordinary controls scan no slower
than every 16 frames; PM and event inputs run every frame.

The widest input selects 1-16 lanes; mono broadcasts and missing channels on
shorter poly cables read zero. Gate normalling applies only to disconnected
ports as specified above. Outputs share the lane count. Removed lanes clear
all slot and sample-playback histories, newly active lanes initialize from
current settings, and simultaneous instances cannot share mutable chip state.

Use 2 V/0.01 V hysteresis. Gate-rise/retrigger causes one attack, gate-fall
wins over retrigger, and retrigger with a connected low Gate does not start
a note. Bound native writes/events, preserve accepted key edges and grouped
writes, reserve release capacity and document queue/overload latency. No
processing-thread allocation/deallocation, locks, file access, logging,
drawing, waiting for workers or work proportional to sample-file length.

Freeze native-output calibration and bus summing, aiming for nominal
single-carrier output near +/-5 V with finite +/-10 V host safety bounds.
Preserve chip clipping/accumulation before the host gain. No per-sample-bank
or per-note automatic normalization. Measure conversion quality and native
aliases separately; do not smooth away chip behavior to make a plot cleaner.

## Verification And Acceptance

Use deterministic native register streams, original PCM fixtures and external
PM fixtures, with separate provenance and predeclared tolerances. Verify every
algorithm, internal waveform, permitted feedback path, accumulation mode,
LFO setting, envelope edge, routing combination and group sync transition.
Reference renders must account for chip revision, clock, output format and
analog reconstruction. Record which claims have hardware evidence and which
remain implementation-level comparisons.

Exercise signed 8/12-bit extrema, packed odd/even samples, minimum/maximum
addresses, malformed metadata, one-shot and loop boundaries, alternate loops,
PFM, key-off during playback, missing samples, replacement while held,
portable patch reload and multi-instance isolation. ASan/UBSan must cover
sample decode, interpolation, address math, adapter and production core.

At 44.1/48/96 kHz and 64/256-frame blocks, measure 1/4/16 lanes in every Voice
mode and both Operator sources, plus two simultaneous Voices and four
connected Operators. Record machine/compiler/worker settings, memory, median,
p95 and maximum over at least 300 repetitions after 2,048 warmup frames.
On the documented target, a single 16-lane Voice and, separately, a four-module
16-lane Operator chain must each stay below 50% median and 75% p95 callback
cost with one worker. Sample loading must not stall those callbacks. Added
delay is at most 1 ms per Operator and 4 ms for four in series, including
input/output conversion and actual Rack scheduling. These are prototype
gates; a failure requires an explicit design decision, not reduced polyphony.

- [ ] Research gates resolve mode/slot mapping, waveforms, PFM, loop behavior
      and faithful native feedback/accumulation, with limitations recorded.
- [ ] A pinned attributed core builds under the existing production standard;
      reusable DSP has no Rack/MAME UI or device-framework dependency.
- [ ] Both modules implement the native synthesis units and tested external
      PM contract, including an explicit supported PCM modulation mapping.
- [ ] Sample import/storage/replacement/reload is portable, bounded, safe,
      and covered by instrumented deterministic tests.
- [ ] CPU, memory, event latency, conversion quality and chain-delay gates
      pass before final panels and presets are accepted.
- [ ] Real-module tests cover divider offsets, polyphony, rates, modes,
      malformed state, presets, reset/randomize/duplicate and concurrency.
- [ ] Existing regressions and Linux x64/macOS arm64/Windows x64 builds,
      tests and package validation pass.
- [ ] Native Rack verifies audible FM/PCM/PFM/PM examples, 16 lanes, portable
      assets, both themes, live switching, previews and cable access.
- [ ] Manuals, original samples/presets, debug patches, panels/captures and
      all inventories/notices agree; generated PDF pages are reviewed.

## Integration And Validation Commands

Proposed paths: `src/YMF271.cpp`, `src/YMF271Operator.cpp`,
`src/dsp/yamaha_ymf271/`, paired `res/<slug>` panels, an attributed original
sample bank under `res/YMF271/`, `presets/<slug>/`,
`patches/debug/<slug>.vcv` and `manual/<slug>/`. Select a pinned dependency
path after the prototype; do not create an empty production core placeholder.

Follow existing theme/title/manual/capture conventions. Operator controls
must be materially more compact than Voice, with readable grouping at normal
zoom. Provide original FM, hybrid and PCM Voice presets and internal-waveform,
percussive-sample and PM Operator examples. State requirements for external
companions and label verified native behavior versus modular extensions.

Freeze IDs and custom-state schema before presets. Integrate registration,
manifest, README/changelog, dependency notices and model/test/manual/capture/
package inventories together. Existing modules, identifiers and patches stay
compatible. Do not reactivate the removed SuperSampler/SuperSynth code.

From the repository root, with [CONTRIBUTING.md](../CONTRIBUTING.md)'s
prerequisites, Rack at `../..`, Clang and the manual/capture dependencies,
run the current aggregate checks after integration:

```shell
make -j2 all test test-rack RACK_DIR="$(pwd)/../.."
make check-build
python3 scripts/validate.py dependencies
python3 -m json.tool plugin.json > /dev/null
git diff --check
```

Implement these proposed focused targets in the existing harness; they are
not currently runnable:

```shell
make test/dsp/yamaha_ymf271/test_voice test/dsp/yamaha_ymf271/test_operator test/dsp/yamaha_ymf271/test_samples
make test/dsp/yamaha_ymf271/test_voice test/dsp/yamaha_ymf271/test_operator test/dsp/yamaha_ymf271/test_samples TEST_MODE=asan-ubsan CXX=clang++
make -C test/rack ymf271 ymf271-operator RACK_DIR="$(pwd)/../.."
make -C test/rack benchmark-ymf271 RACK_DIR="$(pwd)/../.."
for module in YMF271 YMF271Operator; do
    make -C tools/capture capture MODULE="$module"
    python3 tools/capture/export_screenshots.py tools/capture/.build/captures manual --module "$module"
    python3 tools/capture/draw_panels.py tools/capture/.build/captures --module "$module"
    make -C "manual/$module"
done
make -C manual
python3 scripts/validate.py manuals manual/.build
make -j2 dist RACK_DIR="$(pwd)/../.."
```

Validate each actual package with `python3 scripts/validate.py package` and
its concrete generated path; record the command/result. Add exact sample
fixture, hardware/reference and prototype commands here as they are created.
Record completion evidence and limitations before archival under AGENTS.md.

## Non-Goals And Planning Evidence

No full arcade-board/MAME emulator, twelve-group multitimbral workstation,
external effects-chip emulation, streaming sampler, sample editor, ROM
extraction, factory/game sample distribution, timer/IRQ frontend, arbitrary
FM routing inside Voice, existing-module changes, or release/version choice.

Primary documentation, candidate-core limitations and repository integration
were reviewed on 2026-10-01. No OPX code, hardware capture, audio comparison,
benchmark or native session has run for these modules. Status remains PLANNED
and all implementation acceptance is pending. The current request authorizes
committing/pushing the planning documents; it does not authorize issue
messages, implementing modules or publishing a release.

Planning-document checks on 2026-10-01: relative links/anchors, shell-block
syntax, unique spec numbers, planned-status metadata and whitespace passed.
Implementation commands above are future acceptance work, not results from
this documentation change.

[datasheet]: https://www.quarter-dev.info/archives/yamaha/YMF271.pdf
[mame-source]: https://github.com/mamedev/mame/blob/master/src/devices/sound/ymf271.cpp
[mame-header]: https://github.com/mamedev/mame/blob/master/src/devices/sound/ymf271.h
[ymfm]: https://github.com/aaronsgiles/ymfm
