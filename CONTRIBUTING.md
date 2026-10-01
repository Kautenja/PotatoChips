# Contributing To Potato Chips

Use this guide to build and validate changes to the Arhythmetic Units
Potato Chips plugin. Read the [README](README.md), [licensing scope](LICENSING.md),
and relevant [C++](docs/style-guides/cpp.md) or
[Markdown](docs/style-guides/markdown.md) style guide first. Bug reports and
feature proposals should follow [SUPPORT.md](SUPPORT.md). Agent-specific
workflow lives in [AGENTS.md](AGENTS.md).

## Set Up Your Environment

| Work | Prerequisites |
| --- | --- |
| Plugin build | Git, Python 3.9+, GNU Make, C++11 compiler, `jq`, and a prepared Rack 2 SDK/tree for the target platform and architecture |
| Standalone tests | GNU Make, Python 3.9+, and a C++14 compiler; Catch2 3.16.0 is vendored |
| Package | Build tools plus `tar`, `zstd`, and the platform tools invoked by Rack's `plugin.mk` |
| Native verification | A matching Rack 2 runtime and graphical desktop |
| Manuals | Make, `pdflatex`, bibliography tools where used, and the packages listed in the LaTeX sources |

Clone from your projects directory, then run subsequent commands from the
repository root unless a command explicitly changes directories:

```shell
git clone https://github.com/Kautenja/PotatoChips.git
cd PotatoChips
```

Use the Rack 2 SDK for your platform, or a prepared Rack source checkout.
The installed Rack application alone is not a complete SDK. Markdown edits
do not require compiling the plugin or installing TeX.

## Architecture And Compatibility

-   `src/plugin.cpp` and `src/plugin.hpp` register models and shared Rack
    declarations. Module `.cpp` files in `src/` contain processing and UI.
-   `src/dsp/` contains chip emulators, BLIP buffering, PCM conversion,
    math, and triggers. The Sony S-DSP, Yamaha YM2612, and Mutable Instruments
    Edges helpers have their own subdirectories.
-   `src/engine/chip_module.hpp` provides the common polyphonic chip module,
    with per-channel emulators and buffers, CV/light dividers, voltage
    conversion, output normalling, and clipping. Other modules have their
    own processing paths; inspect the affected module's implementation.
-   `src/widget/` contains display and wavetable editing widgets;
    `src/rack_extensions/` contains Rack helpers, parameter quantities, and
    module-owned wavetable storage. Generic math is in `dsp/math.hpp`, constants
    in `dsp/constants.hpp`, voltage conversions in `dsp/eurorack.hpp`, and
    triggers in the five `dsp/trigger_*.hpp` headers.
-   `res/` contains runtime panels, controls, and other assets. `presets/`
    contains saved module presets; `patches/` includes examples and debug
    patches for manual checks.
-   `test/dsp/` contains SDK-free suites; `test/rack/` contains headless
    SDK-backed suites. Make rules live in `mk/`, and Catch2 is vendored in
    `dep/catch2-v3/`. Configurations and reports live under `.build/`.
-   `manual/` contains per-module LaTeX manuals, figures, and shared style.

Maintain source API documentation in code comments. This project does not
build generated API documentation.

Do not renumber existing Rack parameter, port, or light IDs, rename module
slugs, or change saved JSON meanings without an intentional compatibility
plan and verification with existing patches. Display names and slugs can
differ: preserve historical identifiers such as `106`, `2612`, and `2A03`.
SuperSynth is marked disabled in `plugin.json`; its source and registration
do not imply it is a released module.

## Correctness And Real-Time Behavior

Chip emulation, CV processing, and audio rendering run on Rack's audio
engine thread. Keep per-sample work bounded and avoid adding allocation,
blocking, file I/O, logging, or drawing to that path. Document existing
limitations touched by a change rather than claiming the implementation
is fully real-time safe.

Preserve or explicitly test changes to:

-   Chip clocking, register semantics, oscillator tuning, and BLIP scheduling.
-   Polyphonic channel independence, disconnected inputs, output normalling,
    audio voltage scaling, and clipping.
-   CV/light divider cadence, trigger ordering, reset behavior, and host
    sample-rate changes.
-   PCM and BRR formats, envelope stages, interpolation, echo feedback,
    integer widths, saturation, and buffer bounds where applicable.

Review engine/UI handoffs when changing wavetable editing or shared display
state. A shared flag or pointer is not a synchronization contract. Check
Jansson ownership and error paths when changing saved patch data.

