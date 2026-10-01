# Source Organization And Rack Integration

Created: 2026-10-01
Status: PLANNED

Make the DSP/Rack boundary easier to navigate, bring panel presentation in
line with the sibling projects, and protect saved patches and audio behavior
with focused regression evidence.

## Evidence And Current Gaps

-   `src/dsp/math.hpp` and `src/dsp/trigger.hpp` are umbrella includes over
    small utility headers. Rack helpers live under `src/kautenja_rack/`,
    while host processing and custom widgets live in `src/engine/` and
    `src/widget/`. Chip-family subdirectories are cohesive and should remain.
-   Widgets in the module `.cpp` files load one panel SVG directly.
    There is no shared native light/dark panel convention. Runtime and
    manual artwork still use the older KautenjaDSP identity.
-   `IndexedFrameDisplay` stores `nsvgParseFromFile` results as raw pointers,
    indexes them with a callback, and has no explicit cleanup. Wavetable
    history actions retain raw storage pointers. The editor's click path
    permits `x == 1`, then computes `index = x * length`. These are concrete
    ownership/bounds review targets, not reasons to replace the whole UI.
-   Existing tests exercise DSP utilities and Sony S-DSP components, but
    not module registration, patch restoration, widget previews, or the
    common polyphonic `ChipModule` integration.

