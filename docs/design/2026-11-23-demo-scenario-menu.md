# Demo scenario menu

Status: accepted · Branch: `feat/demo-scenarios` · Related: [roadmap](../roadmap.md), [Demo](../../labs/demo/README.md), [launcher](../../launcher/README.md)

Demo will offer a launcher page that starts environments illustrating individual Kinetra features.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
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

The current Demo scene streams terrain around the player. Fixed scenarios can reuse authored bodies and navigation bounds through `Scene::arrange`. A fixed scene needs to keep those bodies across frame updates and camera movement, including Ghost mode. The navigation and teleport checks must use the authored obstacles.

The launcher tour and its menu tests currently select Demo in one action. They need a second selection when the configuration page opens. The existing terrain captures still provide a check for the default scenario.

## Affected areas

The Demo scene and its configuration, launcher registration and CLI, the relevant module guides, and menu, simulation and presentation verification.

## Test plan

Verify that every menu choice opens its scenario and the default launch still starts the streamed terrain. Exercise each fixed scenario's engine behavior headlessly. Verify that resize and Ghost movement preserve fixed obstacles, that existing terrain fingerprints match, and that the tour can choose a scenario. Capture the launcher page and new scenes for review.

## Open points

Desktop playtesting can compare the feel of the four scenarios once their controls and geometry are in place.
