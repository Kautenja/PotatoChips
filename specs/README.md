# Implementation Specifications

The first five specifications adapt the recent Fourier and RackNES
maintenance work to PotatoChips. Specs 006-008 address reported Super Echo,
Super ADSR, and native theme issues. They describe planned work, not
implemented behavior or release promises. All eight are `PLANNED` as of
October 1, 2026.

## Work Areas And Ownership

| Spec | Focus | Primary Files | Dependencies |
| --- | --- | --- | --- |
| [001](001-licensing-and-project-documentation.md) | Licensing, README, contributor and support guidance, public metadata | Root Markdown, license texts, `docs/licenses/`, GitHub templates, `plugin.json` | Can start immediately; finish command, image, and release guidance as 002-005 land. |
| [002](002-build-tests-and-ci.md) | Make-based tests, build hygiene, CI, release artifact validation | `Makefile`, `mk/`, `dep/`, test harness, `.github/workflows/`, build scripts and ignore rules | Establish baseline before 003; integrate the PDF job after 004. |
| [003](003-source-organization-and-rack-integration.md) | DSP header organization, Rack helpers, panel identity, UI lifecycle | `src/`, runtime `res/`, focused regression fixtures | Use 002's harness; coordinate branding/provenance with 001 and preserve 008's themes. |
| [004](004-manual-content-and-publication-style.md) | Manual source structure, shared typography, operating guides, reliable PDF builds | `manual/`, shared LaTeX/build rules | Can begin with current code/artwork; final review follows relevant 003 and 005 changes. |
| [005](005-production-panel-captures-and-figures.md) | Native module screenshots and source-controlled panel reference drawings | `tools/capture/`, `manual/*/img/`, `manual/*/figures/`, shared drawing primitives | Capture final 003 widgets and 008 themes; integrate with 004's manual layout. |
| [006](006-super-echo-controls-and-randomization.md) | Restore FIR sliders (#96) and protect level/bypass controls from randomization (#97) | `src/SuperEcho.cpp`, a scoped slider helper if needed, focused Rack regressions, Super Echo manual | Can proceed before the modernization specs; reuse 002/005 infrastructure if available. |
| [007](007-super-adsr-release.md) | Resolve Super ADSR release behavior and sustain-rate labeling (#98) | `src/SuperADSR.cpp`, Sony S-DSP ADSR, focused regressions, panel, debug patch, Super ADSR manual | Can proceed independently; reuse 002/005 infrastructure if available. |
| [008](008-native-light-and-dark-themes.md) | Native Rack light/dark preference across all 16 enabled models (#95) | Runtime panel pairs, widget helpers/controls, theme regressions, minimum-Rack metadata and usage docs | Owns theme work formerly in 003; coordinate branding with 003 and captures with 005. |

Begin with 001's inventory and 002's executable baseline. Then make 003's
structural changes with regression evidence, develop 004's shared manual
system, and complete 005's captures and diagrams. Finish the public links,
publication CI, and full-manual review once those outputs exist. These are
staged handoffs within five specs, not dependencies that require every spec
to finish before the others can start.

006 is a focused bug-fix priority that can proceed independently of that
sequence. It owns [#96](https://github.com/Kautenja/PotatoChips/issues/96)
and [#97](https://github.com/Kautenja/PotatoChips/issues/97), including separate
resolution comments with fix commit references and closure after verification.
Writing or committing the spec does not resolve either issue.

007 independently owns [#98](https://github.com/Kautenja/PotatoChips/issues/98):
reproduce the reported gate-off behavior, correct demonstrated release
defects, and align sustain-rate terminology. It includes meaningful issue
updates and closure with a fixing commit reference after verification.
Writing or committing the spec does not resolve the issue.

008 independently owns [#95](https://github.com/Kautenja/PotatoChips/issues/95):
native global light/dark support, including both blanks, live switching,
previews, and readable controls. It authorizes useful issue updates and
closure with an implementation commit reference after verification. Planning
alone does not resolve the issue.

Shared files have explicit owners: 001 owns public metadata and contributor
prose, 002 owns workflow and test-build plumbing, 003 owns structural code
changes and general integration regressions, 004 owns manual build rules
and prose, and 005 owns the capture tool and panel figures. Specs 006-008
own their focused fixes/features, tests, and issue follow-through. Update
cross-references when a producer changes an agreed path or command.

For Super Echo's missing FIR controls and randomization policy, 006 owns
the fixes and regressions; 003 preserves them during refactoring, 004 carries
their verified behavior into the manual rewrite, and 005 captures the fixed
widget. Avoid duplicating implementation or completion evidence across specs.

For Super ADSR's release report, 007 owns the fix, regressions, and targeted
panel/manual corrections. Specs 003-005 preserve and incorporate its verified
behavior, terminology, and captures in their broader work.

008 owns theme wiring, paired artwork, and minimum-Rack compatibility.
003 applies branding consistently to both variants; 001 preserves artwork
terms/provenance; 004 documents usage; 005 captures both themes. Theme work
can start before those broad changes and does not introduce patch state.

## Evidence And Baseline

The comparison used local changelogs, commit diffs, and checked-out sources:

-   PotatoChips: `33fb1554` for existing product behavior; `d1821c9f` adds the
    agent setup. The manifest is version `2.0.1` on branch `v2.0.2`.
-   Fourier: `2956159`, particularly the modernization in `e5f1a43` relative
    to `e5bd7d0`, and the later publication fixes. Its current layout uses
    `mk/`, `src/rack_extensions/`, `docs/manual-*/`, and `docs/latex/`.
-   RackNES: `4ef5025`, with separate commits for licensing, documentation,
    CI, production captures, browser safety, and Rack-global themes. Its
    current layout retains `manual/` and adds `manual/latex/`, `tests/`,
    and `tools/capture/`.

Individual specs cite the relevant commits and files. The sibling checkouts
are reference material, not runtime or build dependencies. Their own prose
can lag their code: for example, Fourier's Catch2 README still mentions
SCons although its active build is Make-based. Verify the implementation
before transferring an instruction.

The current manifest has 18 entries: 14 active sound modules with manuals,
two active blank panels, and disabled SuperSampler/SuperSynth entries with
no corresponding manual sources. Keep those categories distinct in docs,
capture coverage, and release checks. Preserve the existing plugin slug,
all module slugs, Rack IDs, patch JSON, and preset compatibility.

This plan includes the siblings' Arhythmetic Units presentation and native
Rack theme conventions, while retaining Potato Chips' module identities.
It does not schedule new chips, enable unfinished modules, import NES
mapper/ROM work, create an FFT research/benchmark program or whitepaper,
or select a release version. Historical `TBD` changelog entries are not
automatically accepted feature requirements.

## Spec Lifecycle

Follow [Planning And Completion](../AGENTS.md#planning-and-completion) for
status, acceptance evidence, and archiving. Commands labeled as proposed
belong to the intended implementation and do not work merely because these
specs exist. Record actual results, platforms, manual checks, and limitations
in the owning spec; unchecked criteria remain outstanding.

Small changes can still be planned in chat. Allocate the next unused number
across this directory and `archive/`; create the archive on first use.
