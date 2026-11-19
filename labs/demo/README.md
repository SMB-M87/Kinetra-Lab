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
| Settings zoom buttons | Adjust camera scale: 17–100 px/unit normally; 5–100 in debug |
| Settings Statistics / Debug Mode | Toggle statistics / enable debug tools |
| DEBUG button (next to Settings) | Toggle the upper-left debug menu in Debug Mode |
| Wheel | Zoom during gameplay only in Debug Mode |
| Settings Launcher / Exit | Return to bundled launcher / exit the application |

Settings reveals centered help controls only after clicking SHOW SIM CONTROLS or SHOW DEBUG CONTROLS, and consumes all gameplay input. Resume or Ctrl closes settings and resumes the run. Space and M have no gameplay action. Statistics and Debug Mode are configured exclusively in settings.

Demo starts with Debug Mode, statistics and all visual layers enabled. Ghost Camera, Agent Goal and Gravity remain off; the debug menu starts collapsed.

Debug rows, in order: Ghost Camera, Vectors, Steering Probes, Paths, Collision Shapes, Active Agents, Agent Goal, Gravity, Reciprocal Avoid. Debug Mode gates visualization/Ghost; leaving it restores player following. Agent Goal makes clicks command all active agents; disable it for individual selection. Reciprocal Avoid starts enabled; disable it to compare the legacy nearest-obstacle steering in the current scene. This behavior setting persists when visual debug mode is disabled. F/P/R/G/Tab/C have no gameplay shortcuts.

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
| Wander | Persistent seeded heading with bounded turns; short reachable goals 4–8 units away |
| Sampling | At most 12 candidates per choice; retry failed choices after two simulated seconds |
| Replan | Every two simulated seconds and after terrain invalidation |
| Wander timeout | 0.8–1.6 active simulated seconds; bounded acceleration smooths frequent goal changes into curves |
| Command timeout | 20 active simulated seconds |
| Arrival / expired command | Resume wandering from the current/arrival location, without returning home |
| Failed route | Hold position and retry while the relevant policy permits |

Route following looks ahead around visible corners and brakes only at the final endpoint. Agents use the shared [reciprocal local avoidance](../../external/Kinetra/docs/engine/navigation.md#reciprocal-local-avoidance) with a 1.5-second horizon, speed limit 4, acceleration limit 12 and 0.05-unit clearance. All active agents read the same neighbor snapshot. Sleeping agents are stationary noncooperative neighbors; the manually controlled player and passive props also receive full avoidance responsibility from agents. Keyboard player control remains direct. The legacy comparison mode retains its personal-space separation.

Avoidance considers moving-body envelopes; static terrain is handled by routes and physics. Neighbor reach includes the horizon, the largest actor footprint and current actor speeds. `AVOID LIMIT` in statistics counts active agents whose requested avoidance constraints could not all be met in the last fixed tick; it does not count physical collisions. Headings turn gradually. Global pause consumes elapsed clock time without advancing simulation, commands or wander timers.

## Verification and limits

[Crowd stability tests](tests/crowd_stability_tests.cpp) exercise 60 simulated seconds with each of seeds 123 and 456, including the spinning tangent-contact freeze regression.

[Crowd characterization tests](tests/crowd_characterization_tests.cpp) pin the current crowd behavior with per-frame invariants and a trajectory fingerprint, so the planned Demo refactor can prove that it preserves behavior.

[Terrain tests](tests/terrain_tests.cpp) cover deterministic streaming, compaction, sleep/wake, wandering, commands, detours, clearance, recovery and arrival. [Application tests](tests/sandbox_application_tests.cpp) cover pause, camera, pointer ordering and bindings. [Presentation tests](tests/presentation_tests.cpp) cover rendered scene/debug/help behavior.

The circular planning envelope can reject narrow gaps; moving crowds may prevent arrival. Rendering culls offscreen bodies and reuses Ghost chunks. Physics and steering still consume resident data. Desktop feel/performance requires manual Artix checks. Decisions behind this design: [terrain](../../external/Kinetra/docs/decisions/0002-camera-driven-terrain.md), [application boundary](../../external/Kinetra/docs/decisions/0004-sandbox-application-boundaries.md), [agent lifecycle](../../external/Kinetra/docs/decisions/0005-sandbox-agent-lifecycle.md).
