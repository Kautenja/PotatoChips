# Build, Tests, And Continuous Integration

Created: 2026-10-01
Status: PLANNED

Replace the legacy build/test split and Travis configuration with a small,
reproducible Make and GitHub Actions workflow suited to this plugin.

## Evidence And Current Gaps

-   The root [Makefile](../Makefile) delegates to Rack's `plugin.mk`;
    [SConstruct](../SConstruct) separately builds 12 DSP test executables,
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

- [ ] Baseline results and one-to-one disposition of all 12 suites recorded.
- [ ] Standalone tests run without Rack; plugin/package and Rack tests build
      with the documented SDK. Production remains C++11.
- [ ] Incremental, clean, mixed-goal, configuration-change, and failure
      propagation checks pass; dependencies are pinned and documented.
- [ ] Linux, macOS, and Windows CI evidence is recorded, including explicit
      unavailable checks. Instrumentation failures are resolved or remain
      visible blockers with an owning regression.
- [ ] All 14 manual PDFs and package notices are validated; disabled module
      manuals and blanks are handled explicitly, not accidentally required.
- [ ] Source assets stay visible to Git, generated outputs stay ignored,
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

Only the existing command dry runs were checked during agent setup. No
baseline tests, replacement targets, or CI runs have been completed here.
