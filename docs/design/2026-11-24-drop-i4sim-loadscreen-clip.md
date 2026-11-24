# Drop the i4sim loadscreen clip

Status: accepted · Branch: `ccr-3004583e-n0otlj` · Related: [The labs in GIFs](2026-11-24-lab-readme-gifs.md), which added the clip

The i4sim section of the root README shows a GIF of the loadscreen with a sentence that introduces it. The user finds both superfluous. The capture tool requires every Lab clip to appear in the root README, so the clip goes as a whole: its scene, its script, its GIF and every page that names it.

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

- [README.md](../../README.md) shows `i4sim-loadscreen-clip.gif` with the sentence "The loadscreen: the floors, SHOWCASE and its junction crossing, BLUEPRINTS in the debug menu back to the catalog, and the `1680x960+` floor running:". The showcase table below it already shows the junction crossing, and the normal and debug clips show a running floor, so the clip repeats what the section shows.
- Removing only the README lines fails `kinetra_capture_tests`: the case *every scene has its file in its docs/media and media guide, and every clip is in its README* (`tools/capture/tests/capture_tests.cpp:153`) requires `docs/media/<clip>.gif` in the root README for every clip of Lab's gallery.
- The clip is also the only Lab tour clip: `i4sim_loadscreen` and its helpers (`catalog_row`, `require_blueprint`) take about 90 lines of `tools/capture/src/scenes/ui_clips.cpp`, and the media guide marks it stale for any change to the loadscreen, the catalog, the junction crossing, the `1680x960+` floor, the debug menu or the launcher's registrations.

## Goals and non-goals

Goals:

- The root README's i4sim section has no loadscreen GIF and no sentence for it.
- `kinetra_capture` no longer renders `i4sim-loadscreen-clip`; its script, its helpers and its GIF are gone.
- Every page that names the clip says so no longer: the media guide, the capture tool README and i4sim's README.

Non-goals:

- The tour video's i4sim chapter, which also opens the loadscreen (`tour_script.cpp`), stays.
- The mechanism that records a tour clip into Lab's gallery stays; Kinetra's shared-UI clips use it.
- The capture test's rule that a clip appears in its README stays.
- No other capture is refreshed: the change removes one, and changes what no other shows.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Remove only from the README, or the clip as a whole | The clip as a whole: scene, script, helpers, GIF and the docs that name it | The user's choice on 2026-11-24. A clip that no README shows would still be rendered and kept up to date; keeping it in i4sim's guide instead would loosen the capture test's rule |

## Design

- `scenes()` loses the entry `clip("i4sim-loadscreen-clip", i4sim_loadscreen)`.
- `ui_clips.h` loses `i4sim_loadscreen`; `ui_clips.cpp` loses its definition and what only it used: `i4sim_launcher_row`, `loadscreen_scene`, `loadscreen_floor`, `catalog_row`, `require_blueprint`, and the includes of `i4sim/blueprint/blueprint.h`, `<algorithm>` and `<filesystem>`.
- `docs/media/i4sim-loadscreen-clip.gif` is deleted.
- The scene-list case of `capture_tests.cpp` drops the name.
- Docs: the README's two lines; in i4sim's README the clip link after "blueprint screen"; in the media guide the file's row, the sentence on how it is played, its share of the GIF sizes and its entries in the staleness table; in the capture tool README the mentions in the introduction, the concepts, the file table, the GIF sizes, the tour-clip paragraph and the change table.

## Affected areas

- `tools/capture/src/scenes/scenes.cpp`, `tools/capture/include/capture/scenes/ui_clips.h`, `tools/capture/src/scenes/ui_clips.cpp`, `tools/capture/tests/capture_tests.cpp`
- `docs/media/i4sim-loadscreen-clip.gif` (deleted)
- `README.md`, `labs/i4sim/README.md`, `docs/media/README.md`, `tools/capture/README.md`

## Test plan

| Behavior | Proven by | Level |
| --- | --- | --- |
| The scene list no longer holds the clip | `kinetra_capture_tests`, *the scenes are the expected list, each once* | Targeted |
| Every remaining Lab clip has its GIF, its media-guide row and its place in the root README | `kinetra_capture_tests`, *every scene has its file in its docs/media and media guide, and every clip is in its README* | Targeted |
| The tool still builds and starts | `kinetra_capture` in the `graphics` preset, and `kinetra_capture_smoke` there as quick feedback | Targeted, then CI |
| No page links the deleted GIF | `check_docs.py`, and a search for `loadscreen` | Docs |

The change touches `README.md` and `docs/media/`, so CI builds in full; that is the Full level.

## Review

## Verification

## Pull request

## Open points
