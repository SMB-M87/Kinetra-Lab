# Default debug layers

Status: implemented · Branch: `ccr-7f91cba1-leqelc` · Related: [Demo](../../labs/demo/README.md#controls), [i4sim](../../labs/i4sim/README.md), [media guide](../media/README.md#which-captures-a-change-makes-stale)

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
| The Demo layers UI clip and the tour | `ui-debug-layers` keeps its clicks, which now toggle each layer away from its default and back; the tour's Demo chapter turns STEERING PROBES, COLLISION SHAPES and ACTIVE AGENTS on for a look and then the first two off with VECTORS, so its later chapters see PATHS and ACTIVE AGENTS as before; its i4sim chapter no longer clicks HEATMAP, which now starts off | The clip still shows every layer; the tour keeps its story (changed in review round 1) |

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

Self-review of `git diff main...HEAD`:

| Finding | Resolution |
| --- | --- |
| The rewritten comment above `enum class Layer` ran past 100 columns | Rewrapped; `clang-format --dry-run --Werror` passes |
| The launcher's graphics case was still named *demo and i4sim start with every visual debug layer on* | Renamed to *demo and i4sim start with their default debug layers on*; only a dated design document names the old one |
| The tour clicked STEERING PROBES, VECTORS and COLLISION SHAPES to turn them off; with the new defaults that turned two of them on | The tour turns STEERING PROBES and COLLISION SHAPES on for a look and then off with VECTORS, so its later chapters see the same layers as before |
| A refresh changed `demo-clip.gif`, `i4sim.png` and `i4sim-clip.gif` too, by a few pixels (10 in `i4sim.png`) | Debug Mode is off in those, so the change does not alter them; reverted, as the media guide asks |
| Kinetra's `ui-debug-menu`, `ui-inspector` and `ui-debug-layers` GIFs show Demo's old defaults | Left alone: Kinetra's media guide lets a Demo-only change wait for the next Kinetra change that makes them stale ([Open points](#open-points)) |

`check_copies.py` with the terms *every debug layer*, *all visual layers*, *every layer on*, *start on* and *starts selected*: the remaining hits are Dotio's, which keeps every layer, the new i4sim sentence, and i4sim's note on the menu's height with every layer on, which still holds.

Agent review, round 1 (on `31ae128`, conclusion *Fix first*):

| Finding | Resolution | Commit |
| --- | --- | --- |
| Blocking: the branch conflicts with `main` (#143's settings-panel move, #142's make-way showcase clip, the Kinetra pointer at `9841a50`); neither side of the four binary conflicts is right | Merged `main`; README and i4sim README sentences combine both sides; the Demo and i4sim debug captures and all six showcase clips rendered again on the merged head. Only the four debug captures and the make-way clip changed; the other clips came out identical | `47d0b49`, `c4d92e7` |
| The tour's clicks assumed the old defaults: Demo's later chapters lost ACTIVE AGENTS, and i4sim's LAYERS chapter turned HEATMAP on | Demo turns ACTIVE AGENTS on with the other two; i4sim no longer clicks HEATMAP. The tour was played to its end ([Verification](#verification)) | `4080f6e` |
| The Demo debug clip's alt text in `README.md` named steering probes and collision shapes | Now "vectors and paths" | `4080f6e` |
| A comment in `scenes.cpp` still said the showcase scenes run with every visual layer | Now "the default layers" | `4080f6e` |
| Nits: the controller suite's case count (44 → 45) in `labs/i4sim/tests/README.md`; the message of the key-two check | Both carried out | `4080f6e` |
| Lessons proposed: re-read toggle clicks after a default flips; merge `main` before review when it changed the same captures | Added to `docs/review-lessons.md` | `6e1d502` |
| For the user: the review session reported running below the `high` effort its Levels table asks for | Passed on to the user; it was started with effort `high` | |
| (Found while merging `main` again) #153 added a Demo launcher preview recorded in Debug Mode as the session starts, so it showed the old probes | Merged `main`; `demo-preview`, the launcher's still `launcher/previews/demo.rgba` and `launcher.png` refreshed; the media guide and a capture comment now say routes and velocity vectors | `8f64ae9`, `14411bf` |

## Verification

Full level: locally the targeted builds and tests of every changed area, then the pull request's CI.

| Command | Result |
| --- | --- |
| `cmake --build --preset dev --target kinetra_demo_tests kinetra_i4sim_controller_tests`; `ctest --preset dev -R '^kinetra_demo:\|^kinetra_i4sim_controller'` | 81/81 passed, the new i4sim case included |
| `ctest --preset release -R '^kinetra_i4sim\|^kinetra_demo(:\|_terrain\|_crowd\|_passing)\|^kinetra_capture' -LE soak`, after building the targets of those areas and `kinetra_i4sim_motion_benchmark` | 277/277 passed |
| `ctest --preset graphics -L graphics -R '_graphics\|kinetra_graphics\|kinetra_smoke'` (offscreen SDL, software Mesa) | 25/25 passed |
| `ctest --preset graphics -R 'kinetra_runtime\|i4sim_presentation\|demo_presentation\|^kinetra_capture'` | 35/35 passed, `kinetra_capture_smoke` included |
| `tools/capture/capture_media.sh build-graphics` with the Demo and i4sim scenes | Every refreshed file looked at; the debug captures show the new defaults |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |
| `clang-format --dry-run --Werror` on the changed C++ files | Clean |
| Build warnings in the `graphics` build | None |

After merging `main` (round 1):

| Command | Result |
| --- | --- |
| `ctest --preset release -R '^kinetra_i4sim\|^kinetra_demo(:\|_terrain\|_crowd\|_passing)\|^kinetra_capture\|^kinetra_menu:\|^kinetra_lab:' -LE soak` | 293/293 passed |
| `ctest --preset graphics -L graphics`; `ctest --preset graphics -R '^kinetra_capture'` | 33/33 and 35/35 passed |
| `tools/capture/capture_media.sh build-graphics` with the Demo and i4sim debug scenes, the Demo scenario clips and all six showcase clips | Every changed file looked at: the new menu and the new defaults |
| `ctest --preset release -R '^kinetra_i4sim_controller\|^kinetra_capture:'` after the review fixes | 79/79 passed |
| `tools/capture/capture_media.sh build-graphics demo-preview launcher` after merging #153; `ctest --preset graphics -R '^kinetra_graphics\|^kinetra_menu:\|^kinetra_capture\|kinetra_smoke:launcher'` | Preview and launcher looked at: routes and vectors, no probes; 61/61 passed |
| `tools/capture/capture_tour.sh build-graphics <scratchpad>/tour.mp4` (the thumbnail next to it, not in `docs/media`) | The tour plays to its end, 19138 frames, 318 s. Frames looked at: Demo's LAYERS chapter shows the five layers on for a look, HOME RANGE then shows on PATHS and ACTIVE AGENTS, and i4sim's LAYERS chapter keeps HEATMAP off while it toggles DETECTION and FORCES |

## Pull request

Title: `feat(demo,i4sim): start sessions with fewer debug layers`

```markdown
## Summary
Demo opens every scenario with only the VECTORS and PATHS debug layers on. i4sim starts with GRID, PATHS, FORCES, BORDERS and DETECTION and all their parts on, and HEATMAP and CONTACT SHAPES off. Every layer can still be turned on in the debug menu.

## Changes
- `DebugLayers` (Demo) and `ViewLayers()` (i4sim) start with the new defaults.
- Tests: Demo's layer rows, a new i4sim case for the start state, the i4sim key and menu cases, and the launcher's graphics check of both menus.
- Capture tool: comments, and the tour's Demo and i4sim chapters keep their story with the new defaults.
- Docs: README, Demo and i4sim READMEs, launcher and overlays page, capture README.
- Media: the Demo and i4sim debug screenshots and clips, the six showcase clips and the Demo launcher preview with the launcher's still, refreshed on the head merged with `main`.

## Design
[docs/design/2026-11-27-default-debug-layers.md](docs/design/2026-11-27-default-debug-layers.md)

## Verification
Targeted suites of Demo, i4sim, the capture tool, the menu and the shared Lab code (293/293 on the merged head), the graphics suites and smoke tests, the capture smoke test, the docs check and clang-format pass locally, and the tour plays to its end with the new defaults. CI is the Full level.

## Review notes
Kinetra's shared-UI GIFs still show Demo's old defaults; its media guide lets them follow with the next Kinetra change that makes them stale.

## Dependencies and merge order
None: no Kinetra change, and the pointer is unchanged.

## Follow-up
None.
```

## Open points

- Kinetra's `ui-debug-menu`, `ui-inspector` and `ui-debug-layers` GIFs still show Demo's old defaults, and the `ui-debug-layers` row of its media guide describes the old clicks. They follow with the next Kinetra change that makes them stale, as its media guide says.
