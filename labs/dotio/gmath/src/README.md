# Dotio math compatibility implementation

Sources in this directory belong to `kinetra_dotio_logic`. These are Lab-owned interfaces and implementations; they are not part of the public Kinetra engine API.

## Table of contents

- [Visual overview](#visual-overview)
- [File map](#file-map)
- [Architecture and build](#architecture-and-build)

[Parent guide](../README.md) · [Project overview](../../../../README.md) · [Documentation index](../../../../docs/README.md)

## Visual overview

These operations translate legacy coordinates and maintain spatial partitions.

```mermaid
flowchart LR
    Coordinates["Legacy positions and vectors"] --> Conversion["gmath.cpp: grid coordinate conversion"]
    Conversion --> Grid["Grid entries"]
    Grid --> Partitions["Neighbor partition queries"]
    Partitions --> Logic["Dotio spatial queries"]
```

## File map

| File | Responsibility |
| --- | --- |
| [gmath.cpp](gmath.cpp) | Grid coordinate conversion, neighbor partitions and compatibility operators |

## Architecture and build

See the [architecture guide](../../../../docs/dotio-architecture.md) for ownership and dependency contracts. Build through the repository root; the containing application or launcher CMake file defines these targets.
