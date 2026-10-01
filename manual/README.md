# Potato Chips Manuals

The 14 sound-module manuals share typography and build rules while keeping
module-specific operating instructions in `manual/<Module>/sections/`.
`manual.tex` declares the manifest name, explicit version and section order.
The two blank panels have no manuals.

## Build And Check

From the repository root:

```shell
make -C manual
make -C manual/SuperEcho
make -C manual clean
python3 manual/latex/test-build.py
```

Install Python 3, Make, `latexmk`, pdfLaTeX and BibTeX. The shared style uses
T1/UTF-8 support, Helvetica (PSNFSS), AMS math, microtype, natbib, xcolor,
graphicx, booktabs, tabularx, longtable, array, TikZ, geometry, enumitem,
fancyhdr, titlesec, needspace, caption, hyperref, bookmark and hyperxmp.
TeX Live with its recommended fonts and extra LaTeX packages supplies these;
the migration was built with TeX Live 2022 and latexmk 4.77 on macOS.
No Rack SDK, running Rack instance, shell escape or capture tool is required.

Individual output is `<Module>/.build/manual.pdf`. The root build copies
successful outputs into `.build/` with the stable names below. Builds read
tracked sources directly; they never copy TeX or images into an output tree.
`BUILD` can override an individual output directory. Both build and clean
reject old output directories containing copied sources. Choose a fresh
`.build` directory instead of building from a legacy `build/` copy.

`latexmk` converges references and bibliographies. A successful build removes
auxiliaries, including the generated bibliography. Failure keeps the log and
auxiliaries for diagnosis and removes the failed PDF; the collection target
removes its previous copy before starting that module. The log check rejects
overflow and unresolved references. Version drift from `plugin.json` fails
before compilation. Clean works even after the manifest version changes.

The disposable build regression covers a fresh build, missing input,
undefined reference, overflow, version mismatch, stale copied sources,
recovery and cleaning. It runs real TeX without modifying these sources.

## Publication Inventory

| Source Directory | Collection Filename |
| --- | --- |
| `Facets` | `Blocks.pdf` |
| `BossFight` | `BossFight.pdf` |
| `InfiniteStairs` | `InfiniteStairs.pdf` |
| `Trio AY` | `Jairasullator.pdf` |
| `MegaTone` | `MegaTone.pdf` |
| `MiniBoss` | `MiniBoss.pdf` |
| `NameCorpOctalWaveGenerator` | `NameCorpOctalWaveGenerator.pdf` |
| `PalletTownWavesSystem` | `PalletTownWavesSystem.pdf` |
| `PotKeys` | `PotKeys.pdf` |
| `Pulse FME-7` | `Pulses.pdf` |
| `StepSaw` | `StepSaw.pdf` |
| `SuperADSR` | `SuperADSR.pdf` |
| `SuperEcho` | `SuperEcho.pdf` |
| `SuperVCA` | `SuperVCA.pdf` |

With Poppler's `pdfinfo`, `pdftotext` and `pdftoppm` on PATH, validate the
collection and render every page for inspection:

```shell
python3 scripts/validate.py manuals manual/.build
mkdir -p /tmp/potatochips-manual-review
for pdf in manual/.build/*.pdf; do
    name=$(basename "$pdf" .pdf)
    pdfinfo "$pdf" > "/tmp/potatochips-manual-review/$name-info.txt"
    pdftotext -layout "$pdf" "/tmp/potatochips-manual-review/$name.txt"
    pdftoppm -scale-to 1400 -png "$pdf" "/tmp/potatochips-manual-review/$name"
done
git diff --check
```

Inspect every rendered page, including covers, contents, tables, figures and
colophons. Check selectable text, linked contents, outline destinations,
external links and English language metadata in a PDF reader. Rebuild and
review all 14 after a shared-style or shared-figure change. Generated PDFs,
auxiliaries and temporary renders are not source files to commit.

## Writing And Source Checks

Use a working first patch, spatial control reference, practical variations
and troubleshooting. Explain the voltage scale, defaults, quantization,
normalled cables, polyphonic channel behavior and saved state where relevant.
Follow the [prose guide](../docs/style-guides/markdown.md). Keep useful
bibliographies and reference labels; do not preserve a numerical claim merely
because it appeared in an older guide.

Check `src/<Module>.cpp`, its parameter quantities and widget placement,
`src/engine/chip_module.hpp`, and the corresponding `src/dsp/` implementation.
For example, voice columns and polyphonic cable channels are different;
mono CV repeats only where the implementation uses Rack's poly accessors.
`normalChain()` by itself does not repeat mono CV.

Existing `patches/debug/<Module>.vcv` files and Echo presets provide
compatibility examples. The Rack contract test restores project modules
from those files and checks their parameters, but does not load every
third-party module or establish that a complete musical patch sounds right.
Run the appropriate native patch and listen before claiming that check.

The 004 source audit corrected the following legacy claims:

