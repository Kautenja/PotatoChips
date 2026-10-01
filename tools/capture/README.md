# Production Panel Captures

Optional native Rack/OpenGL tooling for Potato Chips covers and reference
figures. It links production registration and all module constructors;
it does not emulate the UI, open a patch, start an engine thread, select an
audio device, or read an installed plugin's user data.

## Requirements And Commands

Use a prepared Rack 2 checkout/SDK with matching headers, `libRack`, and
`res/` resources, a C++11 compiler, Make, Python 3.9+ with Pillow, and an
active graphical desktop. The default SDK path is four levels above this
directory. Override `RACK_DIR=/path/to/Rack-SDK` for another location.

From the repository root:

```shell
make -C tools/capture capture
make -C tools/capture screenshots
make -C tools/capture drawings
make -C tools/capture screenshots MODULE=PotKeys
make -C tools/capture drawings MODULE=PotKeys
make -C tools/capture capture
make -C tools/capture test
make -C tools/capture test-native
make -C manual
```

`capture` only writes ignored `.build/captures/`. `screenshots` runs a fresh
capture, validates the entire requested batch and updates `img/Panel.png`.
`drawings` exports production control bounds through shared TikZ primitives,
using the reviewed groups in `regions.json`. Select a manual directory name
with `MODULE`; blank smoke captures accept their manifest slug. Neither
blank has a publication PNG. Disabled Super Sampler/Super Synth are excluded.
The [inventory](modules.json) explicitly maps all 16 enabled slugs, panel
files, widths and 14 manual directories; changes in that inventory require
review. No sibling checkout is a build dependency.

For repeated export from an unchanged capture, without rerunning Rack:

```shell
python3 tools/capture/export_screenshots.py tools/capture/.build/captures manual
python3 tools/capture/draw_panels.py tools/capture/.build/captures
```

Review captures before committing refreshed assets. Review the complete
rebuilt PDFs after changing either covers or panel maps. Normal PDF builds
and CI consume committed PNG/TeX only, with no SDK, desktop or Pillow.

## Geometry And Fixture

Each native canvas is `(panel width + 20) x 420` Rack pixels. The module
starts at `(10, 20)` and is exactly 380 pixels tall. Read the integer physical
pixel ratio from the framebuffer; crop `(10, 20, width + 10, 400)` multiplied
by that ratio. Never scale, sharpen, repaint or reconstruct screenshot
content. On the reviewed Retina session the ratio is 2, so PNG heights are
760 pixels and widths are twice the inventory width.

All modules use constructor defaults at 48 kHz, no patched inputs, and 4,800
synchronous process calls (100 ms) before creating the live widget. Rack's
RNG is seeded with `0x504f5441544f, 0x4348495053` before each live/preview
construction, making full and selected-module captures use the same fixture.
There are no parameter overrides, synthetic gates, ROMs or sample files.
The two blanks have no processing behavior. Untriggered envelopes and idle
effects intentionally remain idle. This fixture validates visuals, not audio.

Name Corp and Pallet Town show their five factory waveforms. Boss Fight
shows the production default algorithm. Screenshots retain known runtime
limitations: missing Super Echo FIR sliders (006) and Super ADSR's historical
RR label (007). Diagrams never invent missing controls or live display data.

## Failure And Lifecycle Checks

A fresh isolated temporary user/output directory prevents old captures from
satisfying a new run. The launcher validates required panels/algorithm frames,
checks the enabled manifest inventory, applies a 120-second process timeout,
and records source revision, rendering-input SHA-256, library hash, fixture,
platform and per-module geometry. Inputs changing during a run invalidate it.
A failed renderer leaves the previous intermediate batch and tracked PNGs
alone. A source fingerprint prevents exporting a stale batch later.

The renderer checks model registration, exact panel geometry, SVG widgets,
OpenGL errors, and every component framebuffer, with at most 160 draw attempts.
It constructs live and null-module widgets and toggles light/dark/light on
both, then dispatches Rack context-destroy/create events to each widget and
captures again. All six images per enabled module are required. Export rejects blank
frames, missing images, wrong geometry, and pixel changes after context
restoration. Every image is validated and encoded before any destination is
replaced; an I/O failure rolls back earlier replacements. Process termination
or power loss during file replacement is outside this rollback guarantee.

`test_capture.py` uses a real batch in a disposable tree to check wrong
geometry late in the batch, missing output, blank rendering, stale sources,
an injected destination I/O failure, and single-module isolation. Rerun a
full `capture` before these tests after using `MODULE`. `test-native` exercises
real wrong-geometry and missing-panel failures plus an intentionally failing
renderer process, preserving publication files and the preceding batch.

## Review Baseline And Limitations

Reviewed October 1, 2026 on macOS 26.6.2 arm64 with the local Rack 2.6.0
headers/library/resources, OpenGL and a 2x Retina framebuffer. Source base:
`76295a14c01276c1e9afd04d23e30e094d995adf`, plus the spec 005 working changes.
The ignored `batch.json` records the exact rendering-input hash for each run.
Do not expect pixel-identical output across different OS/font/graphics builds.
macOS is verified; Linux's build path is provided but unverified. Windows
native capture is unsupported. These limits do not affect normal PDF builds.

All 16 live and preview panels were inspected, including factory waveforms,
Boss Fight's algorithm, sliders, lights, titles and footer clearances. The
64 light/dark preference views use the same current artwork: native dark
panel pairs and actual theme selection remain **pending spec 008**. These
captures do not claim that dark themes are implemented. Another 32 restored
context views must match their initial light images exactly.

The Arhythmetic Units footer matches Fourier's native logo scale: the full
wordmark is 120.4554 Rack pixels wide and 11.2471 high on panels 12 HP and
wider. Narrower panels use the unchanged 11.3938-pixel-wide square emblem
at the same scale, keeping its detail readable without crowding the screws.
Both variants are horizontally centered with their top at 366.882 pixels,
including blank and dormant panels. Boss Fight's wordmark moves from the
left control block to the panel center. Pallet Town retains white fill for
contrast.
Blocks and Name Corp's title contours were separated into independent solids
with their existing holes; contour starting points avoid touching boundaries.
This preserves the original vector silhouettes while avoiding Rack's
compound-path hole inference artifacts. No title font was replaced.

Capture code adapts RackNES's `tools/capture/` and Fourier's native inspector
under GPL-3.0-or-later. Panel art retains the separate terms in
[LICENSING.md](../../LICENSING.md); native screenshots also depict Rack's
host-provided component graphics. No host resource files are copied here.
