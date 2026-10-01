# PotatoChips Agent Instructions

PotatoChips is the Arhythmetic Units Potato Chips plugin for VCV Rack 2. It turns
classic sound-chip emulations into polyphonic oscillators, synthesizers,
envelopes, and effects. Preserve musical timing, chip behavior, audio
quality, and compatibility with users' saved patches.

## Start Here

Read this file and the relevant sources before editing:

-   [README.md](README.md): project overview, modules, and user manual links.
-   [plugin.json](plugin.json): plugin identity, module slugs, and version.
-   [CHANGELOG.md](CHANGELOG.md): historical behavior and compatibility fixes.
-   [LICENSING.md](LICENSING.md): source, visual-asset, and dependency terms.
-   [C++ Style Guide](docs/style-guides/cpp.md): required for C++ source,
    headers, and tests.
-   [Markdown Style Guide](docs/style-guides/markdown.md): required for
    documentation changes.
-   [CONTRIBUTING.md](CONTRIBUTING.md): setup, architecture, compatibility,
    real-time constraints, validation, manuals, and release preparation.
-   [Specifications](specs/README.md): durable plans and acceptance evidence.

These instructions and style guides adapt the shared Fourier and RackNES
workflow to this repository. They are self-contained; neither project is a
build dependency. Shared development guidance lives in CONTRIBUTING.md; read
the relevant sections before changing code or build/publication behavior.

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
    in module code, `src/engine/`, `src/widget/`, or `src/rack_extensions/`.
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
