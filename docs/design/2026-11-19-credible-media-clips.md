# Credible media clips: aimed fire in Dotio, a larger i4sim floor

Status: implemented · Branch: `feat/credible-media-clips` · Related: [media capture tool](2026-11-19-media-capture.md), [i4sim showcase clips](2026-11-19-i4sim-showcase-clips.md), [media guide](../media/README.md), [capture tool](../../tools/capture/README.md)

The gallery's Dotio player fires at a fixed point of the screen, whether an enemy is there or not, and its debug clip is too short and too close to show what the debug layers are for. The i4sim captures show the small MAS demonstrator at four times its speed. The user asked for a Dotio debug clip with believable shooting, more insight into the AI (recovery in particular) and a wider view, and for i4sim captures of a larger floor with good-looking, smooth traffic. This change gives the capture tool a scripted Dotio player that aims at the enemies it sees, and moves the i4sim captures to the `4320x2400` floor with the Reciprocal profile.

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

**Dotio.**

- `dotio_play` in [scenes.cpp](../../tools/capture/src/scenes.cpp) parks the pointer at 70% / 45% of the window and holds the button for the first half of every second. The bolts fly at walls; an enemy is hit by chance.
- The debug clip lasts four seconds. An enemy leaves the Attack state for Recover only when its ammunition is spent: nine shots at one per second on Medium, with the player in sight all the time ([state.cpp](../../labs/dotio/logic/src/state.cpp), `AttackState::Update`). Reloading in shelter takes another five seconds. Neither fits in the clip, so the RECOVERY path filter and HIDING HITS, two of the rows the script turns on, never draw anything.
- The debug menu stays open and covers the upper-left fifth of the 960 × 540 frame. Three wheel notches out show 12.8 × 7.2 cells.
- The capture tool drives a session through `kinetra::Session` only, and Dotio's AI draws from `std::random_device`, so a script cannot know in advance where an enemy will be.

**i4sim.**

