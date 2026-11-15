# Bundled application launcher

Composition root for Demo and Dotio; generic menu/session infrastructure and tests belong to Kinetra.

## Table of contents

- [Composition](#composition)
- [Arguments](#arguments)
- [Verification](#verification)

[Parent guide](../README.md) · [Documentation index](../docs/README.md)

Directory guides: [include](include/README.md), [src](src/README.md), [tests](tests/README.md).

## Composition

[main.cpp](src/main.cpp) parses arguments, registers factories and calls the public `run_launcher` event loop. [session.cpp](src/session.cpp) chooses application-owned factories and initializes cameras. [menu.h](include/menu.h) adapts application identities and Dotio difficulty to public LaunchMenu. Engine lifecycle/ownership is documented once in [launcher and overlays](../docs/launcher-overlays.md).

## Arguments

| Argument | Accepted values |
| --- | --- |
| `--variant` | `demo`, `dotio`; omit for menu |
| `--difficulty` | `easy`, `medium`, `hard`; default medium |
| `--agents` | 0–300 initial Demo agents; default zero |
| `--frames` | 1–100000 rendered frames; omit for unlimited |

Both `kinetra` and compatibility `kinetra_sandbox` use this composition root. A process selects and prints one random world seed. Selecting an application creates a fresh session. Detailed controls live in the [Demo](../labs/demo/README.md) and [Dotio](../labs/dotio/README.md) guides. Build commands live in [Quick start](../README.md#quick-start).

## Verification

[Menu tests](tests/menu_tests.cpp) run without a window in a launcher-enabled build. [Integration tests](tests/runtime_tests.cpp) require an OpenGL context and cover rendering, difficulty, switching and debug capture. See [development](../README.md#tests-and-resources).
