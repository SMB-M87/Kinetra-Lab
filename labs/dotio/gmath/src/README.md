# Dotio math compatibility implementation

Sources in this directory belong to `kinetra_dotio_logic`. These are Lab-owned interfaces and implementations; they are not part of the public Kinetra engine API.

## Table of contents

- [File map](#file-map)
- [Architecture and build](#architecture-and-build)

## File map

| File | Responsibility |
| --- | --- |
| [gmath.cpp](gmath.cpp) | Grid coordinate conversion, neighbor partitions and compatibility operators |

## Architecture and build

See the [architecture guide](../../../../docs/dotio-architecture.md) for ownership and dependency contracts. Build through the repository root; the containing application or launcher CMake file defines these targets.
