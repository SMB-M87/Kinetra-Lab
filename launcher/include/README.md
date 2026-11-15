# Launcher interfaces

Sources in this directory belong to `kinetra_sessions`. These are Lab-owned interfaces and implementations; they are not part of the public Kinetra engine API.

## Table of contents

- [File map](#file-map)
- [Architecture and build](#architecture-and-build)
- [Navigation](#navigation)

## File map

| File | Responsibility |
| --- | --- |
| [menu.h](menu.h) | Bundled variant and difficulty menu adapter |
| [session.h](session.h) | Bundled factory dispatch and initial camera configuration |

## Architecture and build

See the [architecture guide](../../docs/launcher-overlays.md) for ownership and dependency contracts. Build through the repository root; the containing application or launcher CMake file defines these targets.

## Navigation

[Parent guide](../README.md) · [Project overview](../../README.md) · [Documentation index](../../docs/README.md)
