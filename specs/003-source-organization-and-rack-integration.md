# Source Organization And Rack Integration

Created: 2026-10-01
Status: IN PROGRESS

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

- [x] Header/helper moves have explicit includes and no remaining obsolete
      consumers; source attribution and C++11 compatibility are retained.
- [x] Registration, IDs, JSON, presets, and representative audio fixtures
      agree with the baseline except for separately documented bug fixes.
- [ ] Branding and structural changes preserve 008's theme support across
      all 16 enabled entries, including blanks, with safe null-module
      previews; disabled status remains unchanged. Theme delivery is tracked
      in 008 rather than duplicated here.
- [x] Targeted display ownership/editor edge/undo regressions pass, with
      sanitizer evidence where the platform permits it.
- [x] Mono/polyphony, reset, sample rates, and normalling are checked at the
      integration boundary; affected manual claims are reconciled.
- [x] Final runtime artwork is handed to 005 for regenerated figures.

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

Implemented October 1, 2026, starting from `adebc068`, and committed/pushed as
`b6e05564`. Follow-up fixes are `49cfc083` and `12e03225`. Status remains
`IN PROGRESS` for the remaining native interaction/listening checks below. Theme implementation
still belongs to 008, and production figure capture belongs to 005.

### Structural Increment

| Before | After |
| --- | --- |
| `dsp/math/constants.hpp` | `dsp/constants.hpp` |
| `dsp/math/eurorack.hpp` | `dsp/eurorack.hpp` |
| `dsp/math/functions.hpp` plus umbrella | `dsp/math.hpp` |
| `dsp/trigger/{boolean,divider,hold,threshold,zero}.hpp` plus umbrella | `dsp/trigger_<name>.hpp` |
| `kautenja_rack/{helpers,param_quantity}.hpp` | `rack_extensions/{helpers,param_quantity}.hpp` |

All paths above are relative to `src/`. Consumers use the specific trigger and
voltage headers. The ten utility headers compile individually as C++11 without
Rack. DSP exceptions now derive from `std::runtime_error`; the temporary test
substitute is removed. The pitch reference retains Rack's exact `261.6256f`
constant without including Rack. Chip family directories and attribution remain.

Eighteen registered-model SVGs, including two disabled modules and both blanks,
use the sibling Arhythmetic Units vector footer. XML comparison confirmed that
all other attributes and geometry are unchanged. NanoSVG raster previews of
Super Echo, Name Corp, Boss Fight, and Super VCA were generated; Super Echo and
Name Corp were visually inspected. Asset provenance is in `LICENSING.md`,
`docs/licenses/THIRD-PARTY.txt`, and `res/ArhythmeticUnits.svg`. Dormant SCC and
TurboGrafx16 artwork is unchanged. No paired themes existed to update; 008 must
carry the new footer into both variants. Existing manual figures are deliberately
left for 005 to regenerate from the final widgets.

### Behavioral Increment And Reproductions

-   The old editor's `x == width` click produced an ASan heap-buffer-overflow.
    Undo after deleting its raw sample storage produced heap-use-after-free.
    Editing now clamps to the last sample, includes both drag endpoints, and
    supports vertical changes within a sample. Only live left-button presses
    edit; previews own a read-only copy and right clicks tolerate missing parents.
-   History owns before/after values and resolves a Rack module ID on undo/redo.
    Tests remove/delete and recreate the module under the same ID before redo.
    An expired editor cannot write to a former module. Unchanged edits release
    their pending action; cursor locking is paired with end/destruction.
-   Name Corp/Pallet Town use fixed, module-owned arrays of lock-free atomic
    samples. Storage is allocated at construction, with no new process-time
    allocation or locking. Concurrent edits are per-sample updates, not atomic
    whole-waveform transactions; DSP sees updates at its next CV acquisition.
    Boss Fight publishes its display index atomically as well.
-   Index 8 into the old eight-frame display reproduced an ASan heap overflow.
    Parsed SVGs now have RAII deletion; missing/invalid indices return no frame.
    Tests cover empty callbacks, absent files, and 100 complete frame lifetimes.
-   PCM's Windows bitfield failure was already reproduced by 002. Three explicit
    little-endian bytes replace implementation-dependent packing. Tests cover
    size, signed boundaries, truncation, assignment, comparisons and numeric
    limits. Minimum/lowest are now -8388608, with 23 value bits. Including PCM
    before Catch2 also compiles after restricting numeric comparison templates.
    The pinned Windows ABI passes the expanded PCM tests in the follow-up runs.
-   A separately allocated BLIPBuffer reproduced an ASan over-read/write when
    advancing its tail. The old count advanced 17 elements in a 17-element
    array; the corrected move advances the remaining 16 and clears the tail.
    Expanded Rack UBSan testing also reproduced index -1 in impulse adjustment.
    Hamming-window, mirror, phase-adjustment and attenuation loops now respect
    their array bounds. All three quality levels and low-volume rescaling have
    focused sanitizer regressions. This is a local bounds correction, not an
    import of RackNES's already-present `memmove` change.
-   Infinite Stairs returned period 182 on higher voices versus 90 on voice 0
    with a mono 1 V pitch input. Mono-aware Rack reads now reuse CV on all
    channels, including forward normalling, while polyphonic reads stay separate.
-   The instrumented Infinite Stairs constructor reproduced a bool value of 2
    in Ricoh reset. Two assignment chains had omitted their final initializer.
    Reset now writes zero/false to all registers/write flags; a poisoned-storage
    construction/reset fixture guards this independently of allocator contents.

