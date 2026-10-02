# Reliable YM2612 Looping Envelope Retriggers

Created: 2026-10-01
Status: COMPLETE
Issue: [#82](https://github.com/Kautenja/PotatoChips/issues/82)
Planning baseline: `59638fd9` (product source unchanged from `33fb1554`).

Make Mini Boss and Boss Fight reliably restart their looping envelopes
under polyphonic gate/retrigger input. Eliminate the reported intermittent
one-shot envelope in looping mode, preserve ordinary envelope behavior,
and verify both soft-reset settings before resolving #82.

## Hardware-Fidelity Constraint

The implementation request on 2026-10-01 explicitly prioritizes preserving
hardware quirks over making the envelope behave like an ideal hard-reset
LFO. This supersedes any reading of the original cycle-restart requirement
that would clear attenuation, reset the shared envelope clock, force AR 31,
or make zero-rate envelopes loop. Key-on enters attack from the current
attenuation; the first contour depends on the previous contour and the
free-running envelope clock. The regression oracle uses identical incoming
state and clock position, rather than assuming every note starts at silence.

The existing implementation is a simplified repeating SSG contour, not a
complete hardware SSG emulator. This change preserves its envelope behavior;
it does not certify all of that pre-existing behavior as hardware accurate.

## Report And Source Evidence

Issue #82, "YM2612 Triggers fail sometimes in SSG mode", was opened on
December 17, 2020. It was open with no comments when read on October 1,
2026. The report uses Mini Boss with a MIDI controller allocating four or
more voices and a fast looping envelope. Retriggers should restart the
envelope cycle, but occasionally produce a one-shot note. The author
explicitly reports the failure with "prevent clicks" both enabled and
disabled; disabling that option is not an acceptable fix.

The issue includes a Rack 1 patch (`1.dev.476a49b`, PotatoChips `1.10.1`)
with four-channel MIDI-CV, gate/retrigger/pitch connections, and a Mini Boss
output-to-FM cable. Its parameter values map to the current Mini Boss as
follows; verify this mapping when creating the current Rack fixture:

| Control | Reported Value | Control | Reported Value |
| --- | --- | --- | --- |
| Attack rate | 15 | Total level | 100 |
| Decay rate | 31 | Sustain level | 5 |
| Sustain rate | 7 | Release rate | 0 |
| Loop/SSG enable | 1 | Rate scaling | 0 |
| Frequency offset | -1.18500173 | LFO | 0 |
| FMS | 0 | AMS | 0 |
| FM depth | 0 | Multiplier | 1 |
| Feedback | 3 | Output volume | 127 |

The patch stores `prevent_clicks: true`. Repeat with false. The FM cable
is present even though its depth is zero; preserve that detail in the
source fixture and record simplifications rather than silently dropping it.
The old audio/MIDI device names and third-party analyzers are incidental,
not prerequisites for reproduction.

Relevant current implementation:

-   [MiniBoss.cpp](../../src/MiniBoss.cpp) owns one `FeedbackOperator` and gate/
    retrigger detector pair per polyphonic voice. [BossFight.cpp](../../src/BossFight.cpp)
    owns one `Voice4Op` per voice and detectors per operator/voice. Both
    acquire gate and retrigger inside a CV update divided by 16.
-   Both modules XOR the sustained gate with a detected retrigger edge,
    temporarily closing a held gate until the next CV update. A pulse or
    intervening low interval entirely between acquisition frames can be
    missed. This is an input-timing risk, not proof of the reported one-shot
    failure. Test normal MIDI-width pulses as well as short-pulse edges.
-   [Operator::set_gate()](../../src/dsp/yamaha_ym2612/operator.hpp) ignores an
    unchanged gate. Key-on selects attack and optionally resets oscillator
    phase; it does not explicitly reset envelope attenuation or the global
    envelope counter. Key-off selects release for an active envelope.
-   `update_ssg_envelope_generator()` loops only when SSG is enabled,
    attenuation is at least `0x200`, and the stage is above release. Both
    [FeedbackOperator](../../src/dsp/yamaha_ym2612/feedback_operator.hpp) and
    [Voice4Op](../../src/dsp/yamaha_ym2612/voice4op.hpp) call it before output,
    then advance envelope timers afterward. Trace this ordering and the
    sustain-stage restart path; do not assume the module XOR is the sole
    cause. Boss Fight shares one envelope context across its four operators.
-   The current menu says "Soft Reset Envelope Generator", but the saved
    key remains `prevent_clicks`. Preserve that key and its phase policy.
-   Commit
    [`34990c0c`](https://github.com/Kautenja/PotatoChips/commit/34990c0cd24c909e21d69ad620fb8736c3a5dfe6)
    replaced full SSG mode selection with a simplified looping envelope
    before this report. Later trigger fixes
    [`33165d81`](https://github.com/Kautenja/PotatoChips/commit/33165d81e3da2b6bb0510065df1cf4b20fd31cb8)
    and [`009015e5`](https://github.com/Kautenja/PotatoChips/commit/009015e5c391b2a66fd6b4de746aa0112d6cdcba)
    changed threshold handling. Their commit titles do not establish that
    #82 was fixed. The current test tree has no YM2612 regression suite.

These are source findings and hypotheses. No current native reproduction,
root-cause confirmation, or executable regression has run during planning.

## Scope And Dependencies

Own the focused changes in `src/MiniBoss.cpp`, `src/BossFight.cpp`, and
`src/dsp/yamaha_ym2612/` where evidence requires them; add pure DSP and
SDK-backed module regressions, a minimal debug patch/event fixture, targeted
manual corrections, and an unreleased changelog entry. Existing
[Mini Boss](../../patches/debug/MiniBoss.vcv) and
[Boss Fight](../../patches/debug/BossFight.vcv) debug patches are compatibility
fixtures, not substitutes for the issue's reproduction.

This work can proceed independently of modernization. Reuse
[002](002-build-tests-and-ci.md)'s harness when available, otherwise preserve
the SCons/Catch2 v2 workflow and exclude SDK-dependent tests from standalone
test discovery. [003](003-source-organization-and-rack-integration.md) must
preserve this fix during refactoring; [004](004-manual-content-and-publication-style.md)
carries verified behavior into the broader manual rewrite. This spec owns
#82's regression evidence and issue follow-through.

## Requirements

### Reproduce With A Deterministic Event Sequence

1.  Re-read the issue/comments, translate the attached Rack 1 settings into
    a minimal Rack 2 patch, and record the exact versions, sample rate,
    parameters, pulse levels/widths, voice allocation mode, and soft-reset
    setting. Use current Core monitoring where possible; do not require
    obsolete plugins or configure a user's audio/MIDI devices from the JSON.
2.  Reproduce four-voice rapid notes, chords, overlapping notes, voice reuse,
    and stealing, including a held gate with a retrigger. Capture the actual
    per-channel gate, retrigger, and pitch stream. Replay a fixed sequence
    without needing a physical MIDI controller or nondeterministic timing.
3.  Reduce any failure to a bounded regression and retain a before trace:
    host frame, voice/operator, input edges, accepted key events, loop flag,
    envelope stage/attenuation, and envelope timer position. Keep diagnostic
    access in tests or a narrow read-only seam; no audio-thread logging or
    permanent public control is needed.
4.  Separate a missed input event, a restart at the wrong envelope state,
    and normal gate-off release. A missed narrow pulse alone does not explain
    a confirmed one-shot after an accepted retrigger. Check both module
    integration and the shared operator before selecting the smallest fix.
    If an intervening commit already fixed the original failure, identify
    it with comparative evidence and retain the regression. Failure to
    reproduce by manual playing alone is not resolution evidence.

### Deliver Events And Restart The Loop Reliably

1.  In looping mode, every valid gate-on or retrigger during a held gate
    starts one defined envelope cycle, which continues looping while gated.
    Define the restart attenuation/stage and timing tolerance in tests so
    prior cycle position cannot cause a one-shot leak, stuck release, or
    missing cycle. Compare with a known-good cycle at identical parameters,
    accounting explicitly for envelope-clock quantization.
2.  Observe gate/retrigger transitions each host sample, independently of
    slow parameter acquisition, or use an equivalent lossless event path.
    Preserve high/low hysteresis at 2 V/0.01 V. A one-sample pulse preceded
    by a sampled low state must be observed at any divider offset; held-high
    retrigger input produces one edge. Keep ordinary parameter CV updates
    divided unless changing their cadence is necessary and justified.
3.  Define event ordering explicitly. Gate rise plus retrigger on the same
    frame starts once; retrigger during a held gate restarts once; sustained
    high input does not repeatedly restart. Characterize gate fall plus
    retrigger, retrigger with gate low, and disconnection, preserving their
    documented behavior or recording a narrowly justified correction. Do
    not leave a voice latched on after gate-off or a pending artificial
    key-off that turns a valid loop into release.
4.  Separate envelope-cycle restart from oscillator phase policy. Both
    `prevent_clicks` settings must loop reliably. Preserve ordinary soft
    versus hard key-on phase behavior and existing loop-boundary behavior
    unless the demonstrated defect requires a documented change. Do not
    claim the soft option guarantees click-free audio.
5.  Keep state per voice and per Boss Fight operator. Retriggering one
    operator must not reset shared envelope/LFO counters or restart other
    operators; do not reset an entire `Voice4Op` to repair one operator.
    Preserve operator order (`OPERATOR_INDEXES`) and gate/retrigger normalling
    from operator 1 through 4, including explicitly patched overrides.
6.  Preserve ordinary non-looping attack/decay/sustain/release, key-off
    release in looping mode, rate scaling, feedback, modulation, and audio
    scaling. Exercise loop enable/disable during a note, reset, sample-rate
    changes, channel-count shrink/grow, and old-patch reload without stale
    retrigger state. Distinguish module reset from DSP reset in the tests.

Fix the demonstrated integration defect at the Rack boundary where possible.
Change shared emulator code only when needed and retain its attribution.
Do not disable looping, force maximum attack rate, require a particular
MIDI allocation mode, or disable soft reset to hide the symptom.

## Regression Matrix

| Area | Required Cases |
| --- | --- |
| Modules and voices | Mini Boss at 1, 4, and 16 channels; Boss Fight at the same counts, each operator isolated and all four together. |
| Event timing | All 16 CV-divider offsets; one-sample, 1 ms, and held-high retrigger pulses with sampled low intervals; simultaneous/staggered gate and retrigger changes. |
| Envelope state | Retrigger during attack, decay, sustain, release, and immediately around loop boundaries; reported fast settings plus slower cycles and rate/level boundaries. |
| Host timing | 44.1, 48, and 96 kHz, plus rate changes with active voices. |
| Modes | Loop on/off and `prevent_clicks` true/false; loop mode toggled mid-note. |
| Polyphony/routing | Repeated four-voice MIDI allocation, overlapping/legato notes and stealing; channel shrink/grow, unpatched inputs, and Boss Fight normalling/overrides. |
| Compatibility | Existing debug patch reload, parameters/custom JSON, output range, non-looping audio, and unaffected voices/operators. |

Add pure DSP coverage for `Operator`, `FeedbackOperator`, and `Voice4Op`
where their shared behavior is involved, plus tests against the real Rack
module classes for input sampling and voice routing. Do not replace the
production path with a copied test algorithm. Use envelope/state traces
as well as audio so feedback, carrier phase, and clipping do not hide a
one-shot envelope or falsely imply failure from a waveform difference.

Record a fixed stress sequence with at least 10,000 accepted note/retrigger
events across polyphonic voices for each soft-reset setting. Assert cycle
restart/continuation and zero lost or duplicate events, rather than only
checking nonzero audio or listening for a failure. Keep the minimized case
as the fast regression and the longer sequence bounded and reproducible.

## Compatibility And Non-Goals

Preserve `MiniBoss` and `2612` model slugs, all parameter/port/light IDs,
ranges/defaults, channel routing, saved `prevent_clicks` semantics, presets,
clock rate, frequency tuning, and unaffected DSP output. Keep C++11 support
and bounded processing without new audio-thread allocations or locks.
Document any deliberate event-timing change and its saved-patch impact.

No full hardware SSG-mode implementation, new waveform modes, oscillator
hard-sync input, general YM2612 replacement, UI redesign, or broad trigger/
test-framework migration. The issue's envelope restart expectation does
not reopen the declined Mini Boss hard-sync feature in #94. Do not publish
a release or close #82 from a planning commit.

## Acceptance Criteria

The unchecked items below are retained as historical limitations. The user's
2026-10-01 completion decision accepts the verified implementation and
supersedes the original requirement to resolve those items before archiving.
It does not assert that the original one-shot symptom was reproduced or
that GitHub issue #82 was closed.

- [ ] A current reproduction or verified intervening fix is linked to a
      deterministic before/after case, with exact settings and root cause.
- [x] Accepted events reliably restart looping envelopes in both modules
      and soft-reset settings, without one-shot leaks or lost/duplicate
      triggers across the timing and polyphony matrix.
- [x] Non-looping envelopes, gate-off release, phase policy, operator/voice
      independence, normalling, reset, reload, and sample-rate checks pass.
- [x] Focused regressions, bounded stress replay, applicable DSP suites, and
      the Rack build pass. Native Rack confirms the reported four-or-more
      voice workflow; automated success alone is not native verification.
- [x] Mini Boss/Boss Fight manuals and changelog describe verified looping,
      retrigger, and soft-reset behavior without promising full SSG hardware
      modes or unrelated oscillator synchronization.
- [ ] #82 receives a resolution comment with the actual fixing commit and
      verification evidence, is closed as completed, and its comment URL
      and final state are recorded below.

## Validation Commands

Run from the repository root with a prepared Rack 2 SDK/tree, matching
runtime, C++11 compiler, SCons, and pinned Catch2 v2. Existing commands:

```shell
scons test/dsp/trigger/test_threshold.cpp
scons test/dsp/trigger/test_divider.cpp
make -j2
git diff --check
```

Add these focused targets during implementation, or record equivalent
commands supplied by 002. They do not exist at planning time. The first
uses a new pure DSP test source discovered by SCons; the second exercises
the real module classes with the Rack SDK and deterministic event replay:

```shell
scons test/dsp/yamaha_ym2612/test_ssg_retrigger.cpp
make -C test/rack ym2612-ssg RACK_DIR="$(pwd)/../.."
```

Run the applicable full DSP suite (`scons -j2 test`, or `make -j2 test`
after 002). Rebuild changed manuals with `make -C manual/MiniBoss` and
`make -C manual/BossFight`; confirm and inspect their `build/manual.pdf`
outputs because legacy recipes can mask TeX failures. Record native Rack
actions, versions, event fixtures, stress counts, and before/after traces
separately. Missing graphical access leaves native acceptance pending.

## Issue Updates And Closure

The October 1, 2026 user request authorizes comments on #82 and closure once
resolved. Carry that authorization into implementation without another
confirmation. Post useful reproduction findings, focused evidence requests,
or verified results; avoid repetitive progress comments. Planning is not a fix.

1.  Re-read the issue/comments before posting or closing, account for later
    reports, and avoid duplicate comments on retries.
2.  Commit the tested implementation and identify the actual fixing commit
    with its full SHA and canonical GitHub URL. If a historical fix is proven,
    cite that commit and the new regression commit separately. Verify the
    references are accessible upstream; do not cite this spec as the fix.
    Follow the authorized push/merge workflow. This spec does not authorize
    pushing or releasing; keep closure pending while required commits are
    only local.
3.  Post a resolution comment describing the cause, corrected behavior,
    both modules and soft-reset settings tested, native Rack versions,
    event/stress verification, and fixing commit links. Distinguish a
    committed fix from availability in a VCV Library release.
4.  After acceptance passes and the comment succeeds, close #82 with reason
    `completed` and read back the final state. Record the comment URL,
    fixing SHAs, date, and validation here. Failed remote actions remain
    outstanding rather than being reported as complete.
5.  Mark `COMPLETE` and archive according to
    [AGENTS.md](../../AGENTS.md#planning-and-completion) only after all criteria
    and issue follow-through are satisfied.

With authenticated GitHub CLI, prepare a factual body file first. Comments
are permitted during implementation; closure is only for verified resolution:

```shell
gh issue comment 82 --repo Kautenja/PotatoChips --body-file /tmp/potatochips-82-update.md
gh issue close 82 --repo Kautenja/PotatoChips --reason completed
gh issue view 82 --repo Kautenja/PotatoChips --json state,stateReason,comments,url
```

## Completion Evidence

Implementation commit: [`b7f1684db6e3617986e43d1da0860048648f3010`](https://github.com/Kautenja/PotatoChips/commit/b7f1684db6e3617986e43d1da0860048648f3010).

Implemented on 2026-10-01 against baseline `3cff51d2`, on macOS arm64 with
Apple Clang, the local Rack 2.6.0 SDK/runtime, and Catch2 3.16.0. Source work,
regressions, native replay and manuals are implemented.

### Completion Decision

On 2026-10-01, the user explicitly requested marking this spec complete,
archiving it, committing and pushing. Status is COMPLETE and the spec is
archived with the verification evidence and known limitations retained.
This decision supersedes the original reproduction/issue-closure archival
gates; it does not turn the unconfirmed accepted-event one-shot report into
a verified fix. Issue #82 remains separate follow-up work. The user authorized
pushing the current branch; no release was requested.

Archive validation: local Markdown links and incoming spec references were
checked, and `git diff --check` passed. This archival change does not alter
code; the implementation's executable/native results below remain the
validation evidence.

### Findings And Scope

Issue #82 was re-read via GitHub CLI on 2026-10-01: OPEN, no comments.
Its embedded Rack 1 patch confirms all 16 parameter values listed above,
rotation allocation (`polyMode: 0`), four MIDI channels, and `prevent_clicks:
true`. The [Rack 2 fixture](../../patches/debug/YM2612-SSG-Retrigger.vcv)
retains the MIDI pitch/gate/retrigger cables and zero-depth OUT-to-FM cable.
It omits obsolete analyzers/audio modules and device selection. No physical
MIDI device or audio device was selected during verification.

Two integration defects are established, independently of the older report:

-   The pre-fix real module fails the event-order regression: a simultaneous
    gate/retrigger rise between CV frames leaves the operator silent when
    the explicit chip-key reference has entered attack.
-   The [before trace](../assets/009/before-events.csv) records an accepted
    retrigger at frame 96 inserting RELEASE until frame 112. The
    [after trace](../assets/009/after-events.csv) stays keyed on and attacking.
    A one-sample pulse at frame 65 also exercises the divider blind spot.
    Settings are the report's, at 48 kHz, soft reset on, four voices, with
    voice 0 held at 5 V. Stage values: 0 silent, 1 release, 2 sustain,
    3 decay, 4 attack. EG count and fractional timer are recorded each frame.

The core did not produce an accepted-event one-shot in the tested sequences.
A held key with nonzero rates keeps traversing attack/decay/sustain. This
is not proof that the 2020 report was solely a missed pulse, nor evidence
of a historical fixing commit. No unproven envelope-state repair was made.

[Nemesis's corrected hardware research](https://gendev.spritesmind.net/forum/viewtopic.php?f=24&start=410&t=386)
and the [Genesis Plus GX implementation](https://github.com/ekeeke/Genesis-Plus-GX/blob/master/core/sound/ym2612.c)
were consulted for the distinction between key-on, attenuation and SSG phase
behavior. The implementation keeps slow attacks, zero-rate holds, current
attenuation, the shared EG/LFO clocks, and the existing loop-boundary phase
resets. Full SSG modes, release-model changes, and chip clock/tuning changes
remain outside this fix.

The only DSP arithmetic changes replace signed negative left shifts in
feedback and modulation with bounded multiplication. UBSan reproduced the
old error (`left shift of negative value -49`). Products fit in `int32_t`.
[Before](../assets/009/audio-before.csv) and [after](../assets/009/audio-after.csv)
fingerprints agree in all 108 configurations: three host rates, two phase
policies, loop on/off, the single operator with bipolar external modulation,
and all eight four-operator algorithms. Each renders 48,000 samples with
feedback 7 and nonzero AM/FM sensitivity. The sanitized renderer also matches.
These are host-local equivalence checks, not hardware reference recordings.
Original attribution is retained. Test friends expose read-only state in
test code; there are no runtime diagnostic counters, logging or allocation.

### Event Contract And Compatibility

-   Gate/retrigger detection now runs every host sample, with unchanged
    2 V/0.01 V hysteresis. Parameter CV stays divided by 16; newly activated
    lanes acquire controls before their first key event.
-   A retrigger completes key-off/key-on before rendering. Simultaneous
    gate rise/retrigger produces one key-on. Held-high retrigger produces
    one edge; a sampled low rearms it.
-   Retrigger with gate low, including coincident gate fall, keys on for one
    host sample and releases on the next. Previously that pulse lasted a
    CV interval. Disconnect releases keys; retired lanes release and rearm
    their detectors. Dormant DSP clocks are not advanced or reset.
-   Module reset still resets the soft-reset option, without resetting the
    live DSP or input detectors. DSP reset is tested separately. Reloaded
    modules start with fresh DSP progress. IDs, defaults, routing, JSON key
    and parameter ranges remain unchanged.
-   Soft reset preserves oscillator phase only on external key-on. Both
    settings keep the existing SSG phase resets and current attenuation.
    No promise of click-free audio or identical fresh-note contours is made.

### Automated And Native Evidence

The focused Rack suite compares production module state against explicit
chip key writes on every tested frame, including envelope state, attenuation,
phase, gate/loop flags, EG count/timer and LFO timer. It covers both modules,
all 16 divider offsets, 1/4/16 channels, 44.1/48/96 kHz, loop on/off,
both soft settings, one-sample/1 ms/held-high pulses, event priority,
hysteresis, normalling/each operator override, inactive voice rearming,
module reset/custom-JSON reload and active sample-rate change. Comparison
uses the same incoming clock phase: tolerance is zero host frames.

The bounded stress generator runs 10,000 scheduled events per module per
soft-reset setting (40,000 total), spaced 521 frames apart, rotating through
four voices. Every seventh event combines a gate fall and retrigger; other
events include held-key retriggers and reopened keys. It checks every
operator's state against explicit chip events and then verifies at least
two natural loop repetitions per operator after the last event. No state
mismatches, stuck release or one-shot leakage occurred. Event counts describe
the fixed stimulus; production code has no event-count instrumentation.

Pure DSP tests exercise Operator, FeedbackOperator and Voice4Op, key-on
attenuation/phase semantics, shared-clock independence, continuing loops,
key-off release, repeated phase resets above 0x200, zero-rate holds, rate
scaling, and loop toggles across AR/decay/SL boundaries.

[Native replay source](../assets/009/native_replay.cpp) uses Rack's actual
`dsp::MidiParser<16>` in rotation mode to generate rapid notes, overlapping
notes, chords, stealing, key-off and voice reuse at 48 kHz. Core's 1 ms
retrigger pulses are 10 V. The change-only per-channel streams and sampled
states are retained for [Operator/hard](../assets/009/MiniBoss-hard-midi.csv),
[Operator/soft](../assets/009/MiniBoss-soft-midi.csv),
[Voice/hard](../assets/009/2612-hard-midi.csv), and
[Voice/soft](../assets/009/2612-soft-midi.csv).
The harness processes all four voices and every operator, asserting loop
continuation, and renders actual module widgets with Fundamental 2.6.1
Scope. The upper Scope trace is audio; the lower is a diagnostic linear
mapping of internal attenuation (not a new module output). All four native
captures were visually inspected. This is a device-free native replay, not
a physical keyboard performance, listening test, or original failing trace.

### Commands And Results

The implemented Make harness supersedes the historical SCons commands above.
Run from the repository root with the prepared local Rack SDK/runtime:

```shell
make test/dsp/yamaha_ym2612/test_ssg_retrigger
make -C test/rack ym2612-ssg RACK_DIR="$(pwd)/../.."
make test/dsp/yamaha_ym2612/test_ssg_retrigger TEST_MODE=asan-ubsan
make test-ym2612-ssg RACK_TEST_MODE=asan-ubsan TEST_ARGS='*coincident*,*reset*,*overrides*'
make -j2 all test-rack test
make -j2 test
make -j2 test-rack
make -C manual/MiniBoss
make -C manual/BossFight
git diff --check
```

Focused DSP: 4 cases, 1,248,994 assertions, ordinary and ASan/UBSan pass.
Focused Rack: 6 cases, 9,850,532 assertions pass. Focused Rack sanitizer:
3 event/lifecycle/routing cases, 17,012 assertions pass. The complete
standalone DSP suite passes. C++11 plugin builds. The first combined full
run was interrupted by concurrent spec 011 registration changing the model
count from 16 to 17; no YM2151 code or expectations were changed by this task.
The five other existing Rack suites were run independently and passed.
After the concurrent contract update, the full Rack run passes all eight
suites (including the separate YM2151 suite). The complete standalone run
passes all 13 suites, including the separate YM2151 work.

Both manuals rebuilt successfully. All 20 rendered pages were inspected,
including full-size changed operation pages; no overflow or clipping was
seen. Generated PDFs remain ignored build products.

Reproduce the native renderer on macOS with Fundamental built in its usual
Rack plugin directory (Linux needs its OpenGL/dl link equivalents):

```shell
c++ -std=c++11 -O2 -DTEST -Wno-unused-local-typedefs -Wno-deprecated-declarations -I. -isystem ../../include -isystem ../../dep/include specs/assets/009/native_replay.cpp -L../.. -lRack -framework OpenGL -o /tmp/009-native-replay
mkdir -p /tmp/009-native/user
DYLD_LIBRARY_PATH=../.. /tmp/009-native-replay ../.. . ../../plugins/Fundamental /tmp/009-native
c++ -std=c++11 -O2 -I. specs/assets/009/audio_fingerprint.cpp -o /tmp/009-audio
/tmp/009-audio > /tmp/009-audio.csv
cmp specs/assets/009/audio-after.csv /tmp/009-audio.csv
make test-ym2612-ssg TEST_ARGS='[.trace]'
```

### Retained Limitations And Issue Follow-Up

Do not close #82 on the strength of the input-path fix alone. An accepted
retrigger that subsequently becomes a one-shot has not been reproduced;
there is no verified historical fixing commit. A captured failing event
stream is still needed to distinguish a remaining defect from the expected
state-dependent chip contour. The implementation request's hardware-fidelity
constraint rules out forcing an idealized hard-reset LFO as a workaround.
The completion request authorizes committing and pushing, but the resolution
comment/closure remains pending. Native Linux/Windows and physical MIDI/audio
listening checks were not performed. These limitations are retained in the
completed archive rather than treated as outstanding implementation work.

| Issue | Fix Commit | Verification | Resolution Comment | Final State |
| --- | --- | --- | --- | --- |
| #82 | [`b7f1684db6e3617986e43d1da0860048648f3010`](https://github.com/Kautenja/PotatoChips/commit/b7f1684db6e3617986e43d1da0860048648f3010) | Focused DSP/Rack/stress, sanitizer, native replay, audio equivalence and manuals verified; original accepted-event one-shot unconfirmed | Pending | OPEN when re-read 2026-10-01 |
