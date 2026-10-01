# Manual Content And Publication Style

Created: 2026-10-01
Status: PLANNED

Give all active modules practical, consistently styled user manuals with
shared sources and reliable builds, following the siblings' publication
improvements without relocating the whole documentation tree.

## Evidence And Current Gaps

-   There are 14 `manual/<Module>/manual.tex` sources with largely repeated
    makefiles and one `manual/KautenjaDSP.sty`. The stylesheet hardcodes
    version `2.0.1`; individual documents mix operating reference, chip
    history, large paragraphs, and manually maintained panel exports.
-   Per-module builds copy source and assets into `build/`, enable shell
    escape, run fixed TeX passes, and filter errors through `grep ... || true`.
    Stale source copies and swallowed failures undermine successful-build
    claims. Bibliography handling is duplicated across manuals.
-   Concrete writing checks are needed: Super Echo's input section says
    "Super VCA", and its delay discussion includes 31 sixteen-millisecond
    levels and a 512 ms maximum. Resolve the timing from actual buffer/rate
    behavior instead of repeating either claim without verification.
-   The manifest's disabled SuperSampler/SuperSynth entries point to PDFs
    with no manual source. Two enabled blank panels have no manual URL.

References: RackNES's [manual rewrite/build migration](https://github.com/Kautenja/RackNES/commit/d25ece1),
current `manual/README.md`, `manual/latex/arhythmetic-manual.sty`, and
`manual/latex/manual.mk`; Fourier's
[manual restructuring](https://github.com/Kautenja/ArhythmeticUnits-Fourier/commit/e5f1a43),
`docs/latex/arhythmetic-manual.sty`, `docs/latex/publication.mk`, and
`docs/manual-*/sections/`. The sources show shared typography, clear
navigation, practical examples, and code-checked reference values.

## Scope And Ownership

Own manual text, section structure, shared style, metadata, and PDF build
rules. Keep `manual/` and current module directory names: moving 14 manuals
to Fourier's `docs/manual-*` layout adds little value. Panel diagrams and
screenshots belong to [005](005-production-panel-captures-and-figures.md),
workflow plumbing to [002](002-build-tests-and-ci.md), and runtime fixes
discovered during writing to [003](003-source-organization-and-rack-integration.md).

## Requirements

1.  Create `manual/README.md`, `manual/latex/arhythmetic-manual.sty`, and
    shared `manual/latex/manual.mk`. Keep each `manual.tex` as identity,
    explicit version, and section ordering, with lower-case `sections/`
    files for the content. Centralize reusable typography, metadata,
    navigation, covers, and colophons; preserve valid citation/reference
    labels and update every moved path.
2.  Adapt the sibling manual design: legible sans-serif type, clear heading
    hierarchy, restrained module accents, gray branded covers, useful
    headers/footers, linked contents, PDF outline, searchable/copyable text,
    language/title/version metadata, and correctly scoped licensing links.
    Set PotatoChips-specific identity and terms rather than copying another
    project's title, author years, repository URL, or subject keywords.
3.  Use `latexmk` with errors propagated, shell escape disabled, source-relative
    inputs, and separate `.build/` outputs. References and bibliographies must
    converge; retain failure logs, clean auxiliaries after successful builds,
    and reject legacy build directories that contain stale source copies.
    Root `make -C manual` must produce all 14 stable release filenames in
    `manual/.build/`. Allow individual manual builds and clean targets without
    requiring Rack, screenshot generation, or a graphical session.
4.  Rewrite each guide as an operating manual: purpose, a first working patch,
    spatial control reference, practical patch examples, CV/range/default
    details, polyphony and normalling, relevant reset/persistence/context
    menu behavior, and troubleshooting. Explain unusual chip behavior only
    where it helps a musician make a choice. Follow the existing Markdown
    guide's prose principles for LaTeX too; preserve technical substance.
5.  Verify claims against each constructor, parameter quantity, processing
    function, DSP implementation, and preset. Pay particular attention to
    Super Echo sample-rate/delay/filter behavior; Super ADSR/Super VCA integer
    levels and response; Boss Fight/Mini Boss algorithms/envelopes; Namco and
    Game Boy wavetable editing; sync/gate thresholds; and output normalling.
    Use existing debug patches to verify examples. Record disagreements and
    route reproducible code defects to 003 instead of silently designing new
    behavior in prose.
6.  Migrate in reviewable groups: first Super Echo and Infinite Stairs to
    exercise effect and oscillator needs; then FM/wavetable modules; then
    the remaining modules. Review every manual after a shared-style change.
    Integrate 005's reviewed cover PNGs and vector control references before
    final acceptance; existing artwork may serve during the first migration.

## Behavior Examples

-   A reader can patch Super Echo, distinguish dry/mix/feedback controls,
    and understand its actual delay timing at the host sample rate.
-   The same control name and range appear in Rack help, the manual table,
    and the numbered diagram. Hardware history does not replace operating
    instructions.
-   Breaking a TeX input makes `make -C manual` fail; a successful fresh
    build contains all 14 current PDFs without requiring native captures.

## Non-Goals

No whitepaper, paper font package stack, new manuals for disabled modules,
invented blank-panel manuals, DSP redesign, or PDF/A/PDF/UA certification
claim. Release upload belongs to 002. Do not replace chip-specific content
with identical boilerplate across every manual.

## Acceptance Criteria

- [ ] All 14 active module manuals use the shared build/style and organized
      sources, with correct manifest versions and stable release filenames.
- [ ] Quick starts, control values, patches, and troubleshooting are checked
      against code and relevant Rack behavior; discrepancies are recorded.
- [ ] No swallowed TeX errors, stale source copies, undefined references,
      unresolved layout overflow, clipped figures, or unintended blank pages.
- [ ] Metadata, bookmarks, links, text extraction, and citations work; all
      pages are visually reviewed after the final shared-style/figure change.
- [ ] Normal builds use reviewed tracked PNGs and require no Rack session;
      final manual inventory is available to 002's completeness check.

## Validation

From the repository root, after the shared rules are implemented, with
LaTeX, `latexmk`, the documented packages, and Poppler installed:

```shell
make -C manual clean
make -C manual
pdfinfo manual/SuperEcho/.build/manual.pdf
pdftotext -layout manual/SuperEcho/.build/manual.pdf /tmp/potatochips-super-echo.txt
mkdir -p /tmp/potatochips-manual-review
pdftoppm -scale-to 1400 -png manual/SuperEcho/.build/manual.pdf /tmp/potatochips-manual-review/super-echo
git diff --check
```

Repeat PDF/text/render checks for every module, inspect every page, and
compare the collection against the explicit list in 005. In a disposable
source copy, break a TeX input to prove failure propagation. Check links,
outline, metadata/version, and sample patches separately. Report unavailable
manual Rack checks and unresolved claims; do not mark them verified.

## Completion Evidence

Content audit, builds, rendered-page review, and patch checks are pending.
