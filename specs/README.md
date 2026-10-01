# Implementation Specifications

Specs 001-005 are COMPLETE and archived in [archive/](archive/) as of
October 1, 2026, following the user's confirmation. They cover licensing,
project documentation, build/test infrastructure, source organization,
manuals, and production captures. Their completion records preserve prior
validation evidence and limitations.

Specs 008 and 012 are also COMPLETE and archived as of October 1, 2026.
All 16 modules now have native light/dark panels and new names. Dark captures,
manuals, wireframes and compatibility checks are complete; #95 is closed.
The [design handoff](assets/012/README.md) and
[integrated native gallery](assets/012/IMPLEMENTED.md) retain reproducible
artwork and validation evidence.

Spec 006 is COMPLETE and archived: Echo FIR sliders are restored, level and
bypass controls survive randomization, and issues #96/#97 are closed.
Spec 007 is COMPLETE and archived: Contour uses the 32 kHz envelope clock,
releases and rearms reliably, labels sustain rate SR, and closes #98.
Spec 009 is COMPLETE and archived by the user's 2026-10-01 decision. The
verified YM2612 event-path fix preserves the existing SSG behavior; the
unconfirmed original one-shot report and open #82 remain documented limitations.
Spec 010 remains PLANNED for optional Nuked-OPN2 engines. Spec 011 is COMPLETE
and archived by the user's 2026-10-01 decision: Voice 2151 is implemented,
locally verified and published on the development branch; #79 is closed.
Cross-platform and listening limitations remain recorded. Specifications and
source completion are not release promises; see each record for scope,
platform validation and publication limits.

## Work Areas And Ownership

