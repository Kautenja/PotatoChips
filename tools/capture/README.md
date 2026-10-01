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
capture, validates the entire requested batch and updates `img/Panel.png` from the **Dark** view by default.
For an explicit light export, pass `--theme Light` to `export_screenshots.py`.
`drawings` exports production control bounds through shared TikZ primitives,
using the reviewed groups in `regions.json`. Select a manual directory name
with `MODULE`; blank smoke captures accept their manifest slug. Neither
blank has a publication PNG.
The [inventory](modules.json) explicitly maps all 17 enabled slugs, panel
files, widths and 15 manual directories; changes in that inventory require
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
effects intentionally remain idle. The publication fixture validates visuals. A separate post-capture probe
compares 16-channel audio on the 2A03, 106, GBS and SuperEcho models against
untouched twins during repeated theme changes; no device is opened.

Octal 163 and Pocket APU show their five factory waveforms. Voice 2612
shows the production default algorithm. Contour's former RR labels now read SR (007).
Echo's eight FIR sliders are restored by spec 006. Its focused native probe
checks bindings, horizontal hits/drags, numeric entry, reset, randomization,
undo/redo and preset restoration, and captures bypass with lights off and
alternating positive/negative CV at both themes, two zooms and dim lighting. Diagrams never invent missing controls or live display data.

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

All 17 live and preview panels use paired native artwork. The renderer
asserts the selected panel, port and screw SVG pointers, unchanged control
geometry, module JSON and patch history across each light/dark/light toggle.
It also verifies immediate construction with dark preference selected.
Additional views exercise 75%/150% zoom and 50% room dimming outside the
mouse spotlight, retaining emissive light layers. The exporter rejects
identical light/dark pixels and requires 34 restored
context views to match their light images exactly.

Spec 012's [artwork source](../../specs/assets/012/README.md) pins all titles,
palettes and geometry. Regeneration and runtime equality checks:

```shell
python3 specs/assets/012/build-artwork.py --check --installed
```

The shared footer geometry and all control positions are retained. Panel
maps now show the current module name through shared LaTeX typography.
The manual captures always depict actual native widgets, never AI concepts.

Capture code adapts RackNES's `tools/capture/` and Fourier's native inspector
under GPL-3.0-or-later. Panel art retains the separate terms in
[LICENSING.md](../../LICENSING.md); native screenshots also depict Rack's
host-provided component graphics. No host resource files are copied here.

## Contour Gate-Off Scope

Spec 007's optional probe loads the real Fundamental Scope binary and
resources from `FUNDAMENTAL_DIR` (default `$(RACK_DIR)/plugins/Fundamental`).
It renders real module/Scope widgets and records measured samples; it does
not draw a replacement scope or open a user's Rack session. Build Fundamental
for the matching Rack ABI first. This is not a normal plugin dependency.

```shell
make -C tools/capture adsr-scope
```

Output is ignored under `.build/adsr-scope/`: nine native PPMs, measurements
and sample CSVs, for 44.1/48/96 kHz and SR 0/20/31. The stimulus is one
1 ms 0/5 V gate, amplitude 127, Attack/Decay 0, Sustain Level 7, RETRIG
disconnected. Scope shows Gate in the upper trace and OUT below it, starting
0.5 ms before key-off. Requested width is 10 ms; its 256 acquisition buckets
round up to host frames (actual width is recorded). No audio device is used.

`patches/debug/SuperADSR.vcv` is the interactive Rack 2 counterpart: a 50 Hz
LFO with 5% pulse width, followed by 8vert at 0.5, supplies 1 ms 0/5 V gates.
Scope captures both Gate and OUT, triggered at 2.5 V. Sweep SR through 0, 20
and 31; lengthen the pulse to distinguish held decay from fixed key-off.
The second lane is intentionally idle until patched. The fixture contains
no audio/MIDI device configuration.