The changelog lists these behavioral increments separately from the source
moves. Focused manual notes describe mono reuse and bounded undoable editing;
manual formatting/publication remains with 004.

### Compatibility And Validation

`test/rack/fixtures/README.md` records the fixture procedure. All 18 models have
exact counts, parameter names/ranges/defaults/snap flags and custom JSON fixtures.
All 60 Super Echo presets and the registered project modules in all 24 debug
patches restore under the headless Rack engine. Historical unregistered models
and external plugins are excluded. The manifest, saved patches and presets are
unchanged; both disabled flags remain intact.

The original 2A03/106/GBS/SuperEcho audio aggregates are retained separately.
BLIP bounds corrections intentionally change the chip output. A separate copy
of `adebc068` with only the BLIP bounds corrections produced the accepted audio
fixture; the reorganized implementation agrees exactly on this machine.
Fixtures allow relative `1e-5`/absolute `1e-7` floating-point differences across
compilers. Tests use 44.1/48/96 kHz and 1/16 genuinely connected channels.
Deterministic probe-chip tests separately exercise clock quantization, CV/light
cadence, channel independence, reset, sample-rate changes, normalling and clipping.

Commands run locally with Apple Clang and Rack SDK 2.6.3 on macOS arm64:

```shell
make -j2 all test test-rack RACK_DIR=/path/to/Rack-SDK
make -j2 test-asan-ubsan
make -j2 test-rack RACK_TEST_MODE=asan-ubsan RACK_DIR=/path/to/Rack-SDK
make check-build
python3 scripts/validate.py dependencies
git diff --check
git diff --exit-code -- plugin.json presets patches/debug
```

All commands pass. The ordinary suite passes 7,111 assertions in 48 cases across 12 DSP and four
Rack binaries; the same assertions pass in the two sanitizer runs.
The Rack sanitizer run instruments the widget/common-integration tests and
Infinite Stairs included by the host fixture. Its contract executable links
ordinary production objects, so this is not a sanitizer audit of every chip or
the Rack runtime. macOS ASan does not supply LeakSanitizer accounting; resource
ownership is explicit and repeated destruction is exercised without claiming
an OS-level leak report. Existing SDK deprecation warnings remain.

### Unlocked-Desktop And CI Follow-Up

-   `49cfc083` fixes GCC's host-only API compile error by including Rack history
    declarations before `rack.hpp` in the widget test host.
-   `12e03225` fixes two Game Boy/Pallet Town bounds errors exposed during
    cross-platform verification. The final legal register `0xFF3F` indexed
    beyond a 47-byte array; its inclusive range needs 48 bytes. Polyphonic
    pitch transposed the buffer's channel and oscillator indices, so channels
    above three indexed beyond the second dimension. UBSan reproduced both.
    A new SDK-free register-boundary test and a 48-check Rack clock-index test
    guard the fixes. Existing metadata and audio fixtures remain unchanged.
-   The ordinary local run passes 7,163 assertions in 50 cases across 13 DSP
    and four Rack binaries. The targeted Rack ASan/UBSan run passes all four
    binaries, including the newly instrumented Pallet Town probe. The new
    standalone Game Boy regression also passes ASan/UBSan.
-   A broader diagnostic run additionally compiled every production object
    with ASan/UBSan through `EXTRA_FLAGS`. After the two bounds fixes it reaches
    a separate signed-negative shift in disabled `SuperSampler.cpp:124` during
    construction. This is not a passing full-plugin sanitizer audit. The
    module remains disabled; its unfinished sample encoder needs correction
    before it can pass that broader audit or be enabled. The ordinary contract
    tests and documented targeted sanitizer configuration still execute all
    registered model constructions.
-   Native Free 2.6.0 on the unlocked Mac loaded all 16 enabled models in an
    isolated profile. Inspected browser previews and live panels, including
    both blanks, the two wavetable displays and Boss Fight's algorithm display.
    The live fixture restored the project modules' saved data from the Name
    Corp and Pallet Town debug patches; their waveforms rendered correctly.
    Native select-all/delete, undo restoration and redo deletion succeeded
    without a crash. These were module-history operations, not waveform edits.
-   A separate native Infinite Stairs/VCV Scope fixture displays a live output
    at approximately 4.83 V peak-to-peak. No listening result is claimed.
    Automated pointer events did not target Rack controls correctly even with
    the desktop unlocked; keyboard input did. User patches/settings were not
    changed: all native work used a temporary profile and fixture patches.

The final [platform run](https://github.com/Kautenja/PotatoChips/actions/runs/36881102314)
for `12e03225` passes on Linux x64, macOS arm64 and Windows x64: build-rule
fixtures, plugin build, 13 DSP binaries, four Rack binaries, package validation
and artifact retention. The [instrumentation run](https://github.com/Kautenja/PotatoChips/actions/runs/36881102334)
also passes Linux DSP coverage and ASan/UBSan. Windows PCM and the original
cross-platform metadata/audio fixtures pass without relaxed tolerances.

### Remaining Verification

-   With working native pointer input, finish README's first patch, change
    pitch/duty, listen to representative 1/16-channel output, and drag-edit,
    undo/redo and remove both wavetable modules. The headless editor checks
    and native module-history checks above do not replace waveform interaction.
-   005 receives the updated runtime SVGs and ownership-safe widgets for figure
    generation. 008 remains responsible for native theme switching and preview
    verification in both modes; there were no paired themes to preserve here.


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
