# i4sim showcase clips

Status: implemented · Branch: `feat/i4sim-showcase-clips` · Related: [media capture tool](2026-11-19-media-capture.md), [i4sim showcase shuttle](2026-11-19-i4sim-showcase-shuttle.md), [media guide](../media/README.md), [capture tool](../../tools/capture/README.md)

The gallery shows i4sim only on the MAS demonstrator. The five showcase scenes on the loadscreen's SHOWCASE page (junction crossing, corridor passing, corridor recovery, predictive passing, H crossing) are what make the traffic policies visible, and they have no clip. The user wants a video of them. This change adds five `kinetra_capture` clips, one per scene, each one full cycle of the scene, and shows them in the README gallery.

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

- The i4sim clips in [docs/media](../media/README.md#files) show `MAS-demonstrator.json` at 800 ticks per second ([scenes.cpp](../../tools/capture/src/scenes/scenes.cpp)). No capture loads a file from [blueprints/showcase/](../../labs/i4sim/blueprints/showcase/).
- Every capture clip lasts a fixed 60 frames (four seconds). A showcase scene repeats on its own cycle: a shuttle scene drives out, pauses 400 ticks, drives back and pauses again; corridor recovery runs, pauses and restarts ([blueprint guide](../../labs/i4sim/blueprints/README.md#showcase-scenes)). The cycles take between 11 and 27 seconds at 200 ticks per second, and differ per scene. A fixed length cuts a scene off, or shows a jump when the GIF loops.
- The capture tool drives the session through `kinetra::Session` and cannot see the simulation, so a script cannot tell when the movers have come back.

## Goals and non-goals

- Goal: one looping GIF per showcase scene in `docs/media`, rendered by `kinetra_capture` like the other clips.
- Goal: each clip shows exactly one cycle, so the loop looks the same as the application repeating the scene.
- Goal: the clip length follows the scene. A change in the simulation or in a blueprint needs only a refresh, not a new length by hand.
- Non-goal: an MP4 of the scenes. Not chosen by the user; GitHub does not play repository MP4 files inside the README.
- Non-goal: changing the scenes, the HUD layout or the i4sim session.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Form | Five GIF clips, one per scene, in the README gallery | Chosen by the user over one MP4 or both |
| View | Debug Mode on with every visual layer, as the session starts; debug menu closed | Chosen by the user: paths, forces, borders and detection show why a mover waits, gives way or backs out |
| Length and speed | One full cycle per scene at the session's 200 ticks per second | Chosen by the user over a fixed, faster length or a single leg |
| How the length is found | Measured headless before recording: a `FactoryController`, the session's own headless half, runs the scene with the stage's seed, rate and step until the movers set off again after their last standstill | Same steps as the captured session, so the count is exact, and it follows any change to the scenes. Hard-coded lengths would go stale silently |
| End of a cycle | A shuttle cycle ends at the second departure after a standstill (out, pause, back, pause); a restarting cycle at the first (run, pause, restart) | The clip's last frame is in the final pause, and its first frame has the movers just setting off, as after the pause in the application |
| Scene names | `i4sim-showcase-<file>-clip` | Groups the files in `docs/media` and keeps the `-clip` suffix the tool requires |
| Update rate | A named `i4sim_updates_per_second` (200) in the stage, passed to `create_session` and used by the measurement | The measurement and the captured session must run at the same rate; before, the capture relied on `create_session`'s default argument |
| Clip size and rate | 960 × 540 at 15 frames per second, as the other clips | One encoding for all clips; the five GIFs are about 2.7 MB together |

## Design

```mermaid
flowchart LR
    Script["i4sim_showcase(stage, scene)"] --> Measure["showcase_cycle_steps(blueprint)"]
    Measure -->|"load, Space, steps of 1/60 s"| Controller["FactoryController (headless)"]
    Controller -->|"movers' lifecycle"| Measure
    Script -->|"start_i4sim, Space"| Stage
    Script -->|"record(steps / 4 frames)"| Stage
    Stage -->|"advance, render"| Session["i4sim session (same controller inside)"]
```

*Takeaway:* the measurement replays the scene headless with the same inputs and steps, so the recording knows its length before it starts.

- **`showcase.h`**: `int showcase_cycle_steps(const std::filesystem::path &blueprint)`. It reads the scenario (`load_scenario`), rejects a blueprint without a repeating scenario, loads the blueprint into a `FactoryController(capture_seed, paused, i4sim_updates_per_second)`, advances one step with Space and then steps without input, as the session's frame does (`begin_frame`, `suspend(false)`, `handle_input`). After each step it checks whether any mover is `moving()`. A departure is a step with a moving mover after a step in which every mover stood still; the function returns the step of the second departure for a shuttle and of the first for a restart. It gives up after 120 seconds of steps.
- **Script** `i4sim_showcase(stage, scene)`: starts `showcase/<scene>.json` with `Stage::start_i4sim`, measures, presses Space and records `steps / Stage::steps_per_frame` frames without input. The last frame therefore falls in the final standstill.
- **Scenes**: five clip entries in `scenes()`, after the i4sim clips, each a captureless lambda that names its file.
- **Script and encoding**: unchanged. `capture_media.sh` encodes every clip directory it finds.

## Affected areas

- `tools/capture/src/`: new `showcase.h` and `showcase.cpp`; `scenes.cpp` (five scenes and their script), `stage.h` and `stage.cpp` (the named update rate).
- `tools/capture/CMakeLists.txt`: the new source; `tests/capture_tests.cpp` and `tests/smoke.cmake`.
- `docs/media/`: five new GIFs, the media guide.
- `README.md` gallery, `tools/capture/README.md`, `labs/i4sim/blueprints/README.md` (link to the clips).
- Level: Full with graphics (a graphics tool and its smoke test change).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| A shuttle cycle holds both pauses, a restart cycle one; the count is the same on every run | `kinetra_capture_tests`: `showcase_cycle_steps` for every file in `blueprints/showcase/`, at least two (shuttle) or one (restart) pause of 120 steps, and equal on a second call | Targeted |
| A blueprint without a repeating scenario, or a missing file, is rejected | `kinetra_capture_tests`: `MAS-demonstrator.json` throws `std::invalid_argument`, a missing file `std::runtime_error` | Targeted |
| The five scenes exist, are clips, and have their GIFs in `docs/media`, the media guide and the gallery | The scene-list cases in `kinetra_capture_tests`, with the five names added | Targeted |
| A showcase scene renders end to end and starts from the catalog's subdirectory | `kinetra_capture_smoke` also runs `i4sim-showcase-junction-crossing-clip` and checks its frames | Full with graphics |
| The clips show one full cycle and loop cleanly | Run `capture_media.sh` for the five scenes; inspect the first, middle and last frames of each | Manual, recorded below |

## Review

`/code-review` (high) on `main...feat/i4sim-showcase-clips`, seven findings:

| Finding | Resolution |
| --- | --- |
| The media guide listed the five clip lengths in seconds, which go stale when a scene changes | Fixed: the guide says each clip lasts its scene's cycle |
| `handle_input` got the panel state as a positional `{false, true}` | Fixed: designated initializers name `pointer_captured` and `debug_mode` |
| The measurement repeats the session's frame order (`begin_frame`, `suspend`, `handle_input`) by hand | Kept: the order is the controller's documented contract (`controller.h`), and the frames show the result. A shared frame helper is an open point |
| The cycle test has only a lower bound, and the smoke test only checks that frames exist | Kept: an upper bound would need a second cycle detector. The first and last frames of every clip were checked by eye (see Verification), and the clips are identical over two runs |
| `i4sim_updates_per_second` repeats the 200 that `create_session` uses as its default | Kept: 200 also appears in the simulation core and `main.cpp`. Gathering it in one i4sim constant is a refactor of its own (open point) |
| Each scene names its file stem twice, in the scene name and in the lambda | Kept: five visible lines; deriving the file from the scene name would hide the link |

## Verification

Full level with graphics, in WSL2, on the final code (`019ece9`); the same three workflows also passed before the review fixes:

| Command | Result |
| --- | --- |
| `cmake --workflow --preset ci` | 201/201 passed, including the seven `kinetra_capture` cases; no warnings |
| `cmake --workflow --preset headless` | 187/187 passed; the tool is not built without the runtime |
| `cmake --workflow --preset graphics` | 215/215 passed, including `kinetra_capture_smoke` with the junction-crossing clip |
| `ctest --preset dev -R '^kinetra_capture'` | 7/7 passed; no warnings in `tools/capture` (`KINETRA_LAB_WARNINGS`) |
| `tools/capture/capture_media.sh build-graphics <five showcase scenes>` | Five GIFs, 960 × 540 at 15 fps (ffprobe): junction crossing 174 frames (11.6 s), corridor passing 397 (26.5 s), corridor recovery 192 (12.8 s), predictive passing 204 (13.6 s), H crossing 359 (23.9 s); 2.7 MB together |
| A second render compared with the first, before and after the review fixes | Identical GIFs and frames |
| Visual check of the first, middle and last frames of every clip | Each starts with the movers just setting off and ends with them standing at their start (shuttle) or at their goals before the restart (corridor recovery) |
| `clang-format --dry-run` on `tools/capture` | Clean |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |

Not run: the Windows cross build (no Windows file changed, and the tool is not built there without zlib).

## Pull request

Title: `feat(tools): GIF clips of the five i4sim showcase scenes`

```markdown
## Summary
The gallery showed i4sim only on the MAS demonstrator. `kinetra_capture` now also records the five showcase scenes from the loadscreen's SHOWCASE page, each one full cycle in Debug Mode at 200 ticks per second, and the README gallery shows them. A headless `FactoryController` replays each scene before recording to find where its cycle ends, so every GIF loops as the application repeats the scene and follows changes to the scenes.

## Changes
- **Capture tool** (`tools/capture/`): `showcase_cycle_steps` (new `showcase.h/.cpp`) measures one cycle: two departures after a standstill for a shuttle, one for a restarting scene. Five clip scenes `i4sim-showcase-<scene>-clip`. The i4sim rate is a named `i4sim_updates_per_second` passed to `create_session` (refactor commit).
- **Tests**: `kinetra_capture` checks the five names and their files, that each cycle holds its pauses and is the same on every run, and that a blueprint without a repeating scenario or a missing file is rejected. `kinetra_capture_smoke` also renders `i4sim-showcase-junction-crossing-clip`.
- **Media**: five GIFs, 960 × 540 at 15 fps, 12 to 26 s, about 2.7 MB together.
- **Docs**: README gallery table, media guide, capture tool README, blueprint guide link, design document.

## Design
[docs/design/2026-11-19-i4sim-showcase-clips.md](https://github.com/SMB-M87/Kinetra-Lab/blob/feat/i4sim-showcase-clips/docs/design/2026-11-19-i4sim-showcase-clips.md)

## Verification
- `cmake --workflow --preset ci` 201/201, `headless` 187/187, `graphics` 215/215 (with the smoke test); no warnings.
- `ctest --preset dev -R '^kinetra_capture'`: 7/7.
- `capture_media.sh` for the five scenes: two runs give identical GIFs; the first, middle and last frames of every clip checked by eye.
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.

## Review notes
- The measurement replays the session's frame order through `FactoryController`; if that order changes in `session.cpp`, the clips need a look.
- At 960 × 540 the HUD lines overlap the lower edge of the fitted floor; that is the session's layout, not the capture's (open point).

## Dependencies and merge order
None; no Kinetra change.

## Follow-up
- One named i4sim default update rate instead of the 200 in the simulation core, `main.cpp`, the session and the capture.
- A camera fit that leaves room for the HUD.
```

## Open points

- The default update rate of 200 ticks per second is written in the simulation core, `main.cpp`, `create_session` and now the capture's `i4sim_updates_per_second`. One named i4sim constant would keep them together.
- The measurement repeats the session's frame order. A helper in the i4sim logic that runs one session frame would serve both.
- At 960 × 540 the HUD's description and statistics lines overlap the lower edge of the fitted floor, and in the H crossing the producers below it. This is the session's layout at that window size, not the capture's; the camera fit does not leave room for the HUD.
