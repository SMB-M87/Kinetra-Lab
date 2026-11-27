# i4sim start state: showcase scenes run, blueprints show RUN

Status: implemented · Branch: `ccr-39628c0f-9qj8md` · Related: [i4sim](../../labs/i4sim/README.md), [showcase scenes](../../labs/i4sim/blueprints/README.md#showcase-scenes)

A showcase scene is there to be watched, yet it loads paused, and a paused blueprint says so only in the small HUD line. This change starts a showcase scene running, and gives a blueprint that starts paused a RUN button at the top of the window until it runs for the first time.

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

- `FactoryController::load` (`labs/i4sim/src/controller.cpp:96`) runs a new simulation only when the controller was built running (`--running`); the launcher and the blueprint screen build it paused. A showcase scene, whose scenario section already sets its goals so that it plays by itself, therefore waits for Space; the [module README](../../labs/i4sim/README.md#build-and-run) has to tell the user to press it.
- `restart()` (`controller.cpp:196`) restarts every blueprint paused, a showcase scene included, so `R` stops the show.
- The only sign of the pause is `PAUSED` at the start of the lowest HUD line (`labs/i4sim/src/presentation/hud.cpp:39`), yellow text at scale 1.5 among the motion settings, and the status line of the window. A first-time user sees a floor that does not move.

## Goals and non-goals

Goals:

- A blueprint with a scenario section (a showcase scene) loads running, and `R` restarts it running.
- A blueprint without one loads paused, unless the session was started with `--running`, and `R` restarts it paused, as now.
- While a blueprint waits for its first run after a load or `R`, a RUN button is shown at the top of the window, centered below the toolbar. A click on it, Space, P or the menu's RUNNING entry starts the blueprint; the button disappears and does not return when the blueprint is paused later.
- The capture tool and the tour no longer press Space for a showcase scene, so its clip stays the same.

Non-goals:

- No change to the batch mode (`--ticks`), the simulation or the blueprint format.
- The tour video is not rendered again; it is not in the repository.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| What starts running | Showcase scenes start running; other blueprints start paused with a RUN button | The user's choice of 2026-11-27: a scene is a show, a floor is often set up before it runs |
| What marks a showcase scene | A scenario section in the blueprint, not the `showcase/` directory | The scenario is what makes a scene play by itself, and the controller sees it without knowing the catalog; a scene opened with `--blueprint` behaves the same |
| Form of the sign | A clickable RUN button, which Space also answers | The user's choice of 2026-11-27 |
| Where the button stands | Centered across the window, 30 pixels from the top: below the toolbar, above the fitted floor | The middle of the window, first proposed, covers the middle of the floor, where an operator selects movers and gives them goals before the start; the graphics test builds its junction setup that way and failed, and the tour does it too. The band above the floor is free after every fit |
| How long it stays | From a load or `R` that leaves the blueprint paused until it first runs | The user asked for a temporary button; a later pause is the user's own and needs no reminder |
| `R` | A showcase scene restarts running; a blueprint restarts paused and shows the RUN button again | The user's choice of 2026-11-27 |
| Where the button's box and click live | In the controller (logic): `start_button(viewport)` and `awaiting_start()`; the presentation only draws it | The click is tested headless, like the other pointer commands |

## Design

`FactoryController` keeps a flag `awaiting_start_`:

- `load()` runs the new simulation when the controller was built running or the blueprint has a scenario; `awaiting_start_` is set when it stays paused.
- `restart(bool running)` builds the fresh simulation, runs it when `running` and sets `awaiting_start_` when not. `R` passes whether a scenario is loaded; the scheduled replay of a restarting scene passes the run state it had, as before.
- `toggle_pause()` clears the flag once the simulation runs, so Space, P, the RUNNING entry and the button all end the wait. `awaiting_start()` is the flag while the simulation is paused.
- `handle_pointer()` first checks the button: while `awaiting_start()`, a click inside `start_button(camera.viewport)` is the button's, a left click runs the blueprint, and nothing in the world is picked. A panel that captures the pointer keeps it, as for any other click.

`start_button(viewport)` is a free function in `controller.h`: a box of 200 by 40 screen pixels, centered across the viewport and 30 pixels from its top, narrower in a viewport less than 232 pixels wide. The camera fit keeps 120 pixels of the height free and fills nine tenths of the rest, so the fitted floor starts at least 54 pixels plus a twentieth of the height from the top: below the button's 70 in a viewport at least 320 pixels high. In a lower one the floor reaches into the button's band, and a click there is the button's until the start. A button narrowed for a viewport under 232 pixels wide shows RUN without the hint. The button lights only where a click reaches it, not under the debug menu or the inspector. The session remembers whether the pointer was over it in `advance()` and `draw_start_button()` in `hud.cpp` draws it after the HUD and before the debug menu, the inspector and the settings panel: a panel body with an outline, a play triangle, `RUN` at scale 2 and `CLICK OR SPACE` at scale 1 in one row; under the pointer it is drawn in the accent color, like the overlay's buttons.

## Affected areas

- `labs/i4sim/include/i4sim/controller.h`, `labs/i4sim/src/controller.cpp`: load and restart rules, `awaiting_start()`, `start_button()`, the button's click.
- `labs/i4sim/src/presentation/session.cpp`, `hud.cpp`, `include/i4sim/presentation/detail/hud.h`: drawing the button.
- `labs/i4sim/tests/controller_tests.cpp`, `presentation_tests.cpp`.
- `tools/capture/src/scenes/scenes.cpp`, `showcase.cpp`, `tools/capture/src/tour/tour_script.cpp`: no Space for a showcase scene.
- Docs: `labs/i4sim/README.md`, `labs/i4sim/blueprints/README.md`, `labs/i4sim/tests/README.md`, `tools/capture/README.md` where they say a scene loads paused or waits for Space.
- Media: none. A showcase clip still starts with the scene running on its first step; the other i4sim captures start the floor with Space before their first frame.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| A blueprint loads paused and awaits its start; a controller built running does not | `controller_tests`: "a blueprint loads paused …", "a controller built running …" | Targeted |
| A showcase scene loads running without awaiting a start | `controller_tests`: "a showcase scene arrives …" | Targeted |
| Space, P and the RUNNING entry end the wait, and a later pause does not bring it back | `controller_tests`: "space and p …", "the run entries …" | Targeted |
| A click on the RUN button runs the blueprint and picks nothing; outside it, a click picks as before; a right click on it does nothing | `controller_tests`: new case "the run button starts a paused blueprint" | Targeted |
| `start_button` is centered below the toolbar, fits a small viewport and stays above the fitted floor | `controller_tests`: new cases "the run button is centered below the toolbar …", "the run button leaves the fitted floor free" | Targeted |
| `R` restarts a blueprint paused and awaiting its start, and a showcase scene running | `controller_tests`: "r restarts …", "r replays a scene's goals …" | Targeted |
| The session draws the button while awaiting, and not after Space; a click on it runs; a showcase scene loads RUNNING, and R keeps it running | `presentation_tests` (graphics) | Full (CI) |
| The capture tool's showcase cycle is still measured and its smoke test passes | `kinetra_capture_tests`, `kinetra_capture_smoke` | Targeted, Full (CI) |

## Review

Self-review of the branch diff against the workflow's checklist:

- The first placement, in the middle of the window, swallowed the Shift+click goals of the graphics test's junction setup: moved below the toolbar (decision above), with a test that the fitted floor stays free.
- `restart_when_due()` now passes the scene's run state to `restart(bool)` instead of running it again afterwards; a failed replay still closes the blueprint before the run state is touched.
- `check_copies.py` with `--term 'loads paused' --term 'press Space' --term 'restarts paused' --term 'waits for Space'`: the remaining hits describe ordinary blueprints, which still load and restart paused.
- No media refresh: the showcase clips replace their Space step by an empty step, so they record the same frames, and the other i4sim captures press Space before their first frame. The tour video is not in the repository and was not rendered.

Agent review round 1 on f22df81 (conclusion *Fix first*):

| Item | Resolution | Commit |
| --- | --- | --- |
| A scheduled replay of a paused restarting scene, stepped past its pause with SINGLE TICK, turned the RUN button on | `restart()` leaves `awaiting_start()` false; only `R` sets it for a blueprint it restarted paused. New controller case "a paused restarting scene stepped past its pause …", which failed before the fix | `bf5e36d` |
| `docs/media/README.md` still said a showcase scene starts after Space on the SHOWCASE page | Reworded; `git grep -n -i space` over the docs finds no other copy | this round's docs commit |
| The RUN button needs a screenshot on the pull request | The session's GitHub tools cannot upload an image to a comment; the 640 × 480 frame went to the user to attach, and the reply on the pull request says so | — |
| Nit: a finished simulation counted as awaiting its start | `awaiting_start()` requires `RunState::paused` | `bf5e36d` |
| Nit: the floor stays below the button only from a viewport 320 pixels high | Stated in `start_button()`'s comment and in *Design* | `3564dc6` |
| Nit: the hint ran out of a narrowed button | The hint is left out when it does not fit | `3564dc6` |
| Nit: the button lit under the inspector | Lit only when neither the debug menu nor the inspector captures the pointer | `3564dc6` |
| Nit: the blocked-exit steps listed the goals after the second Space | Reordered in `labs/i4sim/tests/README.md` | this round's docs commit |

## Verification

- `cmake --build --preset dev --target kinetra_i4sim_controller_tests` and `ctest --preset dev -R '^kinetra_i4sim_controller'`: 47 of 47 passed.
- `cmake --build --preset graphics --target kinetra_i4sim_presentation_tests kinetra_i4sim kinetra_capture_tests kinetra_capture` without warnings, and `SDL_VIDEODRIVER=offscreen ctest --preset graphics -R '^kinetra_i4sim_graphics|^kinetra_capture'`: 39 of 39 passed, `kinetra_capture_smoke` included.
- `cmake --build --preset release --target kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim kinetra_i4sim_motion_benchmark` and `ctest --preset release -R '^kinetra_i4sim'`: 168 of 168 passed.
- The button looked at in a 640 × 480 frame of the graphics test: centered below the toolbar, above the floor's top edge.
- `python3 external/Kinetra/tools/check_docs.py` and `check_copies.py`: see Review.
- After merging `main` (with the make-way showcase scene and the settings panel's BLUEPRINTS), again: `ctest --preset release -R '^kinetra_i4sim'` 168 of 168, and `SDL_VIDEODRIVER=offscreen ctest --preset graphics -R '^kinetra_i4sim_graphics|^kinetra_capture'` 41 of 41.
- The tour, which drops its Space for the H crossing scene and which CI does not play, played to its end: `SDL_VIDEODRIVER=offscreen kinetra_capture --tour /dev/null` wrote 19108 frames (318 s) and exited with 0.
- Round 1: `ctest --preset dev -R '^kinetra_i4sim_controller'` 48 of 48 (the new case failed before `bf5e36d`); `SDL_VIDEODRIVER=offscreen ctest --preset graphics -R '^kinetra_i4sim_graphics'` 6 of 6.
- Full level: the pull request's CI.

## Pull request

Title: `feat(i4sim): start showcase scenes running and show a RUN button`

Summary: a showcase scene (a blueprint with a scenario section) now starts running when it loads and when `R` restarts it. A blueprint without one still starts paused, and now shows a RUN button at the top of the window until it runs for the first time; a click on it, Space, P or the RUNNING entry starts it, and `R` brings it back.

Changes: `FactoryController` (load and restart rules, `awaiting_start()`, `start_button()`, the button's click), the session and HUD draw the button, controller and graphics tests, the capture tool and tour no longer press Space for a scene, and the i4sim, blueprint, test and capture READMEs.

Design: [this document](2026-11-27-i4sim-start-state.md). Verification: above, and CI. Dependencies: none; Kinetra is unchanged.

## Open points

- None.
