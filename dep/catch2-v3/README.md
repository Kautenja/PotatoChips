# Catch2

Unmodified Catch2 3.16.0 amalgamated sources, selected to match Fourier's
migration. [provenance.json](provenance.json) pins release commit
`317ac1ed4c0bb6e6b91eafc817e05c488feffcb3` and SHA-256 hashes.

-   [Header](https://raw.githubusercontent.com/catchorg/Catch2/317ac1ed4c0bb6e6b91eafc817e05c488feffcb3/extras/catch_amalgamated.hpp)
-   [Source](https://raw.githubusercontent.com/catchorg/Catch2/317ac1ed4c0bb6e6b91eafc817e05c488feffcb3/extras/catch_amalgamated.cpp)
-   [Boost Software License 1.0](LICENSE_1_0.txt), from upstream `LICENSE.txt`

Tests use C++14; production remains C++11. The amalgamated source supplies
main and compiles once per configuration. It is never linked into the plugin.
There is no CMake build, system Catch2, or submodule requirement.

When updating, fetch all three files from one exact upstream commit, retain
unmodified bytes, and update this document, the JSON hashes, the contributor
guide, and the license inventory. From the root, run
`python3 scripts/validate.py dependencies`, `make test`, `make test-rack`,
and the instrumented targets described in [CONTRIBUTING.md](../../CONTRIBUTING.md).
