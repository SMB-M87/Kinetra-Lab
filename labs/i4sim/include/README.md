# Application headers

## Table of contents

- [Visual overview](#visual-overview)
- [Contents and use](#contents-and-use)

[Parent guide](../README.md) · [Project overview](../../../README.md) · [Documentation index](../../../docs/README.md)

## Visual overview

Simulation owns model and runtime state; Session construction stays at the graphical boundary.

```mermaid
classDiagram
    Simulation *-- Blueprint : blueprint_
    Simulation *-- Mover : movers_
    Simulation *-- Producer : producers_
    Simulation *-- Product : products_
    Simulation *-- WeightedGrid : grid_
    Simulation *-- FixedClock : clock_
    Simulation *-- MotionSettings : motion_
    Mover *-- Body : body
    Mover --> Product : optional product id
    Producer --> Product : queue and optional product id
```

## Contents and use

`blueprint.h` defines the validated scene/model data. `scenario.h` reads and applies the optional showcase setup of a blueprint. `simulation.h` exposes headless factory state and operations. `session.h` is the runtime-only session factory. Engine algorithms are consumed through public Kinetra headers.

`factory_settings.h` owns the factory-specific modal controls and input layout; shared Lab settings remain unchanged. `simulation.h` exposes motion profiles, bounded parameter configuration, manual reservations, optional parking/junction/corridor traffic coordination with observable exit reservations and temporary retreat targets and contact/wait statistics.

`MotionSettings::predictive_passing` enables the optional open-floor policy.
`Simulation::predictive_pairs()` and `passing_target(mover)` expose its current
commitments for inspection; `Mover::coordination.passing` identifies participants.
Destination and route ownership remain in Simulation; temporary targets are separate.
See the [passing contract](../../../docs/factory-simulation-architecture.md#predictive-passing).
