# Launcher implementation

Sources in this directory belong to `kinetra_sessions`. These are Lab-owned interfaces and implementations; they are not part of the public Kinetra engine API.

## Table of contents

- [Visual overview](#visual-overview)
- [File map](#file-map)
- [Architecture and build](#architecture-and-build)

[Parent guide](../README.md) · [Project overview](../../README.md) · [Documentation index](../../docs/README.md)

## Visual overview

The composition root validates launch arguments before the loop creates and drives sessions.

```mermaid
flowchart TD
    Main["main.cpp: arguments and seed"] --> Loop["session.cpp: Lab event loop"]
    Loop --> Select["Menu or requested variant"]
    Select --> Factory["Application factory and camera setup"]
    Factory --> Host["SessionHost owns session"]
    Host --> Frame["Poll input, advance, render"]
    Frame --> Action{"Session navigation"}
    Action -->|continue| Frame
    Action -->|return to launcher| Select
    Action -->|exit| Shutdown["Destroy session and runtime"]
```

## File map

| File | Responsibility |
| --- | --- |
| [session.cpp](session.cpp) | Application factories, camera setup and settings-driven launcher loop |
| [main.cpp](main.cpp) | CLI validation, seed selection and bundled executable entry point |

## Architecture and build

See the [architecture guide](../../docs/architecture.md) for ownership and dependency contracts. Build through the repository root; the containing application or launcher CMake file defines these targets.
