# Demo implementation

Sources in this directory belong to `kinetra_sandbox_application and kinetra_demo_presentation`. These are Lab-owned interfaces and implementations; they are not part of the public Kinetra engine API.

## Table of contents

- [File map](#file-map)
- [Architecture and build](#architecture-and-build)
- [Navigation](#navigation)

## File map

| File | Responsibility |
| --- | --- |
| [sandbox_application.cpp](sandbox_application.cpp) | Command, camera, pointer and fixed-update ordering |
| [sandbox_renderer.cpp](sandbox_renderer.cpp) | World rendering, culling and application overlays |
| [session.cpp](session.cpp) | Session adapter, bindings and debug controls |
| [main.cpp](main.cpp) | Direct executable and argument handling |

## Architecture and build

See the [architecture guide](../../../docs/demo-architecture.md) for ownership and dependency contracts. Build through the repository root; the containing application or launcher CMake file defines these targets.

## Navigation

[Parent guide](../README.md) · [Project overview](../../../README.md) · [Documentation index](../../../docs/README.md)
