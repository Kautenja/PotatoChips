# Manual Content And Publication Style

Created: 2026-10-01
Status: IN PROGRESS

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
-   The manifest's disabled SuperSynth entry points to a PDF with no manual
    source. Two enabled blank panels have no manual URL.

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

- [x] All 14 active module manuals use the shared build/style and organized
      sources, with correct manifest versions and stable release filenames.
- [ ] Quick starts, control values, patches, and troubleshooting are checked
      against code and relevant Rack behavior; discrepancies are recorded.
- [x] No swallowed TeX errors, stale source copies, undefined references,
      unresolved layout overflow, clipped figures, or unintended blank pages.
- [x] Metadata, bookmarks, links, text extraction, and citations work; all
      pages are visually reviewed after the final shared-style/figure change.
- [x] Normal builds use reviewed tracked PNGs and require no Rack session;
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

2026-10-01: implemented the content and build migration in the requested
order: Super Echo/Infinite Stairs, FM/wavetable modules, then the remaining
modules. All 14 use identity-only `manual.tex`, lower-case section sources,
one shared style and one build include. The obsolete `KautenjaDSP.sty` was
removed after checking all consumers. Manifest version remains 2.0.1.

The shared style supplies gray covers/colophons, sans-serif hierarchy,
module accents, linked contents, valid outline destinations, searchable text,
English language/title/version metadata and scoped licensing links. The new
wordmark is copied unchanged from the recorded RackNES revision; its origin
and terms are in the component inventory. Existing bibliography entries
remain intact. Useful reference labels and figures were retained; obsolete
noise-frequency calibration and ADSR schematic claims were retired with the
reason recorded in [manual/README.md](../manual/README.md).

Each manual now has a working-patch recipe, spatial reference, defaults and
CV scales, polyphony/normalling, reset/persistence behavior, variations and
troubleshooting. Constructor, processing, quantity, widget and DSP sources
were compared during writing. Important corrections and their implementation
basis are listed in the manual guide. Runtime follow-ups were recorded in
[003](003-source-organization-and-rack-integration.md#manual-audit-follow-ups-004);
004 changes no production DSP and resolves no issue automatically.

### Validation Performed

-   `make -C manual clean` and `make -C manual`: passed. The final rebuild
    contains exactly 14 stable filenames in `manual/.build/`, byte-identical
    to their individual PDFs. Successful individual output directories
    contain only `manual.pdf`.
-   `python3 manual/latex/test-build.py`: seven reported check groups passed
    in a disposable source copy. Real TeX tests cover fresh build/collection
    copying, missing inputs, undefined references, layout overflow, version
    mismatch, stale copied sources, recovery and clean. Failures preserve
    logs and remove the failed PDF and old collection copy. Clean does not
    require the previous manifest version. The disposable tree has no SDK.
-   `python3 scripts/validate.py manuals manual/.build`: passed with Poppler
    on PATH. This is the new collection path for spec 002's publication gate.
-   All PDFs were inspected with Poppler text extraction and rendered at
    `pdftoppm -scale-to 1400 -png`. All 115 pages were visually reviewed,
    including covers, contents, figures, tables, references and colophons.
    Short spill pages in Boss Fight and Mini Boss were replaced with
    intentional tuning/modulation chapters, then rebuilt and reviewed.
    The shared style's final form was reviewed across all 14 manuals.
-   A Python/pypdf pass checked identity/version/author, `en-US`, nonempty
    page text, link annotations, and every outline/named destination's page
    bounds for all 14 PDFs. No unresolved `??` references were extracted.
    Existing citations converged through BibTeX; no overfull boxes survived
    the log guard. This is not a PDF/A or accessibility certification.
-   `DYLD_LIBRARY_PATH=../.. .build/ordinary/rack/test_contract`: passed
    5,527 assertions in one test case against the existing built runtime.
    This includes restoring the 60 Super Echo presets and project modules
    in existing debug patches, saved-state/parameter contracts and selected
    audio checks. It does not execute every complete third-party patch or
    replace listening to the newly written recipes.
-   Relative documentation links, all section input paths, manifest versions,
    obsolete style consumers, copied wordmark bytes and `git diff --check`
    were checked successfully. No runtime rebuild was needed for these
    documentation/build-rule changes.

| Manual | Reviewed Pages |
| --- | --- |
| Blocks | 8 |
| Boss Fight | 10 |
| Infinite Stairs | 8 |
| Jairasullator | 9 |
| Mega Tone | 8 |
| Mini Boss | 10 |
| Name Corp Octal Wave Generator | 8 |
| Pallet Town Waves System | 8 |
| Pot Keys | 9 |
| Pulses | 8 |
| Step Saw | 8 |
| Super ADSR | 7 |
| Super Echo | 7 |
| Super VCA | 7 |

### Remaining Acceptance Work

Spec 005 has now supplied the production cover PNGs and numbered vector
control references. The interim artwork note and obsolete exports are gone.
All 14 manuals (115 pages) rebuilt and passed another complete rendered
layout review, with readable inspection of the new figures and revised
control references. README and covers consume the same native PNGs. This
satisfies the artwork/publication handoff; 005 records capture evidence.

Keep 004 IN PROGRESS for the remaining native recipe/listening acceptance.
The capture harness exercises real widgets but starts no audio device and
cannot verify the musical instructions by itself.

Native Rack 2.6.3 Pro launched, but automation could not reliably enter the
debug-patch path: text input was truncated/reset and clipboard paste timed
out. No complete native patch/listening check was obtained. The final recipe
check remains outstanding, particularly Echo timing/FIR controls, signed
ADSR/VCA behavior, FM gates/loops and editable wave banks. Do not promote the
headless fixture check to a claim of native verification.

The build/content handoff is ready for 002 to consume; publication/upload,
issue comments/closures and runtime fixes are outside this change. No commit
or push was requested for this implementation turn.
