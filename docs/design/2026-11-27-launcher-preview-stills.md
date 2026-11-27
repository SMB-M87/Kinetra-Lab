# Launcher preview stills of their own

Status: implemented · Branch: `ccr-69015929-m24lnf` · Related: [Lab previews in the launcher](../../external/Kinetra/docs/design/2026-11-24-launcher-images.md), [capture tool](../../tools/capture/README.md), [media guide](../media/README.md)

The launcher's Demo and i4sim previews were the full 1920 × 1080 screenshots scaled down six times. The user found the Demo still dull and too far zoomed out, and the i4sim still blurry and too busy. Both now come from a capture scene made for the still.

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

`tools/capture/capture_media.sh` scaled `demo.png`, `dotio.png` and `i4sim.png` (1920 × 1080) down to the 320 × 180 stills in `launcher/previews/`, which the launcher draws about 500 pixels wide (`external/Kinetra/src/runtime/launcher.cpp`, `image_slot`).

- **Demo.** Terrain at Demo's starting zoom of 17 pixels per unit shows about 113 units across. After the sixfold reduction an agent is one or two pixels: a dark field with specks and grey shapes.
- **i4sim.** `4320x2400` holds 29 movers, 18 producers, a label above each of them and two HUD lines. Reduced six times, the 1.0-scale label font and the HUD text become grey smudges over most of the floor.

## Goals and non-goals

