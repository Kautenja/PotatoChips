# Build, Tests, And Continuous Integration

Created: 2026-10-01
Status: IN PROGRESS

Replace the legacy build/test split and Travis configuration with a small,
reproducible Make and GitHub Actions workflow suited to this plugin.

## Evidence And Current Gaps

-   The root [Makefile](../Makefile) delegates to Rack's `plugin.mk`;
    the former `SConstruct` separately builds 12 DSP test executables,
    names the compiler `g++`, uses `-march=native`, and contains benchmark
    and shared-library scaffolding without corresponding current sources.
-   Catch2 is a pinned v2.13.1 submodule. There is no root `make test`,
    SDK-backed module suite, coverage job, or sanitizer job.
-   `.travis.yml` downloads Rack SDK 1.1.6, despite the Rack 2 plugin.
    `.clang_complete` contains old SDK include paths; ignore/language rules
    are broad, including exclusion of new `docs/` material except the guides.
-   Manual recipes can report success after TeX fails. There is no workflow
    to validate and attach the 14 active manual PDFs expected by the manifest.

References: Fourier's [build migration](https://github.com/Kautenja/ArhythmeticUnits-Fourier/commit/e5f1a43)
and current `mk/standalone.mk`, `mk/rack.mk`, `scripts/test-build.py`, and
test/instrumentation workflows; RackNES's
[CI introduction](https://github.com/Kautenja/RackNES/commit/100a8b3),
[trigger/artifact correction](https://github.com/Kautenja/RackNES/commit/869f40e),
and [portability fixes](https://github.com/Kautenja/RackNES/commit/aaa411b).
Fourier runs routine Linux checks with optional other platforms; RackNES
uses a three-platform matrix. Select the latter for this older codebase.

## Scope And Ownership

Own `Makefile`, new `mk/` rules, dependency/test-runner configuration,
build-validation scripts, `.github/workflows/`, `.gitignore`, and
`.gitattributes`; retire stale SCons/Travis/completion files after replacement.
Keep `test/` as the test root. [003](003-source-organization-and-rack-integration.md)
owns new behavior regression cases; this spec supplies their harness.
[004](004-manual-content-and-publication-style.md) owns PDF build rules.

## Requirements

1.  First record a clean baseline of the current Rack build and all 12 SCons
    suites, including compiler/SDK versions and failures. Preserve test
    intent and suite coverage during migration. Fix harness incompatibility
    here; record production defects for 003 instead of disabling tests.
2.  Preserve Rack's default `make`, `dist`, and `install` interface. Add
    SDK-free `make test`/`test-dsp`, `test-build`, individual suite aliases,
    and SDK-backed `test-rack`. Keep tests out of the distributed plugin.
    Isolate build configurations and header dependencies; incremental builds
    must rebuild on compiler/flag/SDK changes. Standalone-only goals must
    not include `plugin.mk`; mixed standalone/Rack goals must still do so.
3.  Adapt Fourier's pinned Catch2 amalgamated distribution and provenance
    approach, compiling the harness once per configuration. Record the
    selected upstream revision/checksums and preserve its license. Keep
    production DSP/plugin C++11; isolate Catch2's C++14 requirement to tests.
    Remove the old submodule only after equivalent suites execute. Do not
    import Fourier's paper, study runner, or benchmark dependency graph.
4.  Provide separate ordinary, coverage, and ASan/UBSan outputs under ignored
    `.build/` locations, with nonrecovering sanitizer failures and retained
    diagnostics. Report actual exercised first-party code separately from
    bundled libraries and the harness. Do not invent a coverage threshold
    or silently suppress a discovered defect.
5.  Replace Travis with Actions for pull requests and pushes to the actual
    default branch (`master` at planning time), plus manual dispatch. Check
    Linux x64, macOS arm64, and Windows x64 Rack 2 builds and available
    regression suites. Pin SDK versions and verify downloaded checksums;
    establish those pins at implementation time. Match Windows Rack's ABI
    and load its runtime for SDK-backed tests. Bound jobs and preserve
    failures through log pipelines; normal jobs have read-only permissions.
6.  After 004 supplies reliable PDF builds, add manual validation on relevant
    source, artwork, manual, capture-tool, and manifest changes. Ordinary CI
    consumes committed PNGs without a graphical session. Check every expected
    manual, versions, metadata, references, and meaningful extracted content;
    retain review artifacts. Do not use a PDF glob that passes with files
    missing. Tag builds produce artifacts; only an explicitly triggered
    release/upload path writes assets to an existing release. Validate the
    checked-out tag against the manifest and grant write access only there.
7.  Scope ignore rules to actual outputs; retain source artwork, specs, and
    license texts. Classify first-party manuals as documentation, dependencies
    as vendored, and patches/presets as nondetectable in language statistics.
    Remove `.clang_complete` after documenting current editor setup. Coordinate
    the license/package inventory with 001 and keep presets in distributions.

## Behavior Examples

-   `make test RACK_DIR=/nonexistent-sdk` executes DSP suites on a machine
    without Rack; `make all test` still requires and builds against Rack.
-   Changing a header, compiler, or sanitizer mode cannot silently reuse
    an incompatible object. An injected failing test fails CI.
-   A missing `PotKeys.pdf` or a release tag/version mismatch blocks the
    complete manual upload, rather than publishing a partial collection.

## Non-Goals

No new DSP feature, performance study, broad production warning cleanup,
mandatory graphical CI, commercial fixtures, release, or VCV submission.
Do not copy sibling version numbers, repository paths, or publication jobs
for reports that PotatoChips does not have.

## Acceptance Criteria

- [x] Baseline results and one-to-one disposition of all 12 suites recorded.
- [x] Standalone tests run without Rack; plugin/package and Rack tests build
      with the documented SDK. Production remains C++11.
- [x] Incremental, clean, mixed-goal, configuration-change, and failure
      propagation checks pass; dependencies are pinned and documented.
- [ ] Linux, macOS, and Windows CI evidence is recorded, including explicit
      unavailable checks. Instrumentation failures are resolved or remain
      visible blockers with an owning regression.
- [ ] All 14 manual PDFs and package notices are validated; disabled module
      manuals and blanks are handled explicitly, not accidentally required.
- [x] Source assets stay visible to Git, generated outputs stay ignored,
      and contributor/agent commands describe the replacement workflow.

## Validation

From the root, baseline commands available today are `make -j2` and
`scons -j2 test`. The following is the proposed public interface to implement;
it is not available in the planning revision. Rack commands require a
prepared Rack 2 tree, or an explicit `RACK_DIR` override. Instrumentation
requires Clang and matching LLVM tools:

```shell
make check-build
make -j2 test RACK_DIR=/nonexistent-sdk
make -j2 all test-rack
make -j2 test-asan-ubsan
make -j2 test-coverage
make -j2 dist
make -C manual
git diff --check
```

Include isolated build-rule regression checks for SDK-free goals, mixed
goals, header edits, changed flags, and failed executables. Validate workflow
syntax and run the actual platform jobs; a local dry run is not CI evidence.
Use a disposable PDF fixture to prove TeX failure, missing manual, and
version mismatch fail without publishing anything. Record artifact contents.

## Completion Evidence

Implemented October 1, 2026; remote CI verification is in progress. Manual
publication remains dependent on 004's reliable PDF rules.

### Baseline And Suite Disposition

Baseline `make clean && make -j2` passed on macOS ARM64, Apple Clang 21.0.0,
prepared Rack Free/SDK 2.6.0. SCons 4.4.0 `scons -k -j2 test` passed ten
suites; BLIP buffer and Sony processor did not compile because the reusable
DSP relies on an undeclared host `Exception`. In a disposable copy, supplying
only `using Exception = std::runtime_error` allowed all 12 unmodified suites
to pass. The replacement retains that test-only substitute explicitly; 003
owns removing the production dependency. No assertions were dropped.

| Suite Under `test/dsp/` | Baseline With Exception Substitute | Make/Catch2 3.16.0 |
| --- | --- | --- |
| `sony_s_dsp/test_adsr.cpp` | 1 assertion in 1 test case | Pass, unchanged |
| `sony_s_dsp/test_brr_sample_player.cpp` | 1 assertion in 1 test case | Pass, unchanged |
| `sony_s_dsp/test_common.cpp` | 20 assertions in 10 test cases | Pass, unchanged |
| `sony_s_dsp/test_gaussian_interpolation_filter.cpp` | 1 assertion in 1 test case | Pass, unchanged |
| `sony_s_dsp/test_processor.cpp` | 3 assertions in 2 test cases | Pass, unchanged |
| `test_blip_buffer.cpp` | 36 assertions in 2 test cases | Pass, unchanged |
| `test_pcm.cpp` | 6 assertions in 3 test cases | Pass, unchanged |
| `trigger/test_boolean.cpp` | 12 assertions in 4 test cases | Pass, unchanged |
| `trigger/test_divider.cpp` | 21 assertions in 2 test cases | Pass, unchanged |
| `trigger/test_hold.cpp` | 30 assertions in 1 test case | Pass, unchanged |
| `trigger/test_threshold.cpp` | 22 assertions in 5 test cases | Pass, unchanged |
| `trigger/test_zero.cpp` | 7 assertions in 3 test cases | Pass, unchanged |

### Local Verification

-   `make -j2 test RACK_DIR=/nonexistent-sdk`: all 12 suites pass with exactly
    the same case/assertion counts as the old harness. Catch2 files match
    the pinned upstream commit byte-for-byte; dependency validation passes.
-   `make -j2 all test-rack RACK_DIR=<verified-2.6.3-sdk>`: plugin and real
    Rack engine/module smoke test pass. Production compiles as C++11;
    Catch2 executables use C++14. The smoke test is not a GUI/audio check.
-   `make check-build`: three disposable-fixture tests pass, covering
    SDK-free/default/mixed goals, no-op builds, header/compiler/flag/SDK
    changes, clean behavior, failed executables, missing PDF, metadata-tool
    failure, wrong PDF version, unresolved references, and tag mismatch.
    They caught and fixed same-second stamp changes with macOS Make 3.81.
-   `make test-asan-ubsan`: all 12 suites pass with nonrecovering diagnostics;
    `make test-coverage`: all pass and separate first-party/imported/harness
    reports are generated. First-party mapped line coverage is 38% (76/200),
    not whole-plugin coverage. No threshold or suppression was introduced.
-   `make -j2 dist` and `scripts/validate.py package`: real macOS ARM64 archive
    passes exact manifest/license/resource/preset content validation and
    excludes development dependencies/tests. Source license text unchanged.
-   Downloaded all three official Rack SDK 2.6.3 archives and the Windows
    Free runtime, verified the recorded SHA-256 hashes. Actionlint 1.7.7
    accepts both workflows. Ordinary jobs are read-only; there is no release
    publication action. Tag builds only retain workflow artifacts.

### Remaining Verification And Handoffs

Record the actual remote platform/instrumentation jobs here after pushing.
Spec 004 owns TeX failure propagation and actual 14-PDF builds/render review.
The manual validator is implemented and its negative fixtures pass, but no
real PDF collection, TeX failure fixture, or upload workflow is claimed to
pass yet. Once 004 supplies reliable rules, add path-filtered manual CI and
an explicit-dispatch-only existing-release upload job with scoped write
permissions. Ordinary CI must consume committed PNGs. This dependency keeps
the PDF acceptance item open; the plugin build/test migration is implemented.
