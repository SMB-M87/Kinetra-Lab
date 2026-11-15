# Dotio architecture

Application ownership and its boundary with the public Kinetra engine.

## Table of contents

- [Components](#components)
- [Lifecycle and constraints](#lifecycle-and-constraints)
- [Source navigation](#source-navigation)

## Components

```mermaid
flowchart LR
    Session[DotioSession: input and debug capture] --> App[Application: stable World, clocks and commands]
    App --> Rules[World / five AI states / combat / spawning]
    Rules --> Adapter[Legacy pixel/frame physics adapter]
    Adapter --> Sim[Kinetra public simulation API]
    Render[Presentation and legacy drawing adapter] --> Runtime[Kinetra Runtime / bitmap / debug / help]
    Rules --> Graph[Local graph and fear-biased HideStar]
```

Movement consumes 60 Hz ticks; AI runs at 20 Hz, sequentially on the owning thread. Legacy Y-down pixels map to Kinetra Y-up world units at 25 pixels/unit. [physics_adapter.h](../labs/dotio/include/physics_adapter.h) converts state; engine integration, contacts, impulses, circle sweeps and raster queries perform reusable mechanics. Legacy field names and grid conversion policies stay local; canonical vector operations delegate to Kinetra.

Source: [Application](../labs/dotio/include/application.h), [session](../labs/dotio/src/session.cpp), [presentation](../labs/dotio/src/presentation.cpp), [logic](../labs/dotio/logic/). The engine imports none of these headers.

## Lifecycle and constraints

Application owns logic::World through a unique_ptr to keep the address stable: enemies and AI states reference that world. World owns actor/projectile pools, navigation graph and props. Static Grid storage limits the process to one active Dotio world. Return to the launcher menu and destroy the session before creating its replacement.

Streaming rebases actors, projectiles, props and the camera together; routes and spatial partitions rebuild. Absolute district coordinates retain seeded terrain identity while local coordinates bound the active simulation. Presentation adapts legacy drawing to Runtime; game rules and the graph remain independent of SDL/OpenGL.

## Source navigation

Start with the [application guide](../labs/dotio/README.md), [headers](../labs/dotio/include/README.md) and [presentation sources](../labs/dotio/src/README.md). The [logic headers](../labs/dotio/logic/include/README.md) and [math adapter](../labs/dotio/gmath/include/README.md) describe the retained port boundary. See the [system architecture](architecture.md) for target dependencies and shared session flow.