- Goals: a Demo still with visible motion and agents large enough to read; a calm, sharp i4sim still; both reproducible with the capture tool, as before.
- Non-goals: the Dotio still, which the user did not mention; the still's size (320 × 180) and how the launcher embeds or draws it; the shared-UI GIF `ui-launcher` in Kinetra, which follows a Lab-only change with the next Kinetra change that makes it stale ([Kinetra's media guide](../../external/Kinetra/docs/media/README.md#which-gifs-a-change-makes-stale)).

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Source of the stills | A capture scene per still, `demo-preview` and `i4sim-preview`, at 960 × 540 | A smaller window shows a smaller area at the same zoom, so shapes are larger; three times the still's size keeps the area scale sharp. The scenes' PNGs sit in `docs/media`, so a reviewer sees what the still came from |
| Demo's moment | Doorway 2.3 s after its start, seven wheel notches in, Debug Mode as it starts, no statistics | Compared with Terrain at several zooms (sparse: zoomed in, only the player and an agent are in view) and Gravity: Doorway's two groups meeting in the opening, with their routes and probes, reads as motion at 320 × 180 |
| i4sim's floor | `1680x960+` whole, two notches in from the session's fit, Debug Mode and statistics off, at the first step of its first minute with the most movers driving | A crop of `4320x2400` cut producers and movers off at the edges and stayed crowded; the small original floor shows every kind of producer and mover with few labels |
| Changing the view | Through input only: the wheel in Debug Mode | The tool's [invariant](../../tools/capture/README.md#invariants-and-contracts): a script changes a session through `kinetra::Input` alone |

## Design

- `scenes.cpp` gains `preview()` scenes at `preview_width` × `preview_height` (960 × 540). `demo_preview` starts Doorway, turns the wheel `demo_preview_zoom_in` notches, turns STATISTICS off and waits `demo_preview_seconds`. `i4sim_preview` loads `i4sim_preview_blueprint` through `i4sim_load`, now split from `i4sim_start`, with the motion clicks in the debug view, turns the wheel `i4sim_preview_zoom_in` notches, turns DEBUG MODE and STATISTICS off, and runs to `i4sim_preview_start()`: the busiest single step of `driving_movers` over 60 s.
- `capture_media.sh` maps each lab to the scene its still comes from: `demo-preview`, `dotio`, `i4sim-preview`. The rebuild before the launcher scenes is unchanged.

## Affected areas

- `tools/capture/src/scenes/scenes.cpp`, `tools/capture/include/capture/scenes/scenes.h`, `tools/capture/capture_media.sh`, `tools/capture/tests/capture_tests.cpp`.
- `launcher/previews/demo.rgba`, `launcher/previews/i4sim.rgba`; `docs/media/demo-preview.png`, `docs/media/i4sim-preview.png`, `docs/media/launcher.png`.
- `docs/media/README.md`, `tools/capture/README.md`, `launcher/README.md`.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| The two scenes exist, each once, with a file and a row in the media guide | The scene-list and gallery cases of `kinetra_capture_tests` | Targeted |
| A preview is a Lab screenshot at a whole multiple of 320 × 180 | New case *a launcher preview is a screenshot at a whole multiple of the launcher's still* | Targeted |
| The preview floor has movers driving in the minute searched | New case *the i4sim preview floor loads and sets movers off within the minute the preview searches* | Targeted |
| The scenes render and the still embeds | `capture_media.sh build-graphics demo-preview i4sim-preview launcher`, then look at both PNGs, the stills as the launcher scales them, and `launcher.png`; `kinetra_menu_tests` checks the embedded size | Targeted |
| The tool runs offscreen | `kinetra_capture_smoke` | Full (CI) |

## Review

Self-review of the diff: the first version set the camera directly, against the tool's input-only invariant; the scripts now zoom with the wheel.

Agent review, round 1 (on `8d4c8c3`, conclusion *Fix first*):

| Finding | Resolution | Commit |
| --- | --- | --- |
| Should fix: the branch conflicts with `main`, and the merge moves the Kinetra pointer from `da98dfa` to `9841a50` (steering, settings panel), against which the scenes were never rendered | Merged `main`; rendered `demo-preview`, `i4sim-preview` and `launcher` again at `9841a50`: the PNGs and both stills came out byte-identical | merge of `main`, `23cbc28` |
| Nit: the *Screenshot* term names only 1920 × 1080 | It names the preview size too | `23cbc28` |
| Nit: "i4sim's scenes load `4320x2400.json`" and the section comment in `scenes.cpp` read as covering the preview | Both scoped to the screenshots and clips | `23cbc28` |
| Nit: the test hard-codes the 60 s search | `i4sim_preview_search_seconds` moved to `scenes.h`; the test uses it | `23cbc28` |
| Nit: no warning when no mover drives in the searched minute | `i4sim_preview_start` warns, as `dotio_debug_clip` does | `23cbc28` |
| Lesson proposed | Added to *Media and tools* | `99be18f` |

## Verification

- `cmake --build --preset graphics --target kinetra_capture_tests kinetra_menu_tests kinetra_capture` and `ctest --preset graphics -R '^kinetra_capture|^kinetra_menu:'`: 46 of 46 passed, `kinetra_capture_smoke` included.
- `tools/capture/capture_media.sh build-graphics demo-preview i4sim-preview launcher`: wrote both previews and `launcher.png`, and rebuilt the capture tool with the new stills first; each looked at. After each merge of `main` (Kinetra at `da98dfa`, then `9841a50`) the same command gave byte-identical files.
- `clang-format --dry-run --Werror` on the changed C++ files; `python3 external/Kinetra/tools/check_docs.py`; `python3 external/Kinetra/tools/check_copies.py` with the old wordings as terms: only intended hits.
- Full level: the pull request's CI.

## Pull request

**feat(capture): launcher previews of their own for Demo and i4sim**

Summary: the launcher's Demo still showed Terrain from too far away and the i4sim still was blurry and crowded, because both were the 1920 × 1080 screenshots scaled down six times. Two capture scenes now make the stills: Demo's Doorway as its groups cross, zoomed in with Debug Mode's routes, and i4sim's small `1680x960+` floor whole, without HUD lines.

Changes, design, verification: as in this document. Dependencies: none; the Kinetra pointer is unchanged. Follow-up: none.

## Open points

- `ui-launcher` in Kinetra still shows the old stills until the next Kinetra change that makes it stale refreshes it.
