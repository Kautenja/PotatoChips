# Licensing And Project Documentation

Created: 2026-10-01
Status: IN PROGRESS

Make the repository's public information, attribution, and contributor
entry points as clear as Fourier's and RackNES's, using PotatoChips's
actual module inventory and existing license terms.

## Evidence And Current Gaps

-   The former `LICENSE.md` (now [LICENSING.md](../LICENSING.md)) mixed source and artwork scope and linked to
    `LICENSE-dist.txt`, which is absent. `LICENSE-GPLv3.txt` contains the
    software license text; the package currently includes `LICENSE*` only.
-   The short Blargg attribution does not inventory the adapted chip code.
    Headers credit Shay Green, Emilie Gillet, Brad Martin, Jarek Burczynski,
    Tatsuyuki Satoh, Nicola Salmoria, and others. Tests have MIT notices and
    Catch2 has separate terms. A blanket replacement would lose information.
-   [README.md](../README.md) leads with a promotional video quotation and a
    Travis badge, followed by a long module catalog. It lacks a concise
    installation/first-patch path and contributor/support entry points.
    Commented material still contains names and links for unfinished modules.
-   `CONTRIBUTING.md` and `SUPPORT.md` do not exist. Shared setup and
    architecture guidance currently live in [AGENTS.md](../AGENTS.md).
-   The manifest remains KautenjaDSP-branded and omits the fuller metadata
    used by the sibling projects. The changelog mixes releases with `TBD`
    feature plans, and its Rack 2 entry has a date inconsistent with the
    corresponding 2022 history; reconcile from evidence rather than guess.

