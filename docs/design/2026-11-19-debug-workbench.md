# Debug workbench: tunable Demo and Dotio, tree menus everywhere, steering wander

Status: accepted · Branch: `feat/debug-workbench` · Related: [Kinetra design](../../external/Kinetra/docs/design/2026-11-19-debug-workbench.md), [launcher and overlays](../launcher-overlays.md), [i4sim debug menu](2026-11-19-i4sim-debug-menu.md), [smooth rendering](2026-11-19-smooth-rendering.md), [media capture](2026-11-19-media-capture.md)

i4sim lets the operator change motion settings while a scene runs; Demo and Dotio only switch drawing layers on and off. The user wants all three to be workbenches: the same tree menu, live parameters, a shortcut that works on every keyboard, zoom as a debug tool, and agents that stay around. This is phase 1 of two; phase 2 regenerates the tour video on top of it.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

- **Demo and Dotio cannot be tuned while running.** Demo's steering values are literals inside `Sandbox::update` ([sandbox_scene.h](../../labs/demo/include/sandbox_scene.h)); Dotio's balance is a `constexpr` preset ([difficulty.h](../../labs/dotio/logic/include/difficulty.h)) and its state forces are literals in [state.cpp](../../labs/dotio/logic/src/state.cpp). Their debug menus are flat lists of check boxes ([Demo session](../../labs/demo/src/session.cpp), [Dotio application](../../labs/dotio/include/application.h)); Dotio indents its PATHS filters with spaces.
- **Demo's agents leave.** An agent wanders with a persistent heading and never uses its home ([agent.h](../../labs/demo/include/agent.h), `choose_wander_goal`). It walks out of the active area around the player, where agents sleep, and a chunk is populated only once ([sandbox_scene.h](../../labs/demo/include/sandbox_scene.h), `populated_`). The user's screenshot shows 8 of 566 agents active.
- **Wandering plans routes.** Every wander leg in Demo is a navigation query, and Dotio's WANDER state runs A* to a random cell. The user expects wandering to be a steering behavior.
- **Backquote and the ZOOM row.** The debug menu opens with Backquote, and only after Debug Mode was switched on in the settings panel. Zoom sits in the settings panel although the wheel already zooms only in Debug Mode.
- **Uneven frames in WSLg** and **no icon in the WSLg title bar**: causes and measurements are in the [Kinetra design](../../external/Kinetra/docs/design/2026-11-19-debug-workbench.md#problem).

## Goals and non-goals

- Goal: Demo, Dotio and i4sim show the same tree menu: SIMULATION, LAYERS and the application's tuning sections, then ZOOM and the application's commands.
- Goal: Demo's motion and wander values and Dotio's balance and state forces change from the menu while the scene runs, and reset to their defaults from the menu.
- Goal: Demo and Dotio can pause, run one tick and change their time scale from the menu.
- Goal: agents wander by steering by default, in Demo around their home; the planned-route wander stays selectable for comparison.
- Goal: Shift+Ctrl opens the debug menu in every application; the settings panel loses ZOOM and gains FRAME LIMIT.
- Goal: every screenshot and clip that changed is rendered again.
- Non-goal: seeding the rest of Dotio's randomness ([quality roadmap](2026-11-18-quality-roadmap.md)). Only the new wander code uses the world's seed.
- Non-goal: steering for Dotio's PURSUE, RECOVER and KAMIKAZE states. They have to get around walls to a place and keep their paths.
- Non-goal: saving tuned values. A new session starts from the defaults.
- Non-goal: the tour video (phase 2, its own branch and design document).

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Shortcut | Shift+Ctrl opens and closes the debug menu and switches Debug Mode on when it was off | User's choice |
| Menu shape | A tree like i4sim's, one section open at a time | User's choice (alternatives: tabs, a full-height side bar) |
| Menu mechanics | In Kinetra (`DebugEntry`, `DebugFolds`); Lab supplies entries | Three applications need it ([AGENTS.md](../../AGENTS.md#scope)) |
| Zoom | A ZOOM row with `[-] [+]` in every debug menu; the ZOOM row leaves the settings panel. Leaving Debug Mode clamps the zoom to the normal limits, as before | User: zoom belongs to the debug view. The clamp already existed |
| Demo agents | Patrol around their home within a tunable range | User's choice (alternatives: respawn around the player, turn back at the edge of the active area) |
| Demo wander | By steering (default) or by planned route, chosen in the menu. Commanded travel always plans a route | User prefers steering and wants to compare. A commanded goal can lie behind obstacles |
| Dotio wander | By steering with wall feelers (default) or by A* path, chosen in the menu | User's choice. The game changes: enemies roam more smoothly and may find the player later |
| Dotio balance | The session's world holds a copy of the difficulty preset that the menu changes | The preset stays the documented starting point of a difficulty |
| Frame limit | A FRAME LIMIT row in every settings panel, on by default; it acts only where VSync is unavailable | The user can compare both in WSLg. Assumption accepted by the user |
| Where menu content is built | In each application's logic layer, without a window | i4sim's controller already does; the rows become testable headless |
| Phasing | Phase 1: everything but the video. Phase 2: the video, stacked on this branch | User's choice |

## Design

### Shared Lab policy

```mermaid
flowchart LR
    subgraph Logic["logic layer (headless)"]
        Entries["debug entries<br/>DebugFolds, tuning"]
        Zoom["lab::ZoomControl"]
    end
    subgraph Presentation
        Session["lab session"]
        Overlay["kinetra::SettingsOverlay"]
        Menu["kinetra::DebugMenu"]
    end
    Session -->|"entries()"| Entries
    Session -->|"update_debug(input, entries)"| Overlay
    Overlay --> Menu
    Menu -->|click| Entries
    Entries -->|zoom steps, time scale, single tick| Session
```

*Takeaway:* a session only passes entries to the overlay; what a row does is decided where the state lives.

- **`lab/settings.h`.** `settings_rows(statistics, debug, frame_times, frame_limit, sim_controls, debug_controls)`: STATISTICS, DEBUG MODE, FRAME TIMES, FRAME LIMIT, SIM CONTROLS, DEBUG CONTROLS. DEBUG CONTROLS starts with `SHIFT + CTRL  DEBUG MENU` and `WHEEL  ZOOM`. A session passes its FRAME LIMIT flag to `Runtime::limit_frames` when it draws.
- **`lab/debug.h`** (new, headless). `ZoomControl` makes the ZOOM row: it shows the camera's zoom, collects the steps its buttons ask for, and hands them to the frame together with the wheel's. `time_scale_entry` makes the SPEED row over a borrowed factor (0.125 to 8, doubling). Logic layers may include it next to `lab/input.h`.

### Menus

Every menu starts with SIMULATION and LAYERS, and ends, below a rule, with ZOOM and the commands.

| | Demo | Dotio | i4sim |
| --- | --- | --- | --- |
| SIMULATION | RUNNING, SINGLE TICK, SPEED, GRAVITY | RUNNING, SINGLE TICK, SPEED, INVULNERABLE | RUNNING, SINGLE TICK, UPS, WORKLOAD (unchanged) |
| LAYERS | VECTORS, STEERING PROBES, PATHS, COLLISION SHAPES, ACTIVE AGENTS, HOME RANGE | VECTORS, STEERING PROBES, PATHS with WANDER, PURSUE, KAMIKAZE, RECOVERY (state colors as swatches), COLLISION SHAPES, ACTIVE AGENTS, AI STATES, HIDING HITS | Unchanged |
| Tuning | MOTION: AVOIDANCE (list: RECIPROCAL, LEGACY), MAX SPEED, ACCELERATION, RESPONSE, LOOKAHEAD, CLEARANCE, then HORIZON and PASSING BIAS (reciprocal) or SEPARATION and SEP WEIGHT (legacy) | ENEMIES: WANDER (list: STEERING, PATH), MOVE SPEED, FIRE INTERVAL, ARROW SPEED, PREDICTION, PURSUIT CYCLES, HITS, and the force of each state | MOTION and TRAFFIC (unchanged) |
| | AGENTS: WANDER (list: STEERING, ROUTE), HOME RANGE, WANDER REACH, WANDER JITTER, CLICK SETS GOAL | SPAWNING: ACTIVE LIMIT, SPAWN RATE, CROWD LOAD, SPACING | |
| Below the rule | ZOOM, GHOST CAMERA, RESET PARAMETERS | ZOOM, GHOST CAMERA, RESET PARAMETERS | ZOOM, FRAME SCENE, BLUEPRINTS |

A number row steps by a factor of 1.25 or by a fixed amount, whichever suits its unit, between limits that keep the value valid for the code that reads it.

### Demo

- **`tuning.h`.** `MotionTuning` (max speed, acceleration, response time, lookahead, clearance, separation radius and weight, avoidance horizon, passing bias) and `WanderTuning` (mode, home range, reach, jitter) with the scene's former literals as defaults, and `validate()`. `Sandbox::tuning` holds them; `SandboxApplication::tuning()` gives the menu access.
- **Steering wander.** An agent keeps a wander angle that drifts by a seeded random amount per tick. Its target is a point on a circle ahead of it: `reach` ahead along its direction of travel, half of `reach` out at the wander angle. Beyond 60 % of the home range the direction is blended toward home, fully at the range, so an agent turns back in a curve. No route is planned and none is drawn.
- **Obstacles without a route.** A planned route keeps its own clearance from terrain, so local avoidance looked at movers only. An agent that wanders by steering also gets the terrain near it: as bodies for the legacy steering, and as non-cooperative circles for the reciprocal avoidance, a long box as a row of circles. The scene keeps a spatial grid of the terrain it loaded and rebuilds it only when terrain changes.
- **Route wander.** Unchanged, except that a leg starts toward home when the agent is beyond its range.
- **Time.** `FrameCommands` gets `time_scale` and `single_step`: the fixed clock advances by scaled time, and a paused scene runs exactly one tick for a single step.
- **`debug_panel.h`** (logic). `DebugPanel` owns the layer flags, the folds, the zoom control and the time scale, builds the entries over a `SandboxApplication`, and fills the debug part of a frame's `FrameCommands`. The session keeps the settings overlay, the menu widget and the drawing.

### Dotio

- **Tuning.** `World` copies its difficulty preset into `settings()` at `Initialize` and keeps an `EnemyTuning` (wander mode and one force per state, with the former literals as defaults). `reset_tuning()` restores both. `difficulty_settings()` returns the copy, so every existing reader follows the menu.
- **Steering wander.** In WANDER an enemy keeps a heading. Every decision tick the heading drifts by a random amount drawn from a generator seeded with the world's seed. Every update a forward feeler of 0.9 cells and two side feelers of 0.6 cells at 0.6 rad are tested against the grid (`Grid::IsIntersectingAnyCell`): a blocked forward feeler turns the heading to the first free direction of a fixed fan, a blocked side feeler nudges it away. The enemy accelerates along its heading with the WANDER force. No path exists; seeing the player still starts ATTACK.
- **Probes.** STEERING PROBES draws the feelers the logic tests, from one function both use.
- **Application.** `Application` builds the entries (`debug_entries()`), gets `time_scale` and a single tick, and keeps the layer flags it has. PATHS and its parts become check rows at depth 1 and 2 with the state colors as swatches.

### i4sim

`MenuEntry` and `EntryKind` become `kinetra::DebugEntry` and `kinetra::DebugEntryKind`; the controller's open section and list move to `DebugFolds`; `layer_color` moves to [view.h](../../labs/i4sim/include/view.h) so the controller can fill `swatch`. The session's `DebugRows` adapter and `row_text` go away. The menu gains ZOOM; its rows and keys are otherwise unchanged.

### Capture tool and media

- Scripts open the debug menu with Shift+Ctrl and click rows of the new menus; the row constants in `scenes.h` follow, with their tests.
- Demo's and Dotio's debug screenshots show their new menus with a tuning section open; the settings screenshot shows the panel without ZOOM.
- Every screenshot and clip is rendered again, because the panels, the toolbar, Demo's agents and Dotio's wander all changed.

## Affected areas

- `external/Kinetra` pointer ([Kinetra design](../../external/Kinetra/docs/design/2026-11-19-debug-workbench.md)).
- Shared: [lab/settings.h](../../labs/common/include/lab/settings.h), new `lab/debug.h`, [common tests](../../labs/common/tests/common_tests.cpp).
- Demo: new `tuning.h` and `debug_panel.h`, [agent.h](../../labs/demo/include/agent.h), [sandbox_scene.h](../../labs/demo/include/sandbox_scene.h), [sandbox_commands.h](../../labs/demo/include/sandbox_commands.h), [sandbox_application](../../labs/demo/src/sandbox_application.cpp), [session.cpp](../../labs/demo/src/session.cpp), [sandbox_renderer](../../labs/demo/src/sandbox_renderer.cpp), tests.
- Dotio: [difficulty.h](../../labs/dotio/logic/include/difficulty.h), [world](../../labs/dotio/logic/src/world.cpp), [enemy](../../labs/dotio/logic/src/enemy.cpp), [state.cpp](../../labs/dotio/logic/src/state.cpp), [application.h](../../labs/dotio/include/application.h), [presentation.cpp](../../labs/dotio/src/presentation.cpp), [session.cpp](../../labs/dotio/src/session.cpp), tests.
- i4sim: [controller](../../labs/i4sim/src/controller.cpp), [view.h](../../labs/i4sim/include/view.h), [session.cpp](../../labs/i4sim/src/session.cpp), [world_view](../../labs/i4sim/src/world_view.cpp), tests.
- Launcher tests, [capture tool](../../tools/capture/README.md), [media](../media/README.md).
- Docs: [AGENTS.md](../../AGENTS.md), [README](../../README.md), [launcher and overlays](../launcher-overlays.md), the three module READMEs and architecture pages.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Bug: with a standing player, the agents active after 60 simulated seconds are at least 80 % of those at the start (fails before the fix) | `kinetra_sandbox_application`: agents stay around a standing player | Targeted |
| Steering wander plans no route, moves the agent, turns it back at its range, and is repeatable for a seed | `kinetra_terrain`: steering wander cases | Targeted |
| Route wander keeps its legs and starts toward home beyond the range | `kinetra_terrain` (existing wander cases, in route mode) | Targeted |
| A steering wanderer walks around terrain in both avoidance models without deep overlap | `kinetra_sandbox_application`; `kinetra_demo_crowd_characterization` invariants | Targeted |
| Tuning: defaults equal the former literals; invalid values are rejected; a changed max speed changes how far an agent gets | `kinetra_sandbox_application` | Targeted |
| Time scale 0.5 runs half the ticks, a single step runs one tick while paused | `kinetra_sandbox_application`, `kinetra_dotio` | Targeted |
| Crowd runs stay finite and separated in both wander modes; their fingerprints are recorded anew | `kinetra_demo_crowd_characterization`, `kinetra_demo_crowd` (soak) | Targeted |
| Demo's menu: sections, one open at a time, every tuning row changes its value within its limits, RESET restores the defaults | `kinetra_sandbox_application`: debug panel cases | Targeted |
| Dotio: `settings()` starts as the preset, changes reach the enemies, `reset_tuning()` restores | `kinetra_dotio` | Targeted |
| Dotio steering wander: no path, moves, turns away from a wall, repeatable for a seed; path mode still plans | `kinetra_dotio` | Targeted |
| Dotio's menu rows and PATHS parts | `kinetra_dotio` (replaces the `debug_options` case) | Targeted |
| i4sim's menu rows are unchanged apart from ZOOM; swatches carry the layer colors | `kinetra_i4sim_controller` | Targeted |
| `settings_rows` order; `ZoomControl` steps and value; the SPEED row's limits | `kinetra_lab_common` | Targeted |
| Real sessions: Shift+Ctrl opens the menu and switches Debug Mode on, a row click reaches the application, settings rows by index | `kinetra_graphics`, `kinetra_demo_graphics`, `kinetra_dotio_graphics`, `kinetra_i4sim_graphics` | Graphics |
| Capture scripts click the rows they name | `kinetra_capture`, `kinetra_capture_smoke` | Targeted, graphics |

The engine pointer, sessions and presentations change: Full level with graphics, plus the Windows cross build because `Runtime` changed.

## Review

Filled in after the review.

## Verification

Filled in after verification.

## Pull request

Filled in when the branch is complete.

## Open points

- Dotio's other randomness is still unseeded ([quality roadmap](2026-11-18-quality-roadmap.md)).