Behavior changes need evidence at the appropriate seam. Reproduce bugs
when practical and add focused deterministic regression tests where useful.
Do not weaken assertions or hide failures. Performance claims require
comparable before/after workloads, compiler settings, repeated measurements,
and a meaningful effect; a successful build is not evidence of a speedup.

## Development And Validation

Run commands from the repository root unless a different directory is
specified. The plugin uses the Rack 2 build system and a C++11 compiler.
The root `Makefile` defaults `RACK_DIR` to `../..`, suitable for a checkout
inside Rack's `plugins/` directory. With a prepared Rack tree:

```shell
make -j2
```

For a separate Rack 2 SDK, replace the example path with the actual SDK
directory containing `plugin.mk`:

```shell
make -j2 RACK_DIR=/path/to/Rack-SDK
```

Standalone DSP tests use C++14 and the pinned Catch2 3.16.0 amalgamation.
Production headers and plugin sources remain C++11. No SDK or submodules
are needed for the following commands:

```shell
make -j2 test RACK_DIR=/nonexistent-sdk
make -j2 test-build
make test/dsp/trigger/test_divider
make check-build
python3 scripts/validate.py dependencies
```

`test` and `test-dsp` run all 13 DSP suites; `test-build` only compiles them.
Individual aliases omit `.cpp`. `TEST_ARGS` passes Catch2 filters/options.
`CXX`, `CPPFLAGS`, `CXXFLAGS`, and `LDFLAGS` configure standalone builds;
Rack's own flags are isolated from them even in mixed invocations.
`make all test-rack` requires the SDK. Its four headless suites cover all 17
registered models, parameter/default/custom-JSON contracts, existing presets
and project modules in debug patches, representative audio, common chip
processing, and editor/display ownership. Fixture provenance and tolerances
are documented in `test/rack/fixtures/README.md`. These checks do not replace
graphical or audible Rack checks.

DSP exceptions use `std::runtime_error` without a Rack substitute. PCM's
numeric comparison overloads permit either Catch2 include order.

Ordinary DSP, Rack, and plugin builds occupy separate directories. Dependency
files track headers, and configuration stamps track compiler identity,
flags, SDK path/build rules/library bytes. `make clean` needs no SDK and
removes `.build/`, plugin binaries, and packages. For plugin-specific flags,
use the SDK's `EXTRA_CXXFLAGS`/`EXTRA_LDFLAGS` instead of replacing its defaults.

Instrumentation runs all DSP suites, retains each suite's output even when
another fails, and returns failure for any assertion or sanitizer diagnostic:

```shell
make test-asan-ubsan
make test-coverage
make -j2 test-rack RACK_TEST_MODE=asan-ubsan RACK_DIR=/path/to/Rack-SDK
```

These require Clang. Set `INSTRUMENT_CXX`, `LLVM_COV`, and `LLVM_PROFDATA`
for a matching LLVM installation; on macOS LLVM tools default to `xcrun`.
Reports live in `.build/<mode>/reports/`, separately from ordinary tests.
Rack sanitizer binaries live in `.build/asan-ubsan/rack/`. Widget and common
integration sources are instrumented; the contract executable links the same
ordinary production objects used by the plugin. Neither the Rack SDK library
nor every chip implementation is instrumented by that contract run.
Coverage reports distinguish first-party utilities, imported DSP, and test
harnesses. Only code mapped by these suites is measured; many chip/audio
paths are unexercised. There is no coverage threshold. ASan/UBSan failures
are nonrecovering and must not be hidden with suppression flags.

### CI And Editors

[Build CI](.github/workflows/build.yml) runs Linux x64, macOS arm64, and
Windows x64 on PRs, `master`/`v2.0.2` pushes, version tags, and manual dispatch.
Rack SDK 2.6.3 downloads have checked SHA-256 pins. Windows uses MinGW64/MSVCRT,
and extracts a separately verified Rack Free runtime for headless tests.
It puts that DLL after the compiler runtime on PATH. Normal jobs have only
read permissions and upload workflow artifacts, never release assets.
Tags must match the manifest (an optional leading `v` is accepted).
The Windows PCM size regression currently fails (`int24_t` is 4 bytes,
expected 3); spec 003 owns the production fix. CI keeps that failure visible
while still running the other suites and retaining packages for inspection.
Workflow artifacts from a failing run are not validated releases.
[Instrumentation CI](.github/workflows/instrumentation.yml) uses Clang/LLVM 18.

For clangd or another C++ editor, generate a compilation database from a
clean build using your preferred tool (for example Bear), or configure the
actual SDK's `include/` and `dep/include/` plus `src/`. Use C++11 for production
and C++14 for test files; old hardcoded dependency paths are no longer valid.

