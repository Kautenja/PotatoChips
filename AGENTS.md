# PotatoChips Agent Instructions

PotatoChips is the KautenjaDSP Potato Chips plugin for VCV Rack 2. It turns
classic sound-chip emulations into polyphonic oscillators, synthesizers,
envelopes, and effects. Preserve musical timing, chip behavior, audio
quality, and compatibility with users' saved patches.

## Start Here

Read this file and the relevant sources before editing:

-   [README.md](README.md): project overview, modules, and user manual links.
-   [plugin.json](plugin.json): plugin identity, module slugs, and version.
-   [CHANGELOG.md](CHANGELOG.md): historical behavior and compatibility fixes.
-   [LICENSE.md](LICENSE.md): source, visual-asset, and dependency terms.
-   [C++ Style Guide](docs/style-guides/cpp.md): required for C++ source,
    headers, and tests.
-   [Markdown Style Guide](docs/style-guides/markdown.md): required for
    documentation changes.
-   [Specifications](specs/README.md): durable plans and acceptance evidence.

These instructions and style guides adapt the shared Fourier and RackNES
workflow to this repository. They are self-contained; neither project is a
build dependency. Setup and architecture guidance currently live here.

## Working In This Repository

-   Inspect `git status --short` and relevant diffs before editing. Preserve
    existing user work, including changes in files needed for the task.
-   Implement one coherent requested change at a time. Read nearby code
    and tests before choosing an implementation. Avoid unrelated formatting,
    dependency upgrades, or new development infrastructure.
-   Follow the style guides and surrounding conventions. Preserve existing
    names and local conventions in adapted third-party code rather than
    reformatting whole files.
-   Keep reusable DSP independent of Rack and UI types. Put host integration
    in module code, `src/engine/`, `src/widget/`, or `src/kautenja_rack/`.
-   Preserve file-level attribution and dependency license notices,
    including the adapted chip emulators and BLIP code. Do not replace
    existing notices with a generic project header.
-   Prefer fixes in first-party integration code over edits to bundled
    libraries. Change imported code only when the requested fix requires
    it, and document the reason. Do not edit generated build products.
-   Keep source, manifest metadata, resources, presets, patches, and manuals
    aligned when behavior or module interfaces change.
-   Commit only when requested or included in the task. Push, publish, and
    release only when requested. Keep credentials and private local paths
    out of tracked files.

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
    `Doxyfile` and `doxygen/` configure generated source documentation.

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

## Planning And Completion

Small changes can be planned in chat. For substantial work that needs a
durable specification, use `specs/NNN-feature-name.md` and include the goal,
behavior examples, requirements, non-goals, testable acceptance criteria,
and exact validation commands. Create specs when useful or requested, not
as a prerequisite for every edit. Use the next unused three-digit number
across `specs/` and `specs/archive/`, starting at `001`; preserve it when
archiving. Include a creation date and an explicit status such as
`Status: PLANNED` or `Status: IN PROGRESS`.

Keep completion evidence in the owning spec when one exists: date,
decisions, commands and results, manual checks, and limitations. Mark a
verified spec `Status: COMPLETE` and move it to `specs/archive/`, creating
the directory when first needed and updating links. Mark intentionally
dropped work `Status: ABANDONED` before archiving. Do not create duplicate
completion diaries or attempt counters.

Finish with a concise summary of the changes, validation actually run, and
unresolved failures or skipped checks. Include the commit and push result
when those actions were requested.