References: Fourier's [header and Rack-helper reorganization](https://github.com/Kautenja/ArhythmeticUnits-Fourier/commit/e5f1a43)
and its `test/rack/` fixtures; RackNES's
[branding](https://github.com/Kautenja/RackNES/commit/b783a8e),
[UI ownership/help fixes](https://github.com/Kautenja/RackNES/commit/53d6544),
and [native themes](https://github.com/Kautenja/RackNES/commit/2cb8de0).
RackNES's Blip_Buffer fixes are comparison material only: PotatoChips
already uses `memmove` in `BLIPBuffer::read_sample`, so do not import that
fix or claim its presence proves another bug here.

## Scope And Ownership

Own `src/`, runtime `res/`, and relevant `test/dsp/`/new `test/rack/` cases.
Use [002](002-build-tests-and-ci.md)'s harness before structural changes.
Coordinate metadata/license scope with [001](001-licensing-and-project-documentation.md),
documented behavior with [004](004-manual-content-and-publication-style.md),
and final figures with [005](005-production-panel-captures-and-figures.md).

[008](008-native-light-and-dark-themes.md) owns native theme implementation,
panel pairs, theme regressions, and issue #95. This spec retains branding
and structural/lifecycle work; preserve 008's theme contract during those
changes rather than implementing a second theme system.

## Requirements

1.  Establish a small source map and direct includes. Move math constants
    and Eurorack conversions to descriptive top-level DSP headers, put the
    actual generic math functions in `src/dsp/math.hpp`, and flatten the
    five trigger headers with explicit names such as `trigger_divider.hpp`.
    Remove obsolete umbrellas after updating every consumer and test.
    Retain existing namespaces and the Sony S-DSP, Yamaha YM2612, and Edges
    family groupings. Preserve attributed code and avoid mass formatting.
2.  Rename the reusable `src/kautenja_rack/` helpers to
    `src/rack_extensions/`, matching Fourier's host boundary. Keep
    `src/engine/`, module-specific widgets, and recognizable module `.cpp`
    entry points where they aid navigation. Extract code only for concrete
    ownership or test needs, not to reach an arbitrary file-length limit.
3.  Adapt Arhythmetic Units branding to the existing panels, retaining module
    names, control geometry, chip illustrations, and collaborator/upstream
    credits. Preserve asset provenance under 001 and coordinate both panel
    variants with 008, which owns native theme support and minimum-Rack
    metadata. Branding changes must apply consistently to light/dark assets
    and preserve live switching and safe browser previews.
4.  Make custom display/editor lifecycle and preview behavior explicit.
    Verify null-module construction, invalid SVG/index handling, release of
    parsed resources, editor edge coordinates, undo/redo, module deletion,
    and engine/UI storage access. Reproduce each defect before its fix and
    add a focused regression. Do not add audio-thread locks or allocations
    to solve a UI ownership problem.
5.  Preserve the entire manifest model inventory, Rack ID values, custom JSON
    meanings, defaults, quantization, presets, and disabled-module status.
    Build deterministic fixtures for registration/serialization and the
    integration being moved: clock/sample-rate handling, 1/16-channel
    independence, mono-input reuse, CV divider cadence, output normalling,
    clipping, and reset. Protect representative chip output before/after
    structural changes; use justified tolerances for floating-point output.
6.  Fix only defects reproduced by those checks or by the manual review
    within the touched integration. Record a separate behavioral increment
    and changelog entry for such fixes, so source moves remain reviewable.
    Larger chip-emulation work requires a separately scoped future spec.

## Behavior Examples

-   An existing `2A03` patch reopens with the same controls and audio after
    includes move; the browser still calls the module Infinite Stairs.
-   Changing Rack's global panel preference updates existing modules and
    preview widgets, with matching controls in both themes.
-   Clicking the right edge of a wavetable edits the final valid sample;
    undo/redo and subsequent module deletion do not access freed storage.

## Non-Goals

No new chips, enabled SuperSampler/SuperSynth, universal emulator rewrite,
algorithm optimization, namespace-wide rename, changed patch format, or
new public controls. Do not adopt Fourier's FFT scheduling or RackNES's
mapper/ROM changes. Do not delete dormant chip implementations merely
because they are not registered modules.

## Acceptance Criteria

- [ ] Header/helper moves have explicit includes and no remaining obsolete
      consumers; source attribution and C++11 compatibility are retained.
- [ ] Registration, IDs, JSON, presets, and representative audio fixtures
      agree with the baseline except for separately documented bug fixes.
- [ ] Branding and structural changes preserve 008's theme support across
      all 16 enabled entries, including blanks, with safe null-module
      previews; disabled status remains unchanged. Theme delivery is tracked
      in 008 rather than duplicated here.
- [ ] Targeted display ownership/editor edge/undo regressions pass, with
      sanitizer evidence where the platform permits it.
- [ ] Mono/polyphony, reset, sample rates, and normalling are checked at the
      integration boundary; affected manual claims are reconciled.
- [ ] Final runtime artwork is handed to 005 for regenerated figures.

## Validation

Run from the repository root after 002 implements these targets, using the
same Rack SDK/compiler as the baseline:

```shell
make -j2 test
make -j2 all test-rack
make -j2 test-asan-ubsan
git diff --check
git diff d1821c9f -- plugin.json
```

Exercise 44.1, 48, and 96 kHz with deterministic processing fixtures and
record expected scaling/tolerances. In Rack, reopen affected existing
`patches/debug/` examples and Super Echo presets; test 1 and 16 channels,
theme changes, editor undo/redo, browser previews, and module removal.
005's capture tool supplies graphical regression evidence, but does not
replace the interactive/audio checks. Record old/new file mapping and
distinguish structural equivalence from intentional fixes.

## Completion Evidence

Source moves, panel changes, reproductions, and regression runs are pending.

## Build-Migration Findings

Spec 002 confirmed that `src/dsp/exceptions.hpp` relies on Rack's unqualified
`Exception` type; its independent implementation is commented out. The two
standalone BLIP/processor fixtures use `test/support/exception.hpp` to retain
the original suite coverage without importing Rack. Remove this host coupling
as part of DSP organization and then retire that test substitute. PCM's broad
comparison operators also require Catch2 to be included first; constrain
those operators only with focused compatibility/regression evidence.

On Windows x64 with the pinned Rack 2.6.3 MinGW64/MSVCRT toolchain, the
existing `test/dsp/test_pcm.cpp` regression fails: `sizeof(PCM::int24_t)`
is 4, but the storage contract requires 3. Linux and macOS report 3.
Investigate the packed bitfield in `src/dsp/pcm.hpp` and choose a portable
representation with conversion/sign-extension and byte-layout regressions.
Preserve the SDK ABI; do not hide this with a Windows-only expected size,
a disabled assertion, or an unrelated global bitfield flag. Spec 002 leaves
the Windows job failing and runs the remaining suites/package checks so
other failures remain observable.
