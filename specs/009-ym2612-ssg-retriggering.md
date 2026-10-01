# Reliable YM2612 Looping Envelope Retriggers

Created: 2026-10-01
Status: PLANNED
Issue: [#82](https://github.com/Kautenja/PotatoChips/issues/82)
Planning baseline: `59638fd9` (product source unchanged from `33fb1554`).

Make Mini Boss and Boss Fight reliably restart their looping envelopes
under polyphonic gate/retrigger input. Eliminate the reported intermittent
one-shot envelope in looping mode, preserve ordinary envelope behavior,
and verify both soft-reset settings before resolving #82.

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

-   [MiniBoss.cpp](../src/MiniBoss.cpp) owns one `FeedbackOperator` and gate/
    retrigger detector pair per polyphonic voice. [BossFight.cpp](../src/BossFight.cpp)
    owns one `Voice4Op` per voice and detectors per operator/voice. Both
    acquire gate and retrigger inside a CV update divided by 16.
-   Both modules XOR the sustained gate with a detected retrigger edge,
    temporarily closing a held gate until the next CV update. A pulse or
    intervening low interval entirely between acquisition frames can be
    missed. This is an input-timing risk, not proof of the reported one-shot
    failure. Test normal MIDI-width pulses as well as short-pulse edges.
-   [Operator::set_gate()](../src/dsp/yamaha_ym2612/operator.hpp) ignores an
    unchanged gate. Key-on selects attack and optionally resets oscillator
    phase; it does not explicitly reset envelope attenuation or the global
    envelope counter. Key-off selects release for an active envelope.
-   `update_ssg_envelope_generator()` loops only when SSG is enabled,
    attenuation is at least `0x200`, and the stage is above release. Both
    [FeedbackOperator](../src/dsp/yamaha_ym2612/feedback_operator.hpp) and
    [Voice4Op](../src/dsp/yamaha_ym2612/voice4op.hpp) call it before output,
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
[Mini Boss](../patches/debug/MiniBoss.vcv) and
[Boss Fight](../patches/debug/BossFight.vcv) debug patches are compatibility
fixtures, not substitutes for the issue's reproduction.

This work can proceed independently of modernization. Reuse
[002](archive/002-build-tests-and-ci.md)'s harness when available, otherwise preserve
the SCons/Catch2 v2 workflow and exclude SDK-dependent tests from standalone
test discovery. [003](archive/003-source-organization-and-rack-integration.md) must
preserve this fix during refactoring; [004](archive/004-manual-content-and-publication-style.md)
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

- [ ] A current reproduction or verified intervening fix is linked to a
      deterministic before/after case, with exact settings and root cause.
- [ ] Accepted events reliably restart looping envelopes in both modules
      and soft-reset settings, without one-shot leaks or lost/duplicate
      triggers across the timing and polyphony matrix.
- [ ] Non-looping envelopes, gate-off release, phase policy, operator/voice
      independence, normalling, reset, reload, and sample-rate checks pass.
- [ ] Focused regressions, bounded stress replay, applicable DSP suites, and
      the Rack build pass. Native Rack confirms the reported four-or-more
      voice workflow; automated success alone is not native verification.
- [ ] Mini Boss/Boss Fight manuals and changelog describe verified looping,
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
    [AGENTS.md](../AGENTS.md#planning-and-completion) only after all criteria
    and issue follow-through are satisfied.

With authenticated GitHub CLI, prepare a factual body file first. Comments
are permitted during implementation; closure is only for verified resolution:

```shell
gh issue comment 82 --repo Kautenja/PotatoChips --body-file /tmp/potatochips-82-update.md
gh issue close 82 --repo Kautenja/PotatoChips --reason completed
gh issue view 82 --repo Kautenja/PotatoChips --json state,stateReason,comments,url
```

## Completion Evidence

Planning: issue body, embedded patch, comments, module/DSP paths, trigger
history, tests, and manuals reviewed on 2026-10-01. Reproduction, root-cause
confirmation, implementation, and executable/native verification are pending.

| Issue | Fix Commit | Verification | Resolution Comment | Final State |
| --- | --- | --- | --- | --- |
| #82 | Pending | Source/history review only; regression/native checks pending | Pending | Open at planning time |
