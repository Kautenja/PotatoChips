# Potato Chips Support

Start with the [first-sound guide](README.md#make-a-first-sound) and the
[manual for your module](README.md#modules). Search
[existing issues](https://github.com/Kautenja/PotatoChips/issues) before
opening a report; a matching issue may contain a workaround or fix.

## Troubleshoot A Patch

-   Confirm Rack/plugin versions, operating system, and CPU architecture.
    This checkout, GitHub releases, and VCV Library builds may differ.
    A planned spec does not mean a fix has shipped.
-   Reduce the patch to the affected module and built-in VCV modules.
    Check levels, Audio device, gate state where needed, and mono versus
    polyphonic cables. Consult the manual for output mixing/normalization.
-   For timing/tuning problems, record sample rate, polyphony, knob values,
    input voltages, and the order of gate/retrigger events.
-   For panel problems, record Rack version, zoom, panel theme, and display
    scaling; include a screenshot of the affected control.

## Report A Bug

Use the [bug report template](https://github.com/Kautenja/PotatoChips/issues/new?template=bug_report.md).
Include expected/actual behavior, exact steps, environment versions, and
installation source. Attach a minimal `.vcv` patch and list other plugins
or files it needs. Remove personal paths, device identifiers, tokens, and
unrelated content from shared logs/patches. For crashes, include the relevant
Rack log excerpt and last action. Compiler/SDK details are useful for build
problems; they are not required for reports about installed modules.

## Request A Feature

Explain the musical task, missing behavior, and alternatives tried in a
[feature request](https://github.com/Kautenja/PotatoChips/issues/new?template=feature_request.md).
Include compatibility/UI considerations. Closed requests and
[specifications](specs/README.md) explain prior scope decisions; planned
work has no guaranteed completion date.

For Rack installation, account, purchases, or Library service problems,
use [VCV support](https://vcvrack.com/support). For code/documentation
changes, see [CONTRIBUTING.md](CONTRIBUTING.md). Project issues are the
public support channel; no response time is promised.
