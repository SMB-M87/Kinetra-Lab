# Dotio game logic headers

Sources in this directory belong to `kinetra_dotio_logic`. These are Lab-owned interfaces and implementations; they are not part of the public Kinetra engine API.

## Table of contents

- [File map](#file-map)
- [Architecture and build](#architecture-and-build)
- [Navigation](#navigation)

## File map

| File | Responsibility |
| --- | --- |
| [world.h](world.h) | World ownership, streaming, pools and progression |
| [enemy.h](enemy.h) | Enemy state and behavior interface |
| [state.h](state.h) | Five AI state implementations and transitions |
| [player.h](player.h) | Player abilities and resources |
| [arrow.h](arrow.h) | Enemy projectile contract |
| [bolt.h](bolt.h) | Player projectile contract |
| [graph.h](graph.h) | Local pathfinding and shelter search |
| [grid.h](grid.h) | Static occupancy raster and seeded terrain |
| [difficulty.h](difficulty.h) | Immutable difficulty presets |
| [fuzzy.h](fuzzy.h) | Fuzzy combat and density decisions |
| [tracker.h](tracker.h) | Reusable population storage |
| [renderer.h](renderer.h) | Legacy drawing abstraction |
| [physics_object.h](physics_object.h) | Base physical object contract |
| [moving_object.h](moving_object.h) | Moving actor contract |
| [static_object.h](static_object.h) | Static object contract |
| [priority_queue.h](priority_queue.h) | Path search frontier storage |
| [stack.h](stack.h) | Path storage |
| [vertex.h](vertex.h) | Graph node representation |

## Architecture and build

See the [architecture guide](../../../../docs/dotio-architecture.md) for ownership and dependency contracts. Build through the repository root; the containing application or launcher CMake file defines these targets.

## Navigation

[Parent guide](../README.md) · [Project overview](../../../../README.md) · [Documentation index](../../../../docs/README.md)
