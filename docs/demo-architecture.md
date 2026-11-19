# Demo architecture

Application ownership and its boundary with the public Kinetra engine.

## Table of contents

- [Components](#components)
- [Lifecycle and constraints](#lifecycle-and-constraints)
- [Source navigation](#source-navigation)

[Directory guide](README.md) · [Project overview](../README.md) · [Documentation index](README.md)

## Components

```mermaid
flowchart LR
    Session[DemoSession: panels and frame order] --> Panel[DebugPanel: menu rows, layers, tuning]
    Panel --> App
    Session --> App[SandboxApplication: pause / clock / camera / pointer order]
    App --> Scene[Sandbox: population and scene policy]
    Scene --> Terrain[Seeded Terrain chunks]
    Scene --> Agents[Agent values and body handles]
    Scene --> Engine[Public World / Navigation / steer]
    Render[Sandbox renderer] -->|const observations| App
    Render --> Runtime[Public Runtime and overlays]
```

Session/presentation consume public engine APIs. World generation and agent decisions remain local. The debug menu's content is logic: `DebugPanel` builds its rows, holds what they set and fills the debug part of a frame's commands, without a window. Source entry points: [application](../labs/demo/include/demo/sandbox_application.h), [scene](../labs/demo/include/demo/world/sandbox_scene.h), [tuning](../labs/demo/include/demo/world/tuning.h), [agent](../labs/demo/include/demo/world/agent.h), [terrain](../labs/demo/include/demo/world/terrain.h), [debug panel](../labs/demo/include/demo/debug_panel.h), [session](../labs/demo/src/presentation/session.cpp).

## Lifecycle and constraints

SandboxApplication owns Sandbox and a fixed clock. Sandbox owns the physics World, Navigation snapshot, terrain and agent values. Body indices must be remapped when scene compaction evicts agents. Terrain rebuilding preserves movers before appending static geometry.

The application orders commands, camera updates and pointer interpretation before fixed simulation ticks. Pausing consumes elapsed clock time without advancing simulation; a frame may scale simulated time or run a single tick of a paused scene. The scene reads its `Tuning` at every tick and rejects values its steering cannot run with. Player-centered residency controls physical terrain and population; Ghost preview uses a separate terrain cache so camera exploration does not activate distant simulation.

## Source navigation

Start with the [application guide](../labs/demo/README.md), [headers](../labs/demo/include/README.md) and [presentation sources](../labs/demo/src/README.md). See the [system architecture](architecture.md) for target dependencies and shared session flow.