| Area | Current Implementation |
| --- | --- |
| Super Echo timing | 512 host frames per positive delay level; 31 levels, plus FIR tap latency. The 16 ms tooltip calibration assumes 32 kHz. |
| Super Echo mix/filter | Wet signal adds to dry; FIR affects wet audio even with feedback at zero. Mix CV is additive. |
| FM oscillator inputs | Infinite Stairs, Blocks, Pulses, Mega Tone, Pot Keys, Jairasullator and both wavetable modules add pitch exponentially; Step Saw uses half their FM scale. |
| Noise and pulse CV | Several old per-step voltages were incorrect; tables now follow the actual scaling, truncation and reversed chip registers. |
| Mini Boss | One operator; VOL is additive, not a multiplicative VCA. Envelope CV uses an 8 V scale. |
| Boss Fight | Algorithm/feedback/LFO CV uses 7/8 setting per volt. Only gate, retrigger and pitch forward across operators. Both outputs are identical. |
| Octal 163 | Active-count depth uses 1.25 V per voice at full depth; changing count changes pitch. One global morph control feeds all eight oscillators per chip instance. |
| Pocket APU | Wave Level is off/25/50/100%; Morph 0--1 selects the first wave. No implemented noise-sync input, despite the legacy drawing. |
| Super ADSR | Last slider is SR (sustain rate). Release is fixed at up to 8 ms; sustain threshold is 12.5--100%; all stages use a 32 kHz clock. |
| Super VCA | Input conversion is 8-bit; level is signed. Frequency drives interpolation and is not a measured cutoff. Mode selection has a compensation inconsistency. |

The old Staircase 2A03 exact noise-frequency/MIDI table and Contour
schematic were retired because they imply calibration or stages not supported
by the current implementation. Operator routing/envelope figures, LFO tables,
shape/mode references and all existing bibliography entries are retained or
rewritten with current meanings. Unverified historical instrument assignments
are replaced by patch starting points.

Runtime discrepancies discovered during this work are routed to
[003](../specs/archive/003-source-organization-and-rack-integration.md#manual-audit-follow-ups-004).
The existing Echo, ADSR and YM2612 issue specs continue to own their fixes;
manual edits do not resolve those reports.

## Artwork And Handoffs

### Repository And Social Banner

`PotatoChips-SocialMedia.svg` is the vector master for the Arhythmetic Units
banner. Its text is outlined and its logo is embedded from
`../res/ArhythmeticUnits.svg`, so it needs no external fonts or images.
The matching PNG is 1280 by 640 pixels; the PDF is a single vector page
with the same 2:1 aspect ratio. Keep all three exports aligned when editing
the master. These tracked publication assets retain their established names
so existing links continue to work. The root README embeds the PNG.

GitHub's custom social preview is a separate uploaded image. After changing
the banner, upload `PotatoChips-SocialMedia.png` in the repository's
**Settings > General > Social preview**; changing the tracked file does not
update that upload. Artwork terms and provenance are in
[the component inventory](../docs/licenses/THIRD-PARTY.txt).

### Module Artwork

Covers and the root README use `img/Panel.png`, lossless crops from the real
Rack widgets. Panel maps use `figures/panel-layout.tex` and shared primitives
in `latex/panel-drawing.tex`. Their numbered groups match each manual's
control reference. Display rectangles and indicators intentionally carry no
live values; the cover shows an actual default-state fixture.

See [the capture guide](../tools/capture/README.md) for SDK/OpenGL/Pillow
prerequisites, deterministic fixtures, reviewed geometry, failure checks and
refresh commands. Ordinary manual builds need only the committed PNG and TeX
sources; they never invoke the native renderer. Update the production capture
and vector guide together when widget geometry changes. Covers use the dark theme for contrast on paper. Echo includes the restored FIR sliders from spec 006.

The shared wordmark `latex/ArhythmeticUnits.pdf` is a byte-for-byte copy of
RackNES `manual/RackNES/img/ArhythmeticUnits.pdf` at commit
`b783a8e473f00242773e3fccbd4ed9e1a6ae17d5`. Its artwork terms and the retained
legacy illustration provenance gaps are recorded in
[LICENSING.md](../LICENSING.md) and the
[component inventory](../docs/licenses/THIRD-PARTY.txt). The repository has no
build dependency on RackNES or Fourier. PDF metadata links to the scoped
license guide rather than declaring one blanket license for the document.

Spec 002 can consume `manual/.build/*.pdf` and the existing completeness
validator. This migration does not enable release upload or claim that the
new manuals have been published.

Module titles use shared LaTeX text styling, with canonical names in
`specs/assets/012/directions.json`. Retired per-module logos, chip pictures,
unused envelope/mode drawings, VU and switch graphics are no longer build
inputs. The two YM2612 envelope PNGs and the Voice 2612 operator PDF/SVG
remain explanatory figures in active use. Historical filenames stay stable.
