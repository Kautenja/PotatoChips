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
| Plugin build | Git, GNU Make, C++11 compiler, `jq`, and a prepared Rack 2 SDK/tree for the target platform and architecture |
| Standalone tests | SCons, a C++11 compiler available as `g++`, and the pinned Catch2 v2 submodule |
| Package | Build tools plus `tar`, `zstd`, and the platform tools invoked by Rack's `plugin.mk` |
| Native verification | A matching Rack 2 runtime and graphical desktop |
| Manuals | Make, `pdflatex`, bibliography tools where used, and the packages listed in the LaTeX sources |

Clone from your projects directory, then run subsequent commands from the
repository root unless a command explicitly changes directories:

```shell
git clone --recurse-submodules https://github.com/Kautenja/PotatoChips.git
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
    `src/kautenja_rack/` contains Rack helpers and parameter quantities.
-   `res/` contains runtime panels, controls, and other assets. `presets/`
    contains saved module presets; `patches/` includes examples and debug
    patches for manual checks.
-   `test/` contains standalone DSP tests built by `SConstruct` using the
    pinned Catch2 submodule in `dep/Catch2/`.
-   `manual/` contains per-module LaTeX manuals, figures, and shared style.

Maintain source API documentation in code comments. This project does not
build generated API documentation.

Do not renumber existing Rack parameter, port, or light IDs, rename module
slugs, or change saved JSON meanings without an intentional compatibility
plan and verification with existing patches. Display names and slugs can
differ: preserve historical identifiers such as `106`, `2612`, and `2A03`.
SuperSampler and SuperSynth are marked disabled in `plugin.json`; their
source and registration do not imply they are released modules.

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

Standalone DSP tests require SCons, a C++11 compiler available as `g++`,
and the pinned Catch2 v2 submodule. Initialize that dependency when needed,
then build and run the suites:

```shell
git submodule update --init --recursive
scons -j2 test
```

For a focused suite, SCons aliases use the test source path, including its
`.cpp` suffix. For example:

```shell
scons test/dsp/trigger/test_divider.cpp
```

There is no root `make test` target. Do not substitute Fourier's Catch2 v3
targets or RackNES's `tests/` commands. The legacy `.travis.yml` downloads a
Rack 1 SDK; it is not current Rack 2 validation evidence. Report missing
prerequisites or stale harness failures explicitly without upgrading the
toolchain as part of an unrelated change.

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

Edit manual sources in `manual/<Module>/manual.tex` and the corresponding
`img/` assets. The shared stylesheet is `manual/KautenjaDSP.sty`; runtime
panel assets live in `res/`. Preserve source/export relationships and
review figures against the actual module when controls or layout change.

Manual builds require `pdflatex` and the packages used by the affected
manual; manuals with bibliographies also invoke `bibtex`. For example,
build Super Echo or the full manual collection from the repository root:

```shell
make -C manual/SuperEcho
make -C manual
```

The per-module output is `manual/<Module>/build/manual.pdf`; the collection
target copies PDFs into `manual/build/`. Existing make recipes filter TeX
output and can mask failures, so verify that the expected PDF exists and
inspect the rendered result. Keep intermediate files and compiled manuals
in ignored build directories. Do not import sibling projects' screenshot
or whitepaper commands without implementing and validating that workflow.

The native capture tooling and Make-based test migration described in specs
002/005 are planned. Until they land, use the commands above and existing
source artwork. Do not label an SVG export as a production screenshot.

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
    not verify their contents.
5.  Build and inspect all 14 manuals. The collection writes to
    `manual/build/`; preserve the PDF names used by `plugin.json` and README:
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
