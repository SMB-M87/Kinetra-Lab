# Default debug layers

Status: accepted · Branch: `ccr-7f91cba1-leqelc` · Related: [Demo](../../labs/demo/README.md#controls), [i4sim](../../labs/i4sim/README.md), [media guide](../media/README.md#which-captures-a-change-makes-stale)

Demo and i4sim open every scenario and blueprint in Debug Mode with (almost) every debug layer on, so a first look is cluttered. This change narrows what a session starts with: VECTORS and PATHS in Demo; every layer with all its parts except HEATMAP and CONTACT SHAPES in i4sim. Every layer can still be turned on in the debug menu.

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

- Demo: `DebugLayers` (`labs/demo/include/demo/debug_panel.h`) starts VECTORS, STEERING PROBES, PATHS, COLLISION SHAPES and ACTIVE AGENTS on. The user wants a scenario to open with only VECTORS and PATHS.
- i4sim: `ViewLayers()` (`labs/i4sim/include/i4sim/simulation/view.h`) selects every layer. The user wants GRID, PATHS, FORCES, BORDERS and DETECTION on, with all their parts, and the rest (HEATMAP, CONTACT SHAPES) off.

## Goals and non-goals

Goals:

- A new Demo session, in every scenario, starts with VECTORS and PATHS on and the other four layers off.
- A new i4sim session starts with GRID, PATHS, FORCES, BORDERS, DETECTION and every part of PATHS, FORCES and BORDERS selected, and HEATMAP and CONTACT SHAPES off.
- The docs, the capture scripts' comments and the captures show the new defaults.

Non-goals:

- Dotio, which keeps debug opt-in.
- Remembering a user's layer choice between sessions.
- RESET PARAMETERS, which resets tuning and leaves layers alone, as before.
- Kinetra's shared-UI GIFs: they show Demo, but [Kinetra's media guide](../../external/Kinetra/docs/media/README.md#which-gifs-a-change-makes-stale) lets them follow a Demo-only change with the next Kinetra change that makes them stale.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Demo's defaults | VECTORS and PATHS on; STEERING PROBES, COLLISION SHAPES, ACTIVE AGENTS and HOME RANGE off | The user's request |
| i4sim's defaults | GRID, PATHS, FORCES, BORDERS, DETECTION and all their parts on; HEATMAP and CONTACT SHAPES off | The user's request; that the parts stay on is the user's choice of 2026-11-27 |
| Captures | Refreshed with the new defaults in this pull request | The user's choice of 2026-11-27, over scripts that turn the other layers on to keep the old pictures |
| The Demo layers UI clip and the tour | `ui-debug-layers` keeps its clicks, which now toggle each layer away from its default and back; the tour turns STEERING PROBES and COLLISION SHAPES on for a look and then off with VECTORS, so its later chapters see the same layers as before | The clip still shows every layer; the tour keeps its story |

## Design

- Demo: the member initializers of `DebugLayers` become `vectors{true}`, `paths{true}` and the rest `{}`. `DebugLayers::none()` and the panel's rows are unchanged.
- i4sim: `ViewLayers()` selects every layer and then resets `Layer::heatmap` and `Layer::contact_shapes`. Because parts are layers of their own, keeping them selected keeps PATHS, FORCES and BORDERS fully drawn. `ViewLayers::none()` still clears every bit.

## Affected areas

- `labs/demo/include/demo/debug_panel.h`, `labs/i4sim/include/i4sim/simulation/view.h`.
- Tests: `labs/demo/tests/debug_panel_tests.cpp`, `labs/i4sim/tests/controller_tests.cpp`, `launcher/tests/runtime_tests.cpp` (graphics).
- Capture tool comments and the tour: `tools/capture/src/scenes/ui_clips.cpp`, `tools/capture/include/capture/scenes/ui_clips.h`, `tools/capture/src/scenes/scenes.cpp`, `tools/capture/src/tour/tour_script.cpp`, `tools/capture/README.md`.
- Docs: `README.md`, `labs/demo/README.md`, `labs/i4sim/README.md`, `docs/launcher-overlays.md`.
- Media: the Demo and i4sim rows of the [media guide](../media/README.md#which-captures-a-change-makes-stale).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Demo's layer rows start with VECTORS and PATHS checked, the other four not, and `layers(true)` returns the same | `kinetra_demo:the layer rows carry their colors and start as the session shows them` | Targeted |
| i4sim selects and shows every layer and part but HEATMAP and CONTACT SHAPES at the start | `kinetra_i4sim_controller:a session starts with every layer and part but the heatmap and contact shapes` (new) | Targeted |
| The keys and the menu rows toggle from the new defaults | `kinetra_i4sim_controller:the keys one to six toggle their layers`, `...:the layer entries are check boxes on the layers` | Targeted |
| The real Demo and i4sim sessions draw the new defaults in their debug menus | `kinetra_runtime_tests` (launcher integration, graphics) | Full (CI's graphics job) |
| The i4sim graphics case still finds its swatches, parts and overlays | `kinetra_i4sim_presentation_tests` | Full |
| The capture scripts still find their rows | `kinetra_capture_tests`, `kinetra_capture_smoke` | Targeted / graphics |

## Review

To be filled after the review.

## Verification

To be filled after verification.

## Pull request

To be filled at hand-over.

## Open points

- Kinetra's `ui-debug-menu`, `ui-inspector` and `ui-debug-layers` GIFs still show Demo's old defaults, and the `ui-debug-layers` row of its media guide describes the old clicks. They follow with the next Kinetra change that makes them stale, as its media guide says.
