# Rack Contract Fixtures

`contracts.txt` records registration order, parameter/input/output/light counts,
parameter names/ranges/defaults/snap settings, sorted custom JSON, and audio
sum/energy after 2,048 samples for 2A03, 106, GBS, and SuperEcho. Audio runs
use 44.1/48/96 kHz, 1/16 connected input channels with `0.01 * channel` volts,
and every output connected. Repeated runs use freshly constructed modules.
Tests establish connections through the host port channel field because
Rack's `setChannels()` does not connect an unconnected port.

The metadata baseline was captured from `adebc068` with Apple's Clang and
Rack SDK 2.6.3 on macOS arm64. `audio-before-blip-fix.txt` retains the original
audio aggregates. The BLIP bounds corrections intentionally change those
aggregates. The accepted audio fixture comes from a separate checkout of
`adebc068` with only those BLIP loop bounds corrected; it agrees exactly with
the reorganized sources on the same machine. Thus the fixture does not use
source reorganization itself to define expected audio. Both original and
corrected captures were repeatable locally.

Metadata must match exactly. Audio permits relative error `1e-5` and absolute
error `1e-7` for compiler floating-point differences in synthesis and aggregate
summation; these tolerances are not permission for audible behavior changes.
The first local run agrees exactly; other architectures require CI verification.
The restored-patch tests also load all 60 Super Echo presets and registered
project modules from all 24 debug patches. They preserve Rack's handling of
reversed parameter ranges. Unregistered historical SCC/TurboGrafx16 and
external plugins are excluded from this headless restoration test.

`test_chip_module` uses deterministic impulses to isolate sample-clock
quantization, 1/16-channel cadence and independence, reset, normalling and
clipping from emulator behavior. `test_host` checks mono-CV reuse in Infinite
Stairs. `test_widgets` covers sample boundaries, inclusive/vertical edits,
read-only previews, deletion/recreation history, concurrent atomic access,
missing SVGs and out-of-range frames. Repeated frame destruction exercises
RAII; macOS ASan does not provide LeakSanitizer leak accounting.

Run from the repository root:

```shell
make -j2 all test-rack RACK_DIR=/path/to/Rack-SDK
make -j2 test-rack RACK_TEST_MODE=asan-ubsan RACK_DIR=/path/to/Rack-SDK
```

Do not refresh numeric fixtures simply to make a failing test pass. Reproduce
the difference, distinguish intended fixes from structural changes, and
record the reason and independent comparison in the owning specification.
