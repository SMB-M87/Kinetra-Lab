# Demo scenario menu

Status: implemented · Branch: `feat/demo-scenarios` · Related: [roadmap](../roadmap.md), [Demo](../../labs/demo/README.md), [launcher](../../launcher/README.md)

Demo will offer a launcher page that starts environments illustrating individual Kinetra features.

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

The launcher registers Demo with an empty configuration list. Demo starts a player and crowd in streamed terrain and uses the same setup for direct launch. Dotio already uses the launcher's configuration page.

## Goals and non-goals

Give Demo a configuration page. Every choice starts a distinct environment with its own purpose. The existing terrain stays available. The shared UI captures can use the new configuration page when its separate roadmap item starts.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| First scenarios | Streamed terrain, gravity and contacts, maze pathfinding, and a doorway encounter | User choice on 2026-11-23 |
| Direct launch | Default to streamed terrain, with `--scenario terrain|gravity|maze|doorway` on both launchers. `--agents` sets the terrain population | User chose a default terrain and a named flag. Authored scenarios use fixed populations |
| Doorway and Motion lab | One doorway simulation with the existing RECIPROCAL and LEGACY selector. Simultaneous comparisons and measurements belong to Motion lab | The existing selector makes both policies available in the same fixture |

## Design

Reuse `LaunchChoice::configurations`, the Demo session and the existing input, camera, renderer and overlays. Give the launcher registration a scenario catalog with stable identifiers and display names. The launcher can pass the selected entry to the same factory that starts the current terrain.

Terrain streams around the player. The three authored scenarios use `Scene::arrange` to initialize bodies, obstacle indexes and navigation bounds. They keep the layout on every frame, including when the camera enters Ghost mode. The fixed camera starts centered, and teleport checks use the authored bounds and obstacles. Gravity starts with falling props. Maze uses alternating gaps, and Doorway starts two groups with opposing goals.

The launcher tour clicks Demo and then selects Terrain. The script checks the active session before proceeding to the Demo chapter. The existing terrain captures still provide a check for the default scenario. The CLI inspector replays the selected scenario, with Terrain as the default.

## Affected areas

The Demo scene and its configuration, launcher registration and CLI, the relevant module guides, and menu, simulation and presentation verification.

## Test plan

Verify that every menu choice opens its scenario and the default launch still starts the streamed terrain. Exercise each fixed scenario's engine behavior headlessly. Verify that resize and Ghost movement preserve the Maze walls and route, and that existing Terrain fingerprints match. Require the capture smoke test to reach the tour's Terrain check. Feed the Doorway inspector's displayed selector through the CLI and compare its records to the headless snapshots. Capture the launcher page and new scenes for review.

## Review

Checked that Terrain's camera interpolation and streaming keep their original paths, and that the launcher passes the selected index through to the Demo factory. Fixed layouts share the existing renderer and clock. The tour and capture tests use the same registration as the launcher.

### First review round

| Finding | Resolution |
| --- | --- |
| The Doorway selector did not work in the CLI | `50c3764` passes the selected scenario to the trace and tests the inspector's actual selector end to end |
| The tour test could succeed before Terrain started | `932d0b2` makes the script check the active Demo scenario and makes the smoke test require that checkpoint |
| The Maze test did not resize the active scene | `355e0c4` resizes while the player is following a route, checks every wall and replans to the original goal |
| The GIF roadmap still called the configuration page missing | `1d59f51` removes the stale statement and keeps the link to the new page's design |

### Second review round

*Ready to merge* on `5a5f6a9`, with three nits:

| Nit | Resolution |
| --- | --- |
| The Demo README did not state the trace's conditions | The README says that the trace uses the default motion and wander settings, RECIPROCAL avoidance and no input |
| The README's count of logic cases was 27 | Corrected to 30 |
| The tour throws `std::bad_cast` instead of its own message when the launcher starts another application | Left for a later change to the capture tool, because it is code; the smoke test still fails in that case |

## Verification

- `ctest --preset graphics -R '^kinetra_demo|^kinetra_menu|^kinetra_graphics|^kinetra_capture|^kinetra_smoke:(demo|launcher.*)$' -LE soak -j 6 --output-on-failure` passed all 124 selected checks. This includes the terrain fingerprints, fixed scenario tests, launcher, capture and graphics suites, and the smoke runs.
- `ctest --preset dev -R 'kinetra_demo_inspector_trace|kinetra_demo:(Maze|seeded Doorway)' --output-on-failure` passed five targeted checks after the follow-up build.
- Started all four choices through `kinetra --scenario` and `kinetra_demo --scenario` with software OpenGL. The launcher rejected an invalid scenario and a scenario requested with i4sim.
- Captured and inspected [the Demo menu](../media/launcher-demo-scenarios.png), Gravity, Maze and Doorway (stills that [clips](2026-11-24-lab-readme-gifs.md) replaced on 2026-11-24). The two existing Terrain screenshots matched the earlier capture byte for byte.
- Docs links, copies and changed-file formatting passed. Pull request CI supplies the Full level.

## Pull request

`feat(demo): add a four-scenario launcher menu`

### Summary

Demo offers Terrain, Gravity, Maze and Doorway through the launcher's configuration page. Both executables accept `--scenario` and default to Terrain.

### Changes

The three authored layouts share the existing physics, navigation and overlays. The capture tool records all four new views and verifies that the tour selects Terrain. The CLI inspector accepts Doorway's selectors.

### Design

[Demo scenario menu](2026-11-23-demo-scenario-menu.md)

### Verification

124 graphics-preset checks and five targeted Debug checks passed. All four CLI scenarios launched in both executables, and the screenshots were inspected.

### Review notes

Doorway uses the existing avoidance selector to compare the two modes on the same layout. Terrain still uses the user's initial population option.

### Dependencies and merge order

Merge after Lab #96.

### Follow-up

Kinetra's shared UI capture can now open Demo's configuration page.

## Open points

Simultaneous crowd comparisons and measurements are part of [Motion lab](../roadmap.md#ideas).
