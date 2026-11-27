# Debug layers under bodies

Status: implemented · Branch: `ccr-7e47a2ec-fcqma6` · Related: [system architecture](../architecture.md#session-ownership-and-frame-flow)

The PATHS layer of Demo, Dotio and i4sim drew every route over the bodies, and the steering probes, velocity vectors, i4sim's forces and its detection ranges did the same, so in a crowd they hid the players, agents and movers they crossed, and it was hard to see what happened. Each application now draws them under its moving bodies, and only the start of a body's own route, vector or line over that body, so it stays linked to its owner while its neighbors stay visible. On the same branch, route lines get one width in all three applications and i4sim's `MAS-demonstrator` fixture becomes `test-environment`.

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

The user reported that the path debug view covers players and agents, so with many agents close together it no longer shows what happens. Every application drew its routes after its bodies:

- Demo: `render` in `labs/demo/src/presentation/renderer.cpp` drew all bodies, then the probes, then the player's route and every active agent's route.
- Dotio: `render` in `labs/dotio/src/presentation/presentation.cpp` drew the enemies, arrows, bolts and the player through `World::draw`, then each enemy's path in the debug loop.
- i4sim: `draw_movers` in `labs/i4sim/src/presentation/world_view.cpp` drew each mover and then its route, so a route covered every mover listed before its own.

The new graphics tests reproduce it on the old code: the player's route covers a Demo agent it crosses (pixel in the route color), a wandering Dotio enemy's path covers the player (pixel `0 128 255`, the WANDER color, where the player is `102 0 255`), and an i4sim route covers a mover listed before its owner (pixel `178 102 255`).

## Goals and non-goals

Goals:

- No route of the PATHS layer hides a player, an agent, an enemy, a mover or a prop, in all three applications.
- The start of a body's own route is drawn over that body, so it stays visible whose route it is; a neighbor drawn later covers it again.
- Routes still cross the terrain (Demo's fixed bodies, Dotio's walls, i4sim's floor and producers) as before.
- The steering probes of Demo and Dotio and i4sim's DETECTION circles go under the moving bodies too, as the user asked after the first round; they get no start over their own body, because they already start at its center and stay visible outside it.
- The velocity vectors of Demo and Dotio, and i4sim's target arrows (RETREAT TARGETS, PASSING TARGETS) and FORCES parts (SEEK, COLLISION, BORDER, PREDICTIVE, VELOCITY, ACCELERATION), go under the moving bodies too, as the user asked after the second round. Right after each body, the part of its own arrow or line within the body is drawn again, so a line shorter than the body, such as a slow mover's VELOCITY line, does not disappear under it.
- One route line width for all three applications: 2.5 screen pixels at every zoom. The user found Demo's routes (3 pixels) too thick, and i4sim's (2 pixels) and Dotio's (0.05 world units: 1.25 pixels at the normal zoom, a fraction of a pixel zoomed out in Debug Mode) too thin.

Non-goals:

- Collision and contact outlines, selection markers, ACTIVE AGENTS dots, HOME RANGE and labels keep their place over the bodies.
- No change to which routes are shown or their colors, nor to the width of other debug lines.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Where the rule applies | Demo, Dotio and i4sim | Settled with the user: all three draw a PATHS layer over bodies. |
| How a route shows its owner | Every route under all moving bodies; the start of each route, up to where it leaves its body, drawn right after that body | Settled with the user. Drawing the start right after the body, not after all bodies, lets a neighbor drawn later cover it, so the start never lies over a neighbor. |
| Where the clipping lives | `kinetra_lab::route_start` in `labs/shared/include/kinetra_lab/route.h`, header-only and headless | Three applications need it, and it is Lab drawing policy. It is a candidate for Kinetra ([scope](../../AGENTS.md#scope)), but a pure clip of a polyline to a circle is too small to justify an engine pull request now. |
| The body's extent for the start | A circle: the body's radius; for a box the smaller half extent; for a Demo polygon the distance to its nearest edge; each less half the line's width | The largest circle around the position that stays inside the body. `route_start` clips the line's center, so the radius loses half the width, or the line's edge would reach that far past the body onto a neighbor (review round 1). |
| How Dotio draws over one enemy at a time | An optional `after_enemy` callback on `World::draw` | `World::draw` draws all enemies through the logic's `Renderer`; the presentation needs to draw between two enemies without copying the draw order of enemies, arrows, bolts and the player. |
| Route line width | `kinetra_lab::route_line_pixels` = 2.5 screen pixels, divided by the camera's zoom in each application | The user asked for thinner lines in Demo and thicker ones in i4sim and Dotio; one value between Demo's 3 and i4sim's 2 pixels makes the three alike. Dotio's width no longer depends on the zoom, so its paths stay visible zoomed out. |
| Name of the i4sim test fixture `MAS-demonstrator` | `test-environment` (`labs/i4sim/tests/test-environment.json`, and its `name`), with the test names and READMEs that said MAS | Asked by the user on the same branch. Kebab case like the other fixtures (`pressure-*.json`). The READMEs still name `MAS-demonstrator` once, as the reference's blueprint it comes from; dated design documents keep the old name as history. The benchmark's CSV now names it `test-environment`, so the recorded baseline tables follow. |
| Probes and detection ranges | Under the moving bodies, before the routes (Demo, Dotio's probes and wall contacts, i4sim's DETECTION); no start over the own body | Asked by the user on the same branch. A probe starts at its body's center and a detection circle lies around it, so both stay visible outside the body; drawing them before the routes keeps the routes over them, as before this branch. |
| Vectors and arrows | Under the moving bodies, before the probes and routes; the part within the own body drawn again right after it (`kinetra_lab::draw_vector_start`, `draw_line_start` in `under_bodies.h`) | Asked by the user on the same branch. Unlike a probe, a vector or i4sim line can be shorter than its body; drawing its own start over the body keeps it visible, as the route start does. `draw_vector_start` repeats the arrow length of Kinetra's `draw_debug_vector` (at least 1.5 units and 24 pixels); a change there needs the same change here. |
| Which bodies go above the routes | Every body that moves (Demo's player, agents and props; Dotio's player, enemies and props; i4sim's movers) | The rule is about what the layer hides; a prop is as much a body in the crowd as an agent. Fixed terrain stays below, because routes go around it. |

## Design

The order of a frame's world drawing becomes, in each application:

1. Terrain: Demo's fixed bodies (and the Ghost preview), Dotio's walls, i4sim's floor and producers.
2. Every route of the PATHS layer, in full.
3. The moving bodies, one by one. Right after each body that has a route shown, the part of its route that `route_start` returns: the body's position, each waypoint while the route stays inside the body's circle, and the point where it first leaves it. A route that comes back into the circle is not followed further.
4. The other debug layers and overlays, as before.

`route_start(start, route, radius)` returns the points of that polyline; the caller draws a line between each pair with its own width and color, in its own coordinates (Dotio's legacy pixels through `to_world`, i4sim's factory millimeters through `flip_y`).

Demo builds the route of each body by its id once per frame (the player's walk, every active agent's route) and draws its bodies in two passes: fixed, then moving. Fixed bodies used to be drawn in id order between the moving ones; a moving body now always lies above terrain, which it cannot overlap anyway.

One side effect in Demo, visible in the refreshed captures: a moving body's velocity vector is no longer hidden by terrain with a higher body id, because all terrain is drawn first.

## Affected areas

- `labs/shared/include/kinetra_lab/route.h` (new), `labs/shared/tests/shared_tests.cpp`.
- Demo: `labs/demo/src/presentation/renderer.cpp`, `labs/demo/tests/presentation_tests.cpp`.
- Dotio: `World::draw` (`labs/dotio/include/dotio/world.h`, `labs/dotio/src/world.cpp`), `labs/dotio/src/presentation/presentation.cpp`, `labs/dotio/tests/presentation_tests.cpp`.
- i4sim: `labs/i4sim/src/presentation/world_view.cpp`, `labs/i4sim/tests/presentation_tests.cpp`.
- Docs: [architecture](../architecture.md), [labs README](../../labs/README.md), the module READMEs of Demo, Dotio and i4sim, the functionality index in `AGENTS.md`.
- Media: the captures of the Demo, Dotio and i4sim rows of the [media guide](../media/README.md#which-captures-a-change-makes-stale) that show the PATHS layer.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| `route_start` ends where the route first leaves the circle, keeps a route that stays inside, stops at the first exit of a route that comes back, ends at a waypoint on the edge without repeating it, and is only its first point without a route or a radius | `kinetra_lab:a route's start …` (five cases) | Targeted |
| Dotio's `World::draw` runs its hook right after each active enemy, before the next | `kinetra_dotio:the world's draw hook runs right after each active enemy, before the next` | Targeted |
| Dotio: an enemy's vector starts over the enemy itself (an enemy's path start has the enemy's own color, so its pixels cannot show it) | `kinetra_dotio_graphics:an enemy's velocity vector starts over the enemy itself` | Full (graphics) |
| Dotio: a path is about 2.5 pixels wide at zoom 5 and 50 | `kinetra_dotio_graphics:an enemy's path is about 2.5 screen pixels wide at every zoom` | Full (graphics) |
| i4sim: a route starts over its own mover | `kinetra_i4sim_graphics:a mover's route starts over the mover itself` | Full (graphics) |
| Demo: the player's route does not cover an agent it crosses, and its start shows over the player | `kinetra_demo_graphics:a route passes under the agents it crosses and starts over its own body` (doorway scenario, the player walks past each agent) | Full (graphics) |
| Dotio: an enemy's path does not cover the player | `kinetra_dotio_graphics:an enemy's path passes under the player it crosses` (path wander, the first frame a path crosses the player) | Full (graphics) |
| i4sim: a route does not cover a mover listed before its owner | `kinetra_i4sim_graphics:a mover's route passes under the movers it crosses` (`4320x2400` at seed 42, the first crossing within 10 000 ticks) | Full (graphics) |
| Demo: no steering probe covers an agent it crosses | `kinetra_demo_graphics:steering probes pass under the agents they cross` (Terrain, 48 agents, seed 123, after two seconds) | Full (graphics) |
| Dotio: no enemy probe covers the player | `kinetra_dotio_graphics:an enemy's probes pass under the player they cross` | Full (graphics) |
| i4sim: no detection range covers a mover listed before its owner | `kinetra_i4sim_graphics:a mover's detection range passes under the movers it crosses` | Full (graphics) |
| Demo: no velocity vector covers an agent it crosses, outside that agent's own vector | `kinetra_demo_graphics:velocity vectors pass under the agents they cross` | Full (graphics) |
| Dotio: the player's vector does not cover an enemy it crosses | `kinetra_dotio_graphics:the player's velocity vector passes under the enemies it crosses` (the player walks to the nearest enemy) | Full (graphics) |
| i4sim: no seek arrow covers a mover listed before its owner | `kinetra_i4sim_graphics:a mover's seek arrow passes under the movers it crosses` (a mover 20 pixels wide, so the arrow reaches past its own mover) | Full (graphics) |
| Dotio's `World::draw` without a callback draws as before | The existing Dotio suites | Targeted |

Each graphics test failed on the old drawing order and passes on the new one.

## Review

Round 1, the agent review of 39ebb48 (*Fix first*):

| Finding | Resolution | Commit |
| --- | --- | --- |
| No test for the start over the own body in Dotio and i4sim, nor for the route width | Headless test of `World::draw`'s hook order; graphics cases for an enemy's vector start over the enemy (a path start has the enemy's own color, so no pixel can show it), a mover's route start over the mover, and the path width at zoom 5 and 50. Each fails without the change it covers | 926d8a6 |
| Stale graphics-case counts in the test tables | Counts and descriptions updated in this round's docs commit, the i4sim diagram too | this round |
| Conflict with `main` | `main` merged (#142, #153); the Kinetra pointer moved with it, so the captures were rendered again | merge commit |
| Nit: a waypoint on the edge repeated as the exit | No second point when the route leaves at once; the test expects two points | 4d282e7 |
| Nit: a line's half width reached past the body | Every start is clipped at the radius less half its line's width | 4d282e7, 6381728 |
| Nit: the Demo test used the player's default radius | It uses the box's smaller half extent | 6381728 |
| Nit: each route copied into a vector every frame | Dotio and i4sim draw from the position through the span | 6381728 |
| Nit: `draw_bodies(bool)` | `enum class Bodies { fixed, moving }` | 6381728 |
| Nit: one test case for three behaviors | Split into one case per behavior | 4d282e7 |
| Nit: the follow-up belongs on the roadmap | Not needed: the vectors and force arrows it named are done on this branch | |
| Lesson proposed | Added to `docs/review-lessons.md` | this round |

## Verification

- `python3 external/Kinetra/tools/check_copies.py` and `python3 external/Kinetra/tools/check_docs.py`.
- After the fixture rename: `cmake --build --preset release` of the i4sim targets in the [table](../../AGENTS.md#verification-in-lab) and `ctest --preset release -R '^kinetra_i4sim'`: 165 of 165 passed; the i4sim graphics suite 8 of 8.
- `cmake --build --preset dev --target kinetra_lab_tests kinetra_dotio_tests kinetra_demo kinetra_dotio kinetra_i4sim`, `ctest --preset dev -R '^kinetra_dotio:|^kinetra_lab:'`: 66 of 66 passed, no warnings.
- `cmake --build --preset graphics --target kinetra_demo_presentation_tests kinetra_dotio_presentation_tests kinetra_i4sim_presentation_tests kinetra kinetra_demo kinetra_dotio kinetra_i4sim kinetra_capture`, `ctest --preset graphics -L graphics` with `SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1`: 23 of 23 passed.
- Each new graphics test run against the old renderer of its application: it fails.
- `tools/capture/capture_media.sh build-graphics` for the captures of the Demo, Dotio and i4sim rows. Committed are those that changed by more than a colour step in some pixel, each looked at before and after: `demo-debug-clip`, `demo-doorway-clip`, `demo-gravity-clip`, `demo-maze-clip`, `dotio-debug`, `dotio-debug-clip`, `i4sim-debug`, `i4sim-debug-clip`. `demo-clip`, `i4sim-clip` and `i4sim.png` differed only in encoding and were not committed; the others came out identical. After the route width changed, the same scenes were rendered again: `demo-debug-clip`, `demo-doorway-clip`, `demo-maze-clip`, `dotio-debug`, `dotio-debug-clip`, `i4sim-debug` and `i4sim-debug-clip` changed and were looked at; the rest again differed only in encoding or not at all.
- After merging `main`: the same targeted builds and tests (dev 65 of 65, release i4sim 165 of 165), the graphics suites 25 of 25, and the debug captures rendered again on the merged code (`demo-debug-clip`, `dotio-debug`, `i4sim-debug`, `i4sim-debug-clip` changed and were looked at).
- After moving the probes and detection ranges: the graphics suites 28 of 28; each of the three new cases fails on the previous drawing order and passes on the new one. The captures rendered again: `demo-debug`, `demo-debug-clip`, `demo-doorway-clip`, `demo-gravity-clip`, `demo-maze-clip`, `dotio-debug`, `dotio-debug-clip`, `i4sim-debug` and `i4sim-debug-clip` changed and were looked at.
- After moving the vectors and arrows: the graphics suites 31 of 31; each of the three new cases fails on the previous drawing order and passes on the new one. The captures rendered again: `demo-debug`, `demo-debug-clip`, `demo-doorway-clip`, `demo-gravity-clip`, `demo-maze-clip`, `dotio-debug`, `dotio-debug-clip`, `i4sim-debug` and `i4sim-debug-clip` changed and were looked at.
- After review round 1 and merging `main` (#142, #153): the graphics suites 34 of 34; dev `kinetra_lab`, `kinetra_dotio` 68 of 68; release `^kinetra_i4sim` 165 of 165; no new warnings.
- The pull request's CI is the Full level.

## Pull request

Title: `fix(labs): draw debug routes under the bodies they cross`

The description summarizes this document: the summary, the changes per application, the route width, the fixture rename, the verification, the side effects and the open points. No dependencies and no Kinetra change; `main` was merged into the branch before the pull request (#140 to #155), and the verification below was repeated on the merge.

## Open points

- Dotio's enemy vectors are tested only through the player's vector: in the test run the few active enemies never came close enough to each other or to the player.
- Kinetra's shared-UI GIF `ui-debug-layers` shows Demo's layers and follows this change with the next Kinetra change that makes it stale, as Kinetra's media guide says.