References: RackNES [license scope](https://github.com/Kautenja/RackNES/commit/a8ad865),
[provenance](https://github.com/Kautenja/RackNES/commit/70f594d),
[final license layout](https://github.com/Kautenja/RackNES/commit/f52f296),
[README onboarding](https://github.com/Kautenja/RackNES/commit/b493f4d), and
[contribution templates](https://github.com/Kautenja/RackNES/commit/e69e8a7).
Fourier's [modernization](https://github.com/Kautenja/ArhythmeticUnits-Fourier/commit/e5f1a43)
provides the matching `LICENSE`, `LICENSING.md`, README, and contributor layout.

## Scope And Ownership

Own root license/documentation files, `docs/licenses/`, GitHub issue/PR
templates, and descriptive manifest metadata. Coordinate package inclusion
with [002](002-build-tests-and-ci.md), runtime branding with
[003](003-source-organization-and-rack-integration.md), provenance for theme assets
with [008](008-native-light-and-dark-themes.md), and final manual
and image links with [004](004-manual-content-and-publication-style.md) and
[005](005-production-panel-captures-and-figures.md).

## Requirements

1.  Put the unmodified GPL text at `LICENSE` and the scope explanation at
    `LICENSING.md`, following the siblings' final arrangement. Preserve the
    existing source/artwork split and individual file notices. Add the
    applicable full dependency/artwork texts and a component inventory in
    `docs/licenses/THIRD-PARTY.txt` with paths, origins, versions/revisions
    where known, and recorded uncertainty where provenance is incomplete.
    Distinguish code shipped in the plugin from test-only Catch2. Resolve
    inconsistencies from source/history/upstream evidence; this task does
    not authorize silently changing third-party terms.
2.  Present Potato Chips as part of Arhythmetic Units, as RackNES now does.
    Preserve Christian Kauten and upstream attribution, the plugin slug
    `KautenjaDSP-PotatoChips`, all 18 module slugs, and all disabled flags.
    Keep the current repository and VCV Library URLs unless there is actual
    migration evidence. Do not fabricate an email, publication, or release.
3.  Rewrite README around a short musical purpose, installation, a working
    first patch, a compact module/chip/use/manual table, selected examples,
    and support/development links. Cover the 14 active sound modules and
    explain the two blanks without advertising unfinished modules. Use
    plain prose and the shared writing conventions; remove stale promotional
    filler and CI claims. Use explicit source-license labeling and current
    workflow badges once 002 exists.
4.  Add `CONTRIBUTING.md` for environment setup, source map, build boundaries,
    tests, manual figures, and release preparation. Move the shared content
    there from AGENTS and keep agent workflow concise. Update the Markdown
    guide's document ownership links accordingly. Add `SUPPORT.md` and align
    templates with actionable Rack/plugin/OS versions, reproduction patches,
    relevant validation, and compatibility notes.
5.  Reconcile changelog history without asserting that `TBD` modules shipped.
    Record newly implemented maintenance in an unreleased section as it
    lands. Keep checkout, GitHub release, PDF, and VCV Library availability
    distinct. Document release asset names, tag/manifest agreement, and the
    separate VCV submission step; do not publish as part of this spec.

## Behavior Examples

-   A new user can find Infinite Stairs, connect its output, set a useful
    pitch/level, and reach the matching manual without reading build steps.
-   A contributor can locate the exact test command and audio-thread boundary
    from `CONTRIBUTING.md`; an agent reads workflow rules in AGENTS.
-   A recipient of a plugin archive can read the software license, visual
    terms, and relevant bundled-code notices without cloning this repository.

## Non-Goals

No relicensing, bulk copyright-year rewrite, repository rename, new module,
DSP change, manuscript, version bump, release publication, or VCV submission.
Runtime logo replacement belongs to 003; manual layout belongs to 004.

## Acceptance Criteria

- [x] Every imported code family and shipped asset family has an evidenced
      scope/notice entry; missing provenance is explicitly identified.
- [x] The root license text is unchanged in substance and the broken
      `LICENSE-dist.txt` reference is gone. All local links resolve.
- [x] README and manifest describe the actual active module inventory;
      slugs and disabled flags match the baseline exactly.
- [x] Contributor, agent, style, and support guidance have clear ownership
      and no obsolete SCons/Travis instructions after 002 is complete.
- [x] All 14 active manual links retain their release filenames. README
      uses reviewed screenshots when 005 is complete.
- [x] The built archive contains the required license texts and notices.

## Validation

Run from the repository root. For packaging, use the prepared Rack 2 tree
or pass `RACK_DIR` as documented by 002:

```shell
git diff --check
python3 -m json.tool plugin.json > /dev/null
git diff d1821c9f -- plugin.json
make -j2 dist
```

Compare `LICENSE` byte-for-byte with `git show d1821c9f:LICENSE-GPLv3.txt`.
Compare manifest slug/disabled maps programmatically against that revision.
Inspect the actual generated `.vcvplugin` archive using the Rack package
format, checking its file list and notice contents. Check Markdown links,
rendered README headings/table/images, and the first-patch instructions in
Rack. Record remote link/license-detection checks separately when performed.

## Completion Evidence

Implemented on October 1, 2026. Status remains `IN PROGRESS` pending the
native first-patch check below; archive after that verification succeeds.

-   Added unchanged root GPL text, licensing scope, complete applicable
    notices, and an evidence-based component/artwork inventory. Preserved
    source notices and recorded the historical YM2612 MAME discrepancy,
    TercerBrazo lineage, and incomplete artwork provenance. These records
    do not establish new rights or resolve the underlying permission gaps.
-   Rewrote README, added contributor/support guides, shortened AGENTS,
    aligned templates/style-guide links, updated descriptive metadata,
    and reconciled historical/unreleased changelog entries.
-   `make -j2 dist` passed using the prepared Rack 2.6.0 macOS ARM64 tree.
    This packaged the existing plugin binary; no DSP source changed and
    no clean rebuild or standalone DSP test run was required/performed.
-   Decompressed the real Zstandard/tar `.vcvplugin`: 210 entries, plugin
    binary and manifest present, all eight license/scope/notice files
    byte-identical to their sources, and no packaged Catch2 code.
-   Programmatic comparison with `d1821c9f` passed: exact GPL bytes, plugin
    slug/version unchanged, and all 18 module objects unchanged, including
    every slug, disabled flag, and manual URL. Catch2 license text matches
    the pinned submodule exactly. JSON parsing and `git diff --check` passed.
-   Checked all 151 local links/heading anchors across 23 first-party
    Markdown files. Rendered README through Pandoc's GFM reader and reviewed
    the complete browser rendering: one title, ordered onboarding steps,
    all 14 module rows, blank-panel explanation, and support/license links.
-   Remote check: GitHub's latest release API returned tag `2.0.0` with
    all 14 referenced PDF asset names. This does not imply source `2.0.1`
    was released. GitHub's license detection was not checked.
-   Reviewed Infinite Stairs' source for default C4 pitch, FM zero, volume
    10, output names, free-running behavior, and output normalling. Native
    verification was attempted in temporary isolated Rack profiles. The
    installed Pro build required activation; the local Free 2.6.0 build
    launched, but the locked macOS desktop prevented GUI/audio checks.
    No successful native patch or listening check is claimed.

### Remaining Verification And Handoffs

- [ ] On an unlocked desktop, follow README's Infinite Stairs/Audio steps
      in Rack, verify pitch/duty changes and the named ports/defaults, then
      record the result and archive this spec with updated cross-references.

The checked acceptance items describe the implemented repository scope.
Spec 002 has updated CONTRIBUTING/style-guide commands, removed obsolete
SCons/Travis guidance, updated Catch2 provenance, and added a workflow badge.
Conditional follow-through remains: 004 verifies publication/manual workflow,
and 005 supplies reviewed native screenshots for README. Current commands
are explicitly labeled; no future screenshot or PDF workflow is claimed to
exist.