- The four i4sim captures load `MAS-demonstrator.json`: 11 movers on a 2160 × 1200 mm floor. The clips run at 800 ticks per second, because at 200 the movers barely move in four seconds; at 15 frames per second a mover then jumps most of its own length per frame.
- They use the default motion settings, Improved with heat A*. On a busy floor that profile corrects its heading constantly (see the measurements under [Decisions](#decisions)).

## Goals and non-goals

- Goal: the Dotio player of every Dotio capture aims at an enemy it can see and holds fire without one.
- Goal: the Dotio debug clip shows the AI cycle, with at least one enemy in Recover when it starts, in a wider view with the whole frame free.
- Goal: the i4sim screenshots and clips show a larger floor with the motion settings that look smoothest, at the session's own speed.
- Goal: a menu change that moves the rows the i4sim scripts click fails a test instead of silently changing a picture.
- Non-goal: seeding Dotio's randomness. It stays a roadmap item, so Dotio captures still differ per run.
- Non-goal: changing an application's behavior, its menus or its blueprints. The six original blueprints stay unchanged.
- Non-goal: the tour video, and the Demo, launcher and showcase captures.

## Decisions

The user left the choice of floor and motion settings to what looks best and smoothest, and named plain A* with the Reciprocal profile as the expectation. The other choices are capture details that a refresh can change; they were made here and are listed for the reviewer.

| Decision | Choice | Why |
| --- | --- | --- |
| How the script learns where the enemies are | `dotio::create_session` returns a `dotio::Session`, a `lab::Session` that also gives its `Application` | The tool can then read the world and answer with ordinary input, through the real session. Finding enemies in the rendered pixels would be fragile; composing a second session in the tool from `Application` and `render` would capture something other than the application |
| Where the aiming lives | Free functions in the capture tool ([dotio_player.h](../../tools/capture/src/dotio_player.h)) | It is capture policy, not a game rule, and it is testable without a window. Nothing is added to the game for it |
| Target and aim | The nearest live enemy on screen that a bolt can reach in a straight line (no wall and no crate in between), led by its velocity for the bolt's flight time | What a player does: shoot at the closest threat in view. The wall check is the sight line the enemies' AI uses; crates stop bolts, and the room the player starts in has one. Bolts fly at 6 px per tick, so an unled shot misses a moving enemy |
| Fire | Half-second bursts while a target is in sight, in both views | Kept from the old script: outside Debug Mode a shot costs 10 mana, and bursts make it last |
| Start of the debug clip | Hold fire until an enemy enters Recover at least a fifth of the window's height inside the view, at most 60 s, then record | The AI is not seeded, so a fixed warm-up cannot promise a recovery. A player that shoots from the start kills an enemy (two hits on Medium) long before its ninth shot. An enemy at the edge runs for shelter and is gone in the first frames |
| Moment of a Dotio screenshot | After the warm-up the player plays on, 20 s at most, until an enemy is in sight and a bolt is on its way | A fixed moment can show an empty corridor, more often now that the player hits what it aims at |
| Movement in the debug clip | A half-second sidestep every two seconds, in turn right, down, left and up | The player holds its ground, so paths and sight lines stay readable, and the camera rests three quarters of the time: 1.4 MB against 3 MB for the running square, 0.5 MB for a player that never moves |
| Dotio debug clip view | Nine wheel notches out (10.6 px per unit, 22.6 × 12.7 cells), debug menu closed, twelve seconds | "Zoom out somewhat more": the whole spawn area is in view and labels stay readable. The screenshot next to it keeps the menu with every row. Twelve seconds hold a flight to shelter, the reload and the return |
| Other Dotio captures | Same player, otherwise unchanged: the running square, four seconds, the debug screenshot with its menu at three notches | They share the script; a normal clip that still fired at walls would contradict the debug clip |
| i4sim floor | `4320x2400`: 29 movers, 18 producers | Of the larger originals it is the busiest that stays readable at 960 × 540. `3840x2400` has 13–27% of its movers driving in the measured windows against 22–56% here. On `8640x4800` the mover labels cover the floor |
| i4sim motion settings | Reciprocal profile, plain A*, traffic policies off | Measured below: Reciprocal turns five to twenty times less per meter than the other profiles and has no contacts. With plain A* the routes are long straight lines; with heat A* they are staircases around producers |
| i4sim speed and length | 200 ticks per second, twelve seconds, after 116 s | The session's own rate: a mover covers about 4 px per clip frame instead of 17. Ticks 23200–25600 are the busiest stretch of this floor with the capture seed |
| i4sim debug clip menu | Closed; the debug screenshot keeps LAYERS open | At 960 × 540 the open menu would cover a fifth of this floor. Same choice as for the showcase clips |
| Guarding the clicked rows | The i4sim clicks are a named list of row and expected label; a window-free test replays it on a `FactoryController`. A second test replays the Dotio debug rows on `Application::debug_options()` | A script that clicks a row by number breaks silently when the menu changes ([capture tool](../../tools/capture/README.md#changing-this-module)) |
| Warnings in Dotio's headers | Six implicit `int` to `float` conversions in `gmath.h`, `world.h` and `application.h` made explicit | The capture tool is built with warnings as errors and now includes `application.h`. Fixing the cause keeps the tool's directory strict; suppressing the warnings around the include would hide new ones |

**Measurements for the motion settings.** `4320x2400`, seed 42, measured with a throwaway probe over `Simulation::tick` and `FlowMetrics` (not part of this change). *Driving* is the share of mover-ticks above 0.3 mm per tick; *turning* is the summed heading change of driving movers per meter driven; *waiting* is `FlowMetrics::waiting_ticks`.

| Settings | Ticks 8000–12000: driving, turning, waiting, contacts | Ticks 23200–25600 (the clip): driving, turning, waiting, contacts |
| --- | --- | --- |
| Improved, heat A* (default) | 39%, 66 rad/m, 47, 0 | 32%, 55 rad/m, 37, 0 |
| Original, heat A* | 41%, 63 rad/m, 88, 0 | 42%, 40 rad/m, 27, 0 |
| RVO experiment, heat A* | 39%, 92 rad/m, 256, 25 | 52%, 110 rad/m, 393, 0 |
| Reciprocal, heat A* | 39%, 6.4 rad/m, 523, 0 | 36%, 2.8 rad/m, 2380, 0 |
| **Reciprocal, plain A*** | 32%, 5.9 rad/m, 5098, 0 | **53%, 12.6 rad/m, 810, 0** |
| Reciprocal, plain A*, predictive passing | 35%, 3.7 rad/m, 4402, 0 | 30%, 6.8 rad/m, 4602, 0 |

Reciprocal with plain A* waits more than with heat A* over a long run, because movers stand and wait for each other instead of driving around; that is why the clip takes the stretch where most of them drive. Predictive passing adds standstills (both movers stop at their entry points first) and is shown by its own showcase clips.

## Design

```mermaid
sequenceDiagram
    participant Sc as Dotio script (scenes.cpp)
    participant P as dotio_player
    participant St as Stage
    participant S as dotio::Session
    participant A as Application and World
    Sc->>St: start_dotio()
    St->>S: create_session(seed, Medium)
    St->>S: application()
    loop every clip frame
        Sc->>P: dotio_aim(application, camera)
        P->>A: player, enemies, wall grid (read only)
        P-->>Sc: screen point, or nothing in sight
        Sc->>St: aim(point), frame input with the fire button
        St->>S: advance(1/60 s, input) four times
        S->>A: commands, ticks
    end
```

*Takeaway:* the script reads the world between frames and answers with a pointer position and a button, so the session still gets nothing but input.

**Dotio session** ([session.h](../../labs/dotio/include/session.h)). `dotio::Session` derives from `lab::Session` and adds `virtual Application &application()`. `create_session` returns `std::unique_ptr<dotio::Session>`; the launcher and `main.cpp` keep compiling, because the pointer converts to its base. `Application` is forward-declared, so the launcher still does not compile the world.

**Scripted player** (`tools/capture/src/dotio_player.h`, `.cpp`):

- `dotio_target(application, camera)`: the nearest enemy that is active, not frozen, with its center inside the camera's viewport, and that a bolt reaches in a straight line: no wall cell on the segment from the player (`Grid::IsIntersectingAnyCell`, as `Enemy::IsInLineOfSight`) and no crate within the bolt's radius of it. Null without one.
- `dotio_bolt_speed(player)`: the launch speed of `Player::Shoot`, which grows with the kills. A test fires a bolt and compares, so the two cannot drift apart unnoticed.
- `lead_offset(offset, velocity, bolt_speed)`: solves `|offset + velocity·t| = bolt_speed·t` for the earliest positive `t` and returns `offset + velocity·t`; returns `offset` when there is no solution or the flight would take more than 90 ticks.
- `dotio_aim(application, camera)`: the target's led position as a screen point, clamped to the viewport; empty without a target.
- `dotio_recovery_in_view(application, camera, margin)`: whether an enemy in Recover is on screen at least `margin` pixels from every edge.

**Stage.** `start_dotio()` starts Dotio as the launcher's entry does (camera at the origin and `dotio::initial_zoom`, capture seed, Medium) and keeps the application for `dotio()`. `play_until(limit, input, done)` advances undrawn clip frames until `done()` holds or `limit` frames have passed, and returns the count.

**Dotio scripts.** Every Dotio scene but the settings screenshot starts with `start_dotio`. `dotio_shoot` aims the pointer and adds the fire button to a frame's input; `dotio_play` is the running square with it, for the screenshots and the normal clip. A screenshot (`dotio_shot`) plays on until a fight is in view and parks the pointer first, because on the open debug menu it would highlight a row. The debug clip turns on Debug Mode and the rows as before, closes the menu, zooms nine notches, then `play_until` an enemy recovers with sidesteps and no fire, and records 180 frames of sidesteps with `dotio_shoot`. If the 60 s pass without a recovery it records anyway and says so on standard error.

**i4sim scripts.** `scenes.h` gets `MenuClick{row, label}`, the list `i4sim_motion_clicks` (MOTION, PROFILE, RECIPROCAL, PLANNER, A*, counted in the menu as each click finds it), `i4sim_layers_click` and the blueprint's file name. `i4sim_start` loads `4320x2400.json`, opens the debug menu, clicks the list, then either clicks LAYERS (debug screenshot) or closes the menu, turns Debug Mode off for the normal view, presses Space and waits 116 s. The clips record 180 frames without the two `E` presses.

## Affected areas

- `labs/dotio/include/session.h`, `labs/dotio/src/session.cpp`: the session type and the factory's return type. No behavior changes.
- `labs/dotio/gmath/include/gmath.h`, `labs/dotio/logic/include/world.h`, `labs/dotio/include/application.h`: six explicit conversions, in a refactor commit.
- `tools/capture/src/`: new `dotio_player.h/.cpp`; `stage.h/.cpp` (`start_dotio`, `dotio`, `play_until`); `scenes.h/.cpp` (the Dotio and i4sim scripts, the click list); `CMakeLists.txt`.
- `tools/capture/tests/capture_tests.cpp`, `tests/smoke.cmake`.
- `docs/media/`: the five Dotio and four i4sim files; the media guide.
- `README.md` gallery captions, `tools/capture/README.md`, the Dotio header guide and architecture page.
- Level: Full with graphics (a `session.cpp` changes, and the capture tool with its smoke test).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| A led shot meets a moving target; a target that cannot be caught, or only after 90 ticks, is aimed at directly | `kinetra_capture_tests`: `lead_offset` for a resting target, a crossing one (the returned point satisfies the intercept equation), one that outruns the bolt, one too far away and one as fast as the bolt | Targeted |
| The player targets the nearest enemy in sight and ignores one behind a wall, one off screen, a frozen one and an inactive one | `kinetra_capture_tests`: enemies placed by hand around the player of a headless `dotio::Application` whose wall grid is cleared | Targeted |
| An enemy behind a crate is no target | `kinetra_capture_tests`: an enemy behind the crate of the player's room and one as far away on the other side | Targeted |
| The lead uses the speed of the bolt the player fires | `kinetra_capture_tests`: `dotio_bolt_speed` against the velocity of a bolt from `Player::Shoot`, at 1 and at 40 kills | Targeted |
| The aim point is the target's screen position, led when it moves, inside the window; empty without a target | `kinetra_capture_tests`: `dotio_aim` on the same world | Targeted |
| A recovering enemy is found inside the margin; one outside it, off screen or in another state is not | `kinetra_capture_tests`: `dotio_recovery_in_view` | Targeted |
| The i4sim clicks find the rows they name and leave the Reciprocal profile with plain A*; LAYERS is where the screenshot clicks it | `kinetra_capture_tests`: replay `i4sim_motion_clicks` on a `FactoryController` with the captured blueprint | Targeted |
| The Dotio debug rows turn on every row but GHOST CAMERA | `kinetra_capture_tests`: replay the rows on `Application::debug_options()` | Targeted |
| The explicit conversions change nothing | The existing `kinetra_dotio` suite | Targeted |
| The session gives the application it runs; the Dotio debug clip and the i4sim debug clip render end to end | `kinetra_capture_smoke` runs `dotio-debug-clip` next to `i4sim-debug-clip` and checks their frames | Full with graphics |
| The captures show what this document says | Run `capture_media.sh` for the eight scenes and look at every file | Manual, recorded below |

## Review

`/code-review` (high) on `main...feat/credible-media-clips`, eight findings, and one from the `ci` workflow:

| Finding | Resolution |
| --- | --- |
| The `ci` preset builds `tools/capture` with warnings as errors, and Dotio's headers had six implicit `int` to `float` conversions | Fixed in Dotio's headers with explicit conversions (refactor commit) |
| The Dotio debug screenshot left the pointer where the player last aimed, which can be on the open debug menu and highlight a row | Fixed: a screenshot parks the pointer first |
| The warning about a clip without a recovery compared frame counts and also fired when the recovery came in the last waited frame | Fixed: it asks the world again |
| The bolt speed repeated the formula of `Player::Shoot` | Fixed: `dotio_bolt_speed` is tested against a bolt the player fires |
| The design document named a function that was renamed | Fixed |
| A crate between the player and an enemy still counted as a clear line, so the player shot the crate | Fixed: crates block the shot, with a test |
| The aim point can land on the SETTINGS and DEBUG buttons: the text highlights and the session drops the shot | Kept: it is what the application does for a pointer there, and the buttons are 112 × 24 pixels (open point) |
| The fire rule of `dotio_shoot` and `Stage::play_until` are only run by the smoke test | Kept: both need a `Stage`, which needs a window (open point) |
| The i4sim warm-up is fitted to one seed and today's simulation | Kept and documented: the simulation is deterministic, so the clip changes only when the simulation does (open point) |

Looking at the first render also changed two things: the recovering enemy must be a fifth of the window's height inside the view, and a Dotio screenshot waits for a fight in view.

## Verification

Full level with graphics, in WSL2, on the final code (`f927078`; only documentation changed after it):

| Command | Result |
| --- | --- |
| `cmake --workflow --preset ci` | 209/209 passed, including the 15 `kinetra_capture` cases; `tools/capture` builds with warnings as errors. The first run, before the review, failed to compile on the six conversions in Dotio's headers (see Review) |
| `cmake --workflow --preset headless` | 187/187 passed; the tool is not built without the runtime |
| `cmake --workflow --preset graphics` | 223/223 passed, including `kinetra_capture_smoke` with the Dotio and i4sim debug clips |
| `cmake --workflow --preset windows` | 205/205 passed; run because Dotio's session header changed. None of its 109 reported warnings is in a changed line |
| `ctest --preset dev -R '^kinetra_(capture\|dotio):'` | 36/36 passed; no warning in `tools/capture` |
| `tools/capture/capture_media.sh build-graphics <eight scenes>` | Four PNGs at 1920 × 1080 and four GIFs at 960 × 540 and 15 fps (ffprobe): `dotio-clip` 60 frames, 0.4 MB; `dotio-debug-clip` 180 frames, 1.3 MB; `i4sim-clip` 180 frames, 0.9 MB; `i4sim-debug-clip` 180 frames, 1.7 MB. The six application clips are 5.1 MB together |
| A second render of the four i4sim scenes compared with the committed files | Identical PNGs and GIFs |
| Six takes of `dotio-debug-clip`; contact sheets of every take compared | In all six the wait found a recovery before its 60 s limit (no warning was printed); earlier trial runs waited 11 to 18 s. The third take is committed: it has two enemies in Recover in its first frame and enemies around the player until its last |
| Visual check of every refreshed file | Dotio: bolts fly at enemies in sight in both screenshots and both clips; the debug clip shows RECOVER with its sight line and wall hit, PURSUE with its path, ATTACK, KAMIKAZE and WANDER, and no highlighted menu row. i4sim: `AVOID LIMIT` in the statistics confirms Reciprocal, the routes are straight lines, CONTACTS stays 0, and 12 to 18 movers drive |
| `clang-format --dry-run` on `tools/capture` and the changed Dotio sources | Clean |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |

## Pull request

Title: `feat(tools): credible Dotio shooting and a larger i4sim floor in the media`

```markdown
## Summary
The gallery's Dotio player fired at a fixed point of the window, and its debug clip was too short and too close to show what the debug rows are for. The i4sim captures showed the small MAS demonstrator at four times its speed. The capture tool now scripts the Dotio player against the world of the running session, so it shoots at the enemies it sees, and the Dotio debug clip shows twelve seconds of the AI cycle from a wide view. The i4sim screenshots and clips show the `4320x2400` floor with the Reciprocal profile and plain A* at the session's own 200 ticks per second.

## Changes
- **Dotio** (`labs/dotio/`): `create_session` returns `dotio::Session`, a `lab::Session` that also gives its `Application`, so a tool can read the world and answer with ordinary input. No behavior changes. Six implicit `int` to `float` conversions in `gmath.h`, `world.h` and `application.h` are explicit (refactor commit), because the capture tool is built with warnings as errors and now includes `application.h`.
- **Scripted player** (`tools/capture/src/dotio_player.*`): the nearest live enemy on screen that a bolt can reach (no wall, no crate), led by its velocity for the bolt's flight time; no target, no fire. `Stage::start_dotio`, `Stage::dotio` and `Stage::play_until` serve the scripts.
- **Dotio scenes**: screenshots wait until the player shoots at an enemy in sight. The debug clip closes the menu, zooms out nine notches, waits without firing until an enemy enters Recover inside the view (60 s at most) and records twelve seconds.
- **i4sim scenes**: `4320x2400.json`; PROFILE RECIPROCAL and PLANNER A* through the debug menu; 116 s warm-up into the busiest stretch; twelve-second clips at 200 ticks per second, the debug clip with the menu closed.
- **Tests**: the i4sim menu clicks are a named list of row and label that a window-free test replays on a `FactoryController`; another replays the Dotio debug rows. The player's target (walls, crates, screen edge), bolt speed, lead, aim point and recovery check are tested on a headless `dotio::Application`. `kinetra_capture_smoke` also renders `dotio-debug-clip`.
- **Media**: `dotio.png`, `dotio-debug.png`, `dotio-clip.gif`, `dotio-debug-clip.gif`, `i4sim.png`, `i4sim-debug.png`, `i4sim-clip.gif`, `i4sim-debug-clip.gif`. The six application clips are 5.1 MB together (was 2 MB).
- **Docs**: README gallery captions, media guide, capture tool README, Dotio header guide and architecture page, design document.

## Design
[docs/design/2026-11-19-credible-media-clips.md](https://github.com/SMB-M87/Kinetra-Lab/blob/feat/credible-media-clips/docs/design/2026-11-19-credible-media-clips.md), with the measurements behind the choice of floor and motion settings.

## Verification
- `cmake --workflow --preset ci` 209/209, `headless` 187/187, `graphics` 223/223 (with the smoke test), `windows` 205/205.
- `ctest --preset dev -R '^kinetra_(capture|dotio):'`: 36/36; no warning in `tools/capture`.
- `capture_media.sh` for the eight scenes: every file checked by eye. A second render of the four i4sim scenes is identical to the committed files.
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.

## Review notes
- **The i4sim settings are a choice.** Reciprocal turns five to twenty times less per meter than the other profiles and has no contacts; with plain A* the routes are straight lines. Over a long run it waits more than with heat A*, so the clip takes the stretch where 12 to 18 of the 29 movers drive. The measurement table is in the design document.
- **Dotio captures still differ per run.** The debug clip in this pull request is one of six takes, chosen by eye; a refresh needs a look.
- The Dotio debug clip and the i4sim debug clip no longer show the debug menu; the debug screenshots next to them do.
- `dotio::create_session` changed its return type to a derived session type; the launcher and `kinetra_dotio` compile unchanged.

## Dependencies and merge order
None; no Kinetra change.

## Follow-up
- Seed Dotio's randomness (quality roadmap), so its captures become reproducible and the debug clip no longer needs a chosen take.
- Keep the scripted player's aim point off the SETTINGS and DEBUG buttons.
- A window-free test for the fire rule and `Stage::play_until` needs a `Stage` without a window.
```

## Open points

- Dotio captures are not reproducible until its randomness is seeded ([quality roadmap](2026-11-18-quality-roadmap.md)). The debug clip therefore waits for a recovery instead of a fixed time, and its take is chosen by eye.
- The i4sim warm-up of 116 s is tied to the capture seed and today's simulation. A change in the simulation moves the busy stretch; the clip then still renders, but may show a quieter moment.
- The scripted player's aim point is not kept off the SETTINGS and DEBUG buttons. An enemy behind them is not shot at while the pointer is there.
- `dotio_shoot`'s fire rule and `Stage::play_until` have no window-free test; a `Stage` needs a `Runtime`.
- The settings rows are still clicked by number without a label check.
