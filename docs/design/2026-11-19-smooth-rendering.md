# Smooth rendering and frame times

Status: accepted · Branch: `feat/smooth-rendering` · Related: [Kinetra design](../../external/Kinetra/docs/design/2026-11-19-frame-times.md), [system architecture](../architecture.md#session-ownership-and-frame-flow), [launcher and overlays](../launcher-overlays.md#settings-policy)

The user sees short freezes and uneven motion in WSL and on a Linux desktop that do not occur in the native Windows build. This change draws all three applications between their last two fixed ticks, and adds a FRAME TIMES row to every settings panel that shows Kinetra's new frame-time graph. With the graph, a stall in the application can be told apart from uneven presentation by the compositor.

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

- Every application draws its simulation state as it is after the last tick. The fixed clocks run 200 (i4sim), 120 (Demo) and 60 (Dotio) ticks per second, so at 60 frames per second i4sim alternates three and four ticks per frame. When frame times vary (no VSync in WSLg, a compositor that holds frames back), Dotio and Demo alternate between zero and two ticks. Motion then jumps.
- In WSLg the runtime logs `VSync unavailable: That operation is not supported`. Frames reach the Windows desktop through the WSLg compositor (RDP), which paces them on its own ([Windows](../windows.md#limits)).
- There is no way to see frame timing. Dotio's FPS and Demo's FPS are averages, which hide a single long frame.

## Goals and non-goals

- Goal: i4sim, Demo and Dotio draw moving objects between their last two ticks.
- Goal: a FRAME TIMES toggle in every settings panel, off by default.
- Goal: no simulation change. Seeded runs, golden lines and characterization fingerprints stay identical.
- Non-goal: interpolating Dotio's physics props, particle-like effects or Demo's Ghost preview terrain. Props are pushed around rarely; the others do not move per tick.
- Non-goal: changing VSync or the swap interval. The graph is the first step to deciding that on the user's Linux machine.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Scope | All three applications in one round | User's choice |
| Graph switch | FRAME TIMES row after DEBUG MODE in `lab::settings_rows`, off by default | User's choice. After DEBUG MODE, so ZOOM, STATISTICS and DEBUG MODE keep their row positions |
| What a snapshot holds | i4sim: each mover's position at the start of a tick. Demo: position and angle of every world body before and after the last tick. Dotio: absolute positions (world offset added) of player, enemy, arrow and bolt slots before and after the last tick | Each application's state model. Dotio's streaming shifts every position, so absolute positions make the shift cancel out |
| Objects that jumped | Drawn exactly when the current state differs from the state after the last tick | Covers teleports, Demo's chunk streaming renumbering bodies, and a Dotio slot taken by a new enemy, without tracking each cause |
| Paused scenes | Drawn exactly (interpolation 1) | A paused or single-stepped scene must show the state the statistics describe |
| Camera | Demo and Dotio move the camera by the player's interpolation offset while drawing (`lab::ScopedCameraShift`), then restore it | A camera on the simulated player would make the drawn player jitter on screen. Restoring keeps the session's camera, and Demo's characterization fingerprint (which hashes it), unchanged |
| Dotio drawing | Swap the blended positions into the entities for one frame (RAII) and restore them | The ported logic draws itself from `Position`; passing poses down would mean rewriting its drawing |
| i4sim settings click in the presentation test | Compute LAUNCHER's position from a panel with the same Lab rows | The test had the panel height and offset as numbers, which every new row breaks |

## Design

```mermaid
sequenceDiagram
    participant Host as run_session / run_launcher
    participant S as Lab session
    participant A as Application logic
    participant R as Runtime
    Host->>S: advance(elapsed)
    S->>A: fixed ticks; snapshot before and after the last one
    Host->>S: render(runtime)
    S->>R: camera += drawn player - simulated player (Demo, Dotio)
    S->>R: draw objects at lerp(before, after, interpolation())
    S->>R: draw_frame_times(runtime.frame_times()) if FRAME TIMES
    S->>R: end(): swap, record frame time
    S->>R: restore camera (and Dotio positions)
```

*Takeaway:* snapshots are taken in `advance` and only read in `render`; nothing in the simulation reads them.

## Affected areas

- [labs/common/include/lab/settings.h](../../labs/common/include/lab/settings.h): FRAME TIMES row.
- i4sim: [simulation.h](../../labs/i4sim/include/simulation.h) (`Mover::previous_position`, `Simulation::interpolation()`), [simulation.cpp](../../labs/i4sim/src/simulation.cpp), [session.cpp](../../labs/i4sim/src/session.cpp).
- Demo: [sandbox_application.h](../../labs/demo/include/sandbox_application.h) (`presented_pose`), [sandbox_application.cpp](../../labs/demo/src/sandbox_application.cpp), [sandbox_renderer.h](../../labs/demo/include/sandbox_renderer.h), [sandbox_renderer.cpp](../../labs/demo/src/sandbox_renderer.cpp), [session.cpp](../../labs/demo/src/session.cpp).
- Dotio: [application.h](../../labs/dotio/include/application.h) (snapshots, `interpolation()`, `frame_times`), [presentation.cpp](../../labs/dotio/src/presentation.cpp), [session.cpp](../../labs/dotio/src/session.cpp).
- Tests: [i4sim simulation](../../labs/i4sim/tests/simulation_tests.cpp) and [presentation](../../labs/i4sim/tests/presentation_tests.cpp), [Demo application](../../labs/demo/tests/sandbox_application_tests.cpp), [Dotio presentation](../../labs/dotio/tests/presentation_tests.cpp), [launcher runtime](../../launcher/tests/runtime_tests.cpp).
- Docs: [system architecture](../architecture.md), [launcher and overlays](../launcher-overlays.md).
- `external/Kinetra` pointer.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| i4sim: `previous_position` is the start of the last tick; interpolation is the accumulated fraction, and 1 while paused | `kinetra_i4sim:drawing interpolates between a mover's last two ticks` | Targeted |
| i4sim: no simulation change | The ten `kinetra_i4sim_golden` lines | Targeted |
| Demo: a moving player is drawn within the last tick; a teleport and a pause show the exact state | `kinetra_sandbox_application:drawn poses lie between the last two ticks and snap after a teleport or pause` | Targeted |
| Demo: no simulation or camera change | `kinetra_demo_crowd_characterization` fingerprints | Targeted |
| Dotio: snapshots cover every moving slot and match the simulated player; drawing restores positions and camera | `kinetra_dotio_graphics:drawing interpolates the player and leaves the simulation untouched` | Graphics |
| The FRAME TIMES row sits after DEBUG MODE; clicks by row still reach SIM CONTROLS and the actions | Launcher `kinetra_graphics` panel cases (row indices), i4sim presentation test (LAUNCHER) | Graphics |
| The graph draws | Kinetra `kinetra_engine_graphics:presented frames are timed and the frame-time graph draws them` | Kinetra graphics |

Presentation, sessions and the engine pointer change: Full level with graphics.

## Review

An independent review of both branches (a separate agent, given the code but not this document's reasoning) confirmed the `FrameTimes` ring, `lerp_angle`, the Dotio world-shift handling, Demo's streaming order and that no simulation code reads the snapshots. Its findings and their resolution:

| Finding | Resolution |
| --- | --- |
| Dotio: a death respawns the player inside a tick (`World::Reset`), so the drawn player and the camera streaked from the place of death to the spawn point for a frame | `World::ResetCount()`; a tick that reset the world is not blended |
| `docs/architecture.md`: a paragraph was glued to the new table's last row | Moved below the table |
| The engine graph test read only `GL_FRONT`, which is undefined after a swap on some drivers | Reads both buffers, like the other runtime cases (Kinetra) |
| After a pause, and in i4sim after a UPS change, the first running frames drew up to one tick behind the state shown while paused | The first running frame starts from the current state; `Simulation::updates_per_second` aligns previous positions |
| The graph at the top center overlapped Lab's debug menu (top left, at `{8, 32}`) on narrow windows | Moved to the top-right corner (Kinetra) |
| The Dotio test passed even if `PresentedPositions` did nothing | `PresentedPositions` moved to `presentation.h`; the test checks the player sits between its last two ticks while drawing |
| The camera was restored by a plain statement after `end()`, not on an exception | `lab::ScopedCameraShift` restores it on every path |
| The module READMEs did not mention drawing between ticks | Demo, Dotio and i4sim READMEs link the architecture section |

Not changed: the FRAME TIMES row order has no direct test. The launcher panel cases address SIM CONTROLS and DEBUG CONTROLS by row index, and the i4sim presentation test finds LAUNCHER through a mirrored panel, so a misplaced row fails them. The in-tick respawn is not provoked by a test: it needs a scripted death; the reset counter is a one-line check.

## Verification

To be filled after verification.

## Pull request

Title: `feat(labs): draw between fixed ticks and show frame times`

**Summary.** i4sim, Demo and Dotio now draw their moving objects between the last two fixed ticks, so motion stays even when frames run different numbers of ticks. A FRAME TIMES row in every settings panel shows Kinetra's new frame-time graph, which tells a stalled frame apart from uneven presentation by the compositor (WSLg has no VSync).

**Changes.**
- `lab::settings_rows`: FRAME TIMES row after DEBUG MODE.
- i4sim: `Mover::previous_position`, `Simulation::interpolation()`; the session draws movers, labels and debug vectors blended.
- Demo: pose snapshots and `presented_pose`; the renderer draws moving bodies and markers blended and moves the camera with the drawn player for the frame.
- Dotio: absolute position snapshots; drawing swaps in blended positions and restores them.
- Tests for each application; the i4sim presentation test finds LAUNCHER through a mirrored panel.
- Docs: system architecture, launcher and overlays.
- `external/Kinetra` pointer.

**Design.** [docs/design/2026-11-19-smooth-rendering.md](2026-11-19-smooth-rendering.md), Kinetra: [frame times](../../external/Kinetra/docs/design/2026-11-19-frame-times.md).

**Verification.** See the design document.

**Review notes.** Snapshots are presentation state only; golden and characterization lines are unchanged. Dotio's position swap is the least elegant part, chosen over rewriting the ported drawing code.

**Dependencies and merge order.** The Kinetra frame-times pull request first, with a merge commit. The window-icon pull requests also move the pointer; the Lab pull request merged second needs its pointer moved to the Kinetra merge commit that holds both.

**Follow-up.** Measure on the user's Linux desktop with FRAME TIMES on; if frames are long, profile; if they are even, look at the compositor and swap interval.

## Open points

- Dotio's physics props are not interpolated.