For DSP changes, run relevant suites; for Rack integration, also build the
plugin and check the affected module in Rack when available. Use relevant
`patches/debug/` fixtures and exercise mono/polyphony, reset, sample-rate
changes, and saved patch reloads as appropriate. Existing test coverage
does not cover every chip or module.

Documentation-only edits need link, path, command, and diff checks rather
than a mandatory C++ build. Run `git diff --check` and review the complete
diff, including new files. Distinguish passing DSP tests, a successful Rack
build, and an actual manual Rack session in completion reports.

## Manuals And Assets

Edit `manual/<Module>/sections/*.tex`; keep identity, explicit manifest
version and section order in each `manual.tex`. Shared typography and build
rules live in `manual/latex/`. See [the manual guide](manual/README.md) for
prerequisites, the 14-file inventory, source audit and rendered-page review.

```shell
make -C manual
make -C manual/SuperEcho
python3 manual/latex/test-build.py
python3 scripts/validate.py manuals manual/.build
```

The individual output is `manual/<Module>/.build/manual.pdf`; the collection
is `manual/.build/`. Builds use `latexmk` with shell escape disabled, reject
copied sources/version drift and fail on unresolved references or overflow.
The validator needs Poppler on PATH. Generated outputs remain ignored.

Covers use committed production captures and panel maps use vector guides.
Refresh both after UI changes with the optional [capture tools](tools/capture/README.md).
Spec 002 owns later manual CI and authorized upload to an existing release.
Normal PDF builds need no Rack SDK, OpenGL session, Pillow or capture tool.

## Prepare A Release

Release preparation is separate from publishing and from submitting to the
VCV Library. Work through these checks for an explicitly requested release:

1.  Review the active module inventory, compatibility, open provenance gaps
    in `docs/licenses/THIRD-PARTY.txt`, and outstanding acceptance evidence.
    Keep disabled modules disabled unless their implementation is complete.
2.  Set the intended manifest version and match the release tag exactly,
    allowing a leading `v` in the tag. Replace unreleased notes only with
    changes actually included; historical `TBD` plans are not releases.
3.  Build/test each supported platform and perform native Rack checks.
    Run `make -j2 dist` with the same `RACK_DIR` used to build. Packages are
    `dist/KautenjaDSP-PotatoChips-<version>-<os>-<cpu>.vcvplugin`.
4.  Inspect the real package: Rack 2's `.vcvplugin` is a Zstandard-compressed
    tar archive, despite the old makefile comment calling it ZIP. For one
    specific artifact, substitute its path in this command:

    ```shell
    zstd -dc dist/KautenjaDSP-PotatoChips-<version>-<os>-<cpu>.vcvplugin | tar -tf -
    ```

    Check the plugin binary, manifest, `res/`, presets, `LICENSE`,
    `LICENSING.md`, and every file under `docs/licenses/`. Compare the
    extracted notice bytes with the source; a file-list check alone does
    not verify their contents. Automate this with
    `python3 scripts/validate.py package <archive.vcvplugin>`; it also checks
    every resource/preset and rejects packaged test/dependency code.
5.  Build and inspect all 14 manuals. The collection writes to
    `manual/.build/`; preserve the PDF names used by `plugin.json` and README:
    `Blocks.pdf`, `InfiniteStairs.pdf`, `StepSaw.pdf`, `Pulses.pdf`,
    `Jairasullator.pdf`, `PotKeys.pdf`, `MegaTone.pdf`, `BossFight.pdf`,
    `MiniBoss.pdf`, `NameCorpOctalWaveGenerator.pdf`,
    `PalletTownWavesSystem.pdf`, `SuperADSR.pdf`, `SuperEcho.pdf`, and
    `SuperVCA.pdf`. PDF assets are published separately from plugin packages.
6.  Publish only when authorized. Confirm uploaded release assets and links;
    a checkout, tag, GitHub release, and PDF upload are different outputs.
7.  Submit the release separately through the
    [VCV Library workflow](https://github.com/VCVRack/library#updating-your-plugin).
    A GitHub release does not mean the Library build is already available.

## Submit A Change

Use a focused branch and describe the problem, resulting behavior, and
validation in the pull request. Include a small patch/event fixture for
behavior changes, the Rack/plugin/OS versions used, compatibility effects,
and any skipped checks. Preserve attribution; new dependencies and artwork
need a recorded origin and applicable notices. Keep generated binaries,
manual PDFs, temporary renders, and local device settings out of commits.