| Spec | Focus | Primary Files | Dependencies |
| --- | --- | --- | --- |
| [001](archive/001-licensing-and-project-documentation.md) | Licensing, README, contributor and support guidance, public metadata | Root Markdown, license texts, `docs/licenses/`, GitHub templates, `plugin.json` | Complete; archived. Public documentation baseline for later work. |
| [002](archive/002-build-tests-and-ci.md) | Make-based tests, build hygiene, CI, release artifact validation | `Makefile`, `mk/`, `dep/`, test harness, `.github/workflows/`, build scripts and ignore rules | Complete; archived. Build/test baseline for later work. |
| [003](archive/003-source-organization-and-rack-integration.md) | DSP header organization, Rack helpers, panel identity, UI lifecycle | `src/`, runtime `res/`, focused regression fixtures | Complete; archived. Native theme delivery remains with 008. |
| [004](archive/004-manual-content-and-publication-style.md) | Manual source structure, shared typography, operating guides, reliable PDF builds | `manual/`, shared LaTeX/build rules | Complete; archived. Manual conventions for later changes. |
| [005](archive/005-production-panel-captures-and-figures.md) | Native module screenshots and source-controlled panel reference drawings | `tools/capture/`, `manual/*/img/`, `manual/*/figures/`, shared drawing primitives | Complete; archived. Theme-specific capture updates remain with 008. |
| [006](archive/006-super-echo-controls-and-randomization.md) | Restore FIR sliders (#96) and protect level/bypass controls from randomization (#97) | `src/SuperEcho.cpp`, a scoped slider helper if needed, focused Rack regressions, Super Echo manual | Complete; archived. Native Rack rendering, interaction, randomization and audio compatibility verified. |
| [007](archive/007-super-adsr-release.md) | Resolve Super ADSR release behavior and sustain-rate labeling (#98) | `src/SuperADSR.cpp`, Sony S-DSP ADSR, focused regressions, panel, debug patch, Super ADSR manual | Complete; archived. Core/module regressions, native Scope, panel/manual and #98 verified. |
| [008](archive/008-native-light-and-dark-themes.md) | Native Rack light/dark preference across all 16 enabled models (#95) | Runtime panel pairs, widget helpers/controls, theme regressions, minimum-Rack metadata and usage docs | Complete; archived. Native preference and #95 verified on Rack 2.4/2.6. |
| [009](archive/009-ym2612-ssg-retriggering.md) | Reliable polyphonic looping-envelope retriggers in Mini Boss and Boss Fight (#82) | Module gate/retrigger paths, shared YM2612 operator/voice DSP, focused tests and event fixtures, manuals | Complete; archived by user decision with verification evidence and original-report limitations retained. |
| [010](010-nuked-opn2-engines.md) | Optional Nuked-OPN2 YM2612/YM3438 engines in Mini Boss and Boss Fight (#83) | Vendored core, DSP adapter, module menus/state, focused audio/control/performance tests, manuals | Prototype control mapping and cost first; coordinate dependency/build work with 001/002 and preserve 009's loop contract. |
| [011](archive/011-ym2151-synth-voice.md) | New polyphonic Yamaha YM2151 synth voice (#79) | Core/adapter, new module and panel, registration, presets, manual, reference/performance tests | Complete; archived by user decision with validation limitations retained. Implementation published and #79 closed. |
| [012](archive/012-module-rebranding-and-title-system.md) | Rename all 16 active modules and integrate prepared paired panel artwork | Manifest display names, SVG title sources/exports, design handoff, current documentation, manuals, captures, font provenance | Complete; archived. All names, 32 panels, 14 manuals and reusable artwork sources integrated. |

The archived specs establish the inventory, executable baseline, source
organization, manual system, and capture tooling for subsequent work.
Their original handoffs remain documented as implementation history.

006 completed the focused Echo fixes. It owns [#96](https://github.com/Kautenja/PotatoChips/issues/96)
and [#97](https://github.com/Kautenja/PotatoChips/issues/97), including separate
resolution comments with fix commit references and closure after verification.
Both issues are closed as completed; the archived spec records the fix and
resolution comments.

007 completed the Contour gate-off fixes and sustain-rate terminology.
It owns [#98](https://github.com/Kautenja/PotatoChips/issues/98), now closed
as completed; its archived record includes native measurements, the timing
compatibility change, the fixing commit and the resolution comment.

008 independently owns [#95](https://github.com/Kautenja/PotatoChips/issues/95):
native global light/dark support, including both blanks, live switching,
previews, and readable controls. Its verified implementation and resolution
comment are recorded in the archived spec; #95 is closed as completed.

009 records the verified YM2612 gate/retrigger corrections in Mini Boss and
Boss Fight, including both soft-reset settings. It is complete and archived
by user decision. The original accepted-event one-shot symptom remains
unconfirmed, and [#82](https://github.com/Kautenja/PotatoChips/issues/82) remains
open; archiving the spec does not claim issue closure.

010 owns [#83](https://github.com/Kautenja/PotatoChips/issues/83): selectable
Nuked-OPN2 engines with the existing engine retained as the default. It
includes control-mapping and performance validation, useful issue updates,
and closure with accessible implementation commit references after verified
resolution. Writing the spec does not resolve the issue.

011 owns [#79](https://github.com/Kautenja/PotatoChips/issues/79): a new
YM2151 four-operator polyphonic synth voice with chip-specific modulation,
detune, and noise. It includes core selection, performance/reference tests,
complete module presentation, and issue follow-through. The implementation
commit and resolution comment are recorded in the archived spec; #79 is
closed as completed.

Shared files have explicit owners: 001 owns public metadata and contributor
prose, 002 owns workflow and test-build plumbing, 003 owns structural code
changes and general integration regressions, 004 owns manual build rules
and prose, and 005 owns the capture tool and panel figures. Specs 006-011
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

009 owns the YM2612 trigger/envelope corrections and regression evidence;
003 preserves their behavior and 004 incorporates the verified operating
guidance. It does not add the Mini Boss hard-sync feature declined in #94.

010 owns engine selection and its adapter, not 009's existing-engine defect.
Preserve the legacy rendering baseline and reuse 009's event/loop evidence
for the new modes. Coordinate changes to the shared module/DSP files rather
than treating engine replacement as proof that #82 is resolved.

011 owns the new YM2151 module and its artifacts. Extend 005's capture and
008's theme inventories when it is registered, along with 001/002/004's
public metadata, packaging, and manual coverage. Baseline module counts
remain historical evidence rather than a fixed limit on future inventory.

012 owns the display-name mapping and reproducible SVG title system for all
active modules, including both blanks. It preserves saved-patch identifiers,
control geometry, audio behavior, and historical PDF URLs. Coordinate its
publication assets with 004/005 and native theme wiring with 008. Its
prepared design handoff supplies the exact light/dark SVGs; integration
does not require new creative choices. The runtime panels, names and manuals
now use that handoff; all identifiers and geometry remain compatible.

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

The current manifest has 17 entries: 15 active sound modules with manuals
and two active blank panels. SuperSampler and SuperSynth were removed on
2026-10-01; their unused Sony S-DSP processor, BRR sample player, and tests
were also removed. DSP components used by active modules remain. Earlier
model and DSP suite counts in completion evidence are historical.
Keep those categories distinct in docs, capture coverage, and release checks.
Preserve the existing plugin slug, remaining module slugs, Rack IDs, patch
JSON, and preset compatibility.

This plan includes the siblings' Arhythmetic Units presentation and native
Rack theme conventions, while retaining Potato Chips' stable module identifiers.
Beyond 010's optional YM2612/YM3438 engines and 011's YM2151 module, it does
not schedule other new chips, enable unfinished modules, import NES
mapper/ROM work, create an FFT research/benchmark program or whitepaper,
or select a release version.
Historical `TBD` changelog entries are not automatically accepted feature
requirements.

## Spec Lifecycle

Follow [Planning And Completion](../AGENTS.md#planning-and-completion) for
status, acceptance evidence, and archiving. Commands labeled as proposed
belong to the intended implementation and do not work merely because these
specs exist. Record actual results, platforms, manual checks, and limitations
in the owning spec; unchecked criteria remain outstanding unless an explicit
completion decision supersedes the historical checklist.

Small changes can still be planned in chat. Allocate the next unused number
across this directory and `archive/`; preserve numbers when archiving.
