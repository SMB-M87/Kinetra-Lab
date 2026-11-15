# Labs

Applications and experiments built on the public Kinetra APIs. Each lab owns its application logic, presentation and tests; reusable engine functionality belongs in Kinetra.

| Guide | Purpose |
| --- | --- |
| [Demo](demo/README.md) | Reference application for terrain, navigation, agents and physics |
| [Dotio](dotio/README.md) | Survival game and AI experiment |

## Table of contents

- [Build and run](#build-and-run)
- [Verification](#verification)

[Parent guide](../README.md) · [Documentation index](../docs/README.md)

Directory guides: [demo](demo/README.md), [dotio](dotio/README.md).

## Build and run

Build from the repository root; this directory is included by the top-level CMake project. See the [quick start](../README.md#quick-start) for dependencies and Kinetra setup.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/kinetra_demo --frames 120
./build/kinetra_dotio --difficulty medium --frames 120
```

The [launcher](../launcher/README.md) also provides a shared menu for both applications.

## Verification

For logic tests without graphical applications, run from the repository root:

```sh
cmake -S . -B build-headless -DCMAKE_BUILD_TYPE=Release -DKINETRA_LAB_BUILD_RUNTIME=OFF -DBUILD_TESTING=ON
cmake --build build-headless --parallel
ctest --test-dir build-headless --output-on-failure
```

See [tests and resources](../README.md#tests-and-resources) for graphics checks. Application-specific behavior and verification are documented in the guides above.
