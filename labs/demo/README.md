# Demo

Reference application for public Kinetra physics, navigation, camera, runtime and UI APIs. [Setup and commands](../../README.md#quick-start) are maintained in the root README.

## Table of contents

- [Visual overview](#visual-overview)
- [Controls](#controls)
- [Scene ownership and streaming](#scene-ownership-and-streaming)
- [Navigation and agents](#navigation-and-agents)
- [Verification and limits](#verification-and-limits)

[Parent guide](../README.md) · [Project overview](../../README.md) · [Documentation index](../../docs/README.md)

Directory guides: [include](include/README.md), [src](src/README.md), [tests](tests/README.md).

## Visual overview

The session adapts input and rendering; the headless application owns simulation policy.

```mermaid
flowchart LR
    Input["Input and modal capture"] --> Session["Demo session"]
    Session --> Commands["FrameCommands"]
    Commands --> App["SandboxApplication"]
    App --> Scene["Sandbox: terrain, agents, physics"]
    Scene --> Engine["Kinetra navigation and simulation"]
    Scene --> Renderer["Sandbox renderer"]
    Renderer --> Runtime["Kinetra Runtime and overlays"]
```

## Controls

| Input | Behavior |
| --- | --- |
| WASD / arrows | Move player; move camera only in debug Ghost |
| Q/E or Shift+Left/Right | Rotate; manual input cancels walking |
| Click | Select agent, command selected agent, otherwise walk player |
| Click selected agent again | Clear selection |
| Right-click | Clear selection |
| Click in Ghost | Teleport real player to free visible destination, when settings is closed |
| SETTINGS button / Ctrl | Open/close centered settings; pause while open |
| Settings Statistics / Debug Mode | Toggle statistics / enable debug tools |
| Settings Frame Times / Frame Limit | Show the frame-time graph / pace frames where the platform has no VSync |
| Shift+Ctrl | Open/close the upper-left debug menu; switches Debug Mode on when it was off |
| DEBUG button (next to Settings) | Toggle the debug menu in Debug Mode |
| Wheel / ZOOM row of the debug menu | Zoom in Debug Mode: 5–100 px/unit; leaving Debug Mode clamps to 17–100 |
| Settings Launcher / Exit | Return to bundled launcher / exit the application |

Settings shows the help lines of SIM CONTROLS and DEBUG CONTROLS only after a click on their heading, and consumes all gameplay input. Resume or Ctrl closes settings and resumes the run. Space and M have no gameplay action. F/P/R/G/Tab/C have no gameplay shortcuts.

Demo starts with Debug Mode, statistics and the layers VECTORS, STEERING PROBES, PATHS, COLLISION SHAPES and ACTIVE AGENTS on. Ghost Camera, Gravity, HOME RANGE and CLICK SETS GOAL start off; the debug menu starts closed.

### Debug menu

The menu is a tree with one section open at a time; SIMULATION is open at the start. [`DebugPanel`](include/debug_panel.h) builds it in the logic layer, so its rows are tested without a window ([launcher and overlays](../../docs/launcher-overlays.md#debug-menu)).

| Section | Rows |
| --- | --- |
| SIMULATION | RUNNING (pause and resume), SINGLE TICK (pauses, then runs one 120 Hz tick per click), SPEED (time scale 0.125× to 4×), GRAVITY |
| LAYERS | VECTORS, STEERING PROBES, PATHS, COLLISION SHAPES, ACTIVE AGENTS, HOME RANGE, each with the color it is drawn in. HOME RANGE draws a line from every awake agent to its home, and the range circle of the selected agent |
| MOTION | AVOIDANCE (RECIPROCAL or LEGACY), MAX SPEED, ACCELERATION, RESPONSE, CLEARANCE; then HORIZON and PASSING BIAS for reciprocal avoidance, or LOOKAHEAD, SEPARATION and SEP WEIGHT for legacy steering |
| AGENTS | WANDER (STEERING or ROUTE), HOME RANGE, then WANDER REACH and WANDER JITTER for steering wander, and CLICK SETS GOAL |
| Below the rule | ZOOM, GHOST CAMERA, RESET PARAMETERS (the default motion and wander values, reciprocal avoidance, speed 1×) |

The MOTION and AGENTS rows change the scene's [`Tuning`](include/tuning.h) between ticks, within limits the steering code accepts (`tuning_step` in [debug_panel.h](include/debug_panel.h)); the defaults are the values the scene used as constants before. Debug Mode gates layers, gravity and Ghost; leaving it restores player following. The tuned values, the avoidance model and the time scale persist outside Debug Mode. CLICK SETS GOAL makes a click command every awake agent; switch it off for individual selection. LEGACY compares the nearest-obstacle steering with the reciprocal avoidance in the same scene. The statistics line shows PAUSED or the time scale when the scene does not run in real time.

The camera follows the player by default. Ghost moves only the camera; clicking teleports the player and clears momentum/routes after validating destination clearance. No swept teleport path is required. A yellow outline identifies the selected agent. Overlay pointer capture precedes gameplay mapping.

## Scene ownership and streaming

See [Demo architecture](../../docs/demo-architecture.md) for component ownership, lifecycle and the engine boundary.

Drawing runs between fixed ticks: `SandboxApplication` keeps every body's pose before and after the last 120 Hz tick, and the renderer draws moving bodies at `presented_pose(id)`, with the camera on the drawn player for that frame ([drawing between ticks](../../docs/architecture.md#session-ownership-and-frame-flow)).

Moving bodies now include circles, boxes, triangles, pentagons and hexagons. Polygon rendering and collision outlines use the same convex vertices. Circle and nonrotating box/polygon pairs use continuous contact detection on a shared timeline, including moving/moving pairs. Pairs containing rotating boxes/polygons still use discrete contacts. Steering and navigation remain conservative envelope-based; this is not yet a collision-free multi-agent or vehicle simulation.

Terrain uses 32-world-unit chunks with seeded static circles/rotated rectangles. Independent seed/coordinate streams regenerate identical geometry, including negative coordinates and revisits. Each process chooses and prints a new seed. Generation reserves the initial spawn area and rejects local overlaps; global navigability is not guaranteed.

| Region / bound | Policy |
| --- | --- |
| Physics terrain residency | Player-centered 1.25× maximum debug view |
| Ghost preview | Separate cached visible terrain, without distant physics/population |
| Agent activation | Body-AABB overlap with 1.75× maximum normal player view |
| Agent retention | Evict beyond 1.25× maximum debug player view |
| Navigation | Activation area plus 24-unit wander margin |
| Population | 48 initial agents near the player; new activation chunks attempt 12 safe spawns; cap 1,200 |

The launcher’s `--agents 0..300` overrides the default initial population. The standalone Demo also starts with 48 agents. Initial agents sample a 72×48-unit region centered on the initial player, with reserved player/prop clearances. Zoom and Ghost position do not affect player-centered lifecycle bounds; viewport size does. Dormant agents discard paths, ignore new goals and freeze timers. During physics they temporarily become fixed obstacles; full body state is restored afterward. Distant-agent eviction compacts movers and remaps surviving agent/player indices. Terrain rebuilding preserves movers before appending static obstacles.

Terrain validates finite ordered views, ±200,000-unit bounds and a maximum 4,096 chunks. Camera centers are clamped to ±100,000. Neither agent state nor world edits persist after eviction; revisits can create fresh agents. Physics outside resident terrain is inactive.

## Navigation and agents

The reusable [planner](../../external/Kinetra/docs/engine/navigation.md) owns A*, swept clearance and route shortening. Demo selects one-unit spacing and a conservative 0.85-unit circular envelope. Exact free clicks stay exact; occupied clicks project to the closest reachable free node. Outside/disconnected free goals fail. Outward recovery can leave conservative clearance margins without crossing a surface.

| Decision | Demo policy |
| --- | --- |
| Home | Where the agent was placed. Beyond its home range (12 units, tunable) it turns back, so the population stays spread over the terrain instead of drifting away from where it was created |
| Wander by steering (default) | No route. The agent aims at a point on a circle ahead of it: WANDER REACH (3 units) along its direction of travel, half of that out at a wander angle that drifts by a seeded random amount of at most WANDER JITTER (4 rad/s). Beyond 60 % of the home range the direction blends toward home, fully at the range |
| Stalled steering wanderer | Slower than 30 % of its top speed, an agent turns its heading on the spot at 2 rad/s, and the pull toward home fades over one second, so it walks out of a pocket between terrain before it heads home again |
| Wander by route | Persistent seeded heading with bounded turns; short reachable goals 4–8 units away; beyond the home range a leg starts toward home |
| Route sampling | At most 12 candidates per choice; retry failed choices after two simulated seconds |
| Replan | Every two simulated seconds and after terrain invalidation |
| Route wander timeout | 0.8–1.6 active simulated seconds; bounded acceleration smooths frequent goal changes into curves |
| Command timeout | 20 active simulated seconds |
| Arrival / expired command | Resume wandering from the current/arrival location; the home range then brings the agent back |
| Failed route | Hold position and retry while the relevant policy permits |

Commanded travel (a click on a selected agent, or CLICK SETS GOAL) always plans a route, because its goal can lie behind obstacles. Route following looks ahead around visible corners and brakes only at the final endpoint. Agents use the shared [reciprocal local avoidance](../../external/Kinetra/docs/engine/navigation.md#reciprocal-local-avoidance) with, by default, a 1.5-second horizon, speed limit 4, acceleration limit 12 and 0.05-unit clearance. All active agents read the same neighbor snapshot. Sleeping agents are stationary noncooperative neighbors; the manually controlled player and passive props also receive full avoidance responsibility from agents. Keyboard player control remains direct. The legacy comparison mode retains its personal-space separation.

Avoidance considers moving-body envelopes. A route keeps its own clearance from terrain, so an agent on a route looks at movers only. An agent that wanders by steering has no route and also avoids the terrain near it: as bodies for the legacy steering, and for the reciprocal avoidance as noncooperative circles, a rotated box as a row of up to six circles along its long side. The scene keeps a spatial index of the loaded terrain for this and rebuilds it only when terrain changes. Neighbor reach includes the horizon, the largest actor footprint and current actor speeds. `AVOID LIMIT` in statistics counts active agents whose requested avoidance constraints could not all be met in the last fixed tick; it does not count physical collisions. Headings turn gradually. Global pause consumes elapsed clock time without advancing simulation, commands or wander timers; a single tick runs one tick of a paused scene.

## Verification and limits

[Crowd stability tests](tests/crowd_stability_tests.cpp) exercise 60 simulated seconds with each of seeds 123 and 456, including the spinning tangent-contact freeze regression, and check that the agents around a standing player are still there after a minute, in both wander modes.

[Crowd characterization tests](tests/crowd_characterization_tests.cpp) pin the current crowd behavior in both wander modes with per-frame invariants and a trajectory fingerprint, so the planned Demo refactor can prove that it preserves behavior.

[Terrain tests](tests/terrain_tests.cpp) cover deterministic streaming, compaction, sleep/wake, wandering in both modes and the home range, commands, detours, clearance, recovery and arrival. [Application tests](tests/sandbox_application_tests.cpp) cover pause, time scale and single tick, camera, pointer ordering, bindings and tuning. [Debug panel tests](tests/debug_panel_tests.cpp) cover the menu's rows, their limits and the commands they produce. [Presentation tests](tests/presentation_tests.cpp) cover rendered scene/debug/help behavior.

The circular planning envelope can reject narrow gaps; moving crowds may prevent arrival. An agent whose home lies within its range of the edge of the active area can still walk out and fall asleep there, and in a dense crowd agents stray up to about ten units beyond their range for moments. The fixed clock runs at most 16 ticks per frame, so at the fastest time scale of 4 simulated time falls behind below 30 frames per second. Rendering culls offscreen bodies and reuses Ghost chunks. Physics and steering still consume resident data. Desktop feel/performance requires manual Artix checks. Decisions behind this design: [terrain](../../external/Kinetra/docs/decisions/0002-camera-driven-terrain.md), [application boundary](../../external/Kinetra/docs/decisions/0004-sandbox-application-boundaries.md), [agent lifecycle](../../external/Kinetra/docs/decisions/0005-sandbox-agent-lifecycle.md).
