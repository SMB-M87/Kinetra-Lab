# Dotio tests

Regression tests for Dotio game logic, application commands and presentation.

## Table of contents

- [Coverage](#coverage)
- [Build and run](#build-and-run)
- [Navigation](#navigation)

## Coverage

| Source | CTest name | Coverage |
| --- | --- | --- |
| [dotio_tests.cpp](dotio_tests.cpp) | `kinetra_dotio` | Geometry, routing, streaming, combat, fuzzy AI, difficulty, lifecycle, debug camera and commands |
| [presentation_tests.cpp](presentation_tests.cpp) | `kinetra_dotio_graphics` | Wall colors, rendering resources, statistics and Ghost marker presentation |

## Build and run

Run from the repository root for the tests that need no window:

```sh
cmake -S . -B build-headless -DKINETRA_LAB_BUILD_RUNTIME=OFF -DBUILD_TESTING=ON
cmake --build build-headless --target kinetra_dotio_tests --parallel
ctest --test-dir build-headless -R '^kinetra_dotio$' --output-on-failure
```

For `kinetra_dotio_graphics`, use a runtime build with `KINETRA_TEST_GRAPHICS=ON` and an OpenGL context as described in [Tests and resources](../../../README.md#tests-and-resources). Filter that build's CTest run with `-R '^kinetra_dotio_graphics$'`.

## Navigation

[Parent guide](../README.md) · [Project overview](../../../README.md) · [Documentation index](../../../docs/README.md)
