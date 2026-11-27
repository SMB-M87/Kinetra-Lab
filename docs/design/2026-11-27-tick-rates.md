# Tick rates: typed UPS and FPS in every lab, and runs of a million ticks

Status: accepted · Branch: `ccr-74d1b15a-ig41um` (step 1), later steps below · Related: [i4sim](../../labs/i4sim/README.md), [performance](../performance.md), [motion benchmark](../../labs/i4sim/tests/README.md#motion-benchmark)

Every lab gets a UPS row (ticks per second) and an FPS row in its debug menu, where a number can be typed as well as halved or doubled, up to a million ticks per second. The mechanism lives in Kinetra. Headless runs and the motion benchmark run a million ticks by default and report ticks per second, and the largest hotspot of a long run is optimized. The user asked for all of this on 2026-11-27: i4sim's UPS was hard to tune, stopped at 10000, existed only in i4sim, and a 50000-tick benchmark says little.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Steps](#steps)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Verification](#verification)
- [Review](#review)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

- **Only i4sim has a UPS, and only two buttons.** Its `UPS` row halves or doubles within 1..10000 (`labs/i4sim/src/controller.cpp:18`, `labs/i4sim/src/simulation/simulation_core.cpp:102`, `labs/i4sim/src/main.cpp:56`). A specific rate such as 750 cannot be set. Demo and Dotio have a `SPEED` row instead, a time scale of 0.125 to 4 (`labs/shared/include/kinetra_lab/debug.h`).
- **10000 is not the real ceiling.** i4sim's `FixedClock` runs at most 256 ticks per frame (`simulation_core.cpp:105`), so at 60 frames per second no setting gives more than about 15,000 ticks per second; Demo's clock stops at 16 per frame, Dotio's at 8.
- **No number can be typed.** `kinetra::Input` reports a fixed set of keys, digits only 1 to 6 (`external/Kinetra/include/kinetra/view/input.h`), and a `DebugEntry` has only `[-]` and `[+]`.
- **Frame rate is fixed.** `Runtime` uses VSync when the platform accepts it and otherwise paces at the display's refresh rate; only a capture program can switch the pacing off (`Runtime::limit_frames`).
- **Headless runs ignore UPS.** `kinetra_i4sim --ticks N` runs as fast as the processor allows; it reports no rate. The motion benchmark runs 50,000 ticks by default (`labs/i4sim/tests/motion_benchmark.cpp:72`).
- **How fast a headless run is.** Measured on 2026-11-27 in the `release` preset, cloud machine with four cores, `kinetra_i4sim --ticks 50000`:

  | Blueprint | Movers | Ticks per second |
  | --- | --- | --- |
  | `MAS-demonstrator` | 11 | 185,000 |
  | `1680x960+` | | 206,000 |
  | `3840x2400` | 25 | 72,000 |
  | `8640x4800` | 106 | 9,500 |

  A million ticks per second is 5 to 100 times faster than that.

## Goals and non-goals

Goals:

- Every lab (Demo, Dotio, i4sim) shows UPS and FPS rows in its debug menu. A number is typed by holding the pointer over the row's value and typing digits; Enter applies it, Escape or moving the pointer away discards it. `[-]` and `[+]` halve and double from the value in use.
- UPS ranges over 1 to 1,000,000 in every lab. In a window the clock runs as many ticks per frame as fit in a time budget, so the window stays responsive, and the measured UPS shows how many ran.
- FPS is VSYNC, a limit of 1 to 1000 frames per second, or UNCAPPED.
- The mechanism is Kinetra's: typed number rows, the budgeted clock and the frame-rate limit. The rates and limits each lab uses are its own policy ([Kinetra rule](../../external/Kinetra/AGENTS.md#architecture-boundaries): fixed rates are application policy).
- `kinetra_i4sim --ticks N` reports ticks per second. The motion benchmark runs 1,000,000 ticks by default, accepts up to 100,000,000, and reports `ticks_per_second`.
- The largest hotspot of a long i4sim run is optimized, measured before and after.

Non-goals:

- Reaching a million ticks per second on the large factories. After the hotspot of step 2, further optimization is a roadmap item.
- A CTest case with a minimum throughput: machine-dependent and flaky in CI.
- Typed values in rows other than UPS and FPS. The mechanism allows it; the rows stay as they are.
- Changing the tick length of any lab: the physics, and with them every recorded run, stay the same.

## Decisions

Settled with the user on 2026-11-27, in three rounds of questions.

| Decision | Choice | Why |
| --- | --- | --- |
| Meaning of UPS in Demo and Dotio | Ticks per wall-clock second at the fixed tick length; it replaces SPEED (Demo 120 = 1×, Dotio 60 = 1×) | Behavior is the same at every UPS, as in i4sim; two rows for one setting would confuse |
| Range of UPS | 1 to 1,000,000 | Well above what a processor reaches now; the measured UPS shows when it falls behind |
| Clock in a window | A time budget per frame; the backlog beyond it is dropped | The window stays responsive at any setting; a fixed tick count per frame either caps UPS or freezes large factories |
| FPS | A row with VSYNC, a typed or halved/doubled limit, and UNCAPPED | Fewer frames leave more of the processor for ticks at a high UPS |
| Typing a number | Hover over the row and type (the user's choice; the recommendation was a click on the value) | Fastest to use; digits typed with the pointer elsewhere still reach the application |
| Pointer leaves the row while typing | Discard; Escape discards too, and does not close the window while typing | A half-typed number never changes a setting by accident |
| Benchmark | 1,000,000 ticks by default, at most 100,000,000, a `ticks_per_second` column; `--ticks` reports ticks per second | A long run measures throughput and the behavior of a factory over hours of simulated time |
| When to optimize | In this stack, right after the benchmark (the user's choice; the recommendation was a roadmap item after it) | The gain shows early and does not depend on the menu work |
| Scope of the optimization | The largest hotspot of the profile; further ones become roadmap items | A pull request per hotspot, measured before and after |
| Results of the optimization | May shift slightly; golden lines are refreshed, and the benchmark's metrics (completed, contacts, stuck movers) must not get worse (the user's choice; the recommendation was bit-identical results) | Leaves room for gains that change the order of floating-point work |
| Cut | Five pull requests, below | Each step can be reviewed on its own, and the benchmark is useful before the menu work |
| Location of this document | Kinetra-Lab | Three of the five steps, the first among them, are Lab's; the Kinetra pull requests link here |

Smaller choices, proposed and not contested: the per-frame budget is one frame interval of the FPS target (the display's refresh interval with VSync, 1/N with a limit, 1/60 uncapped); FPS limits 1 to 1000; i4sim's HUD shows the target and the measured UPS; Q and E keep halving and doubling i4sim's UPS, and Demo and Dotio get no new shortcuts; a typed value holds digits only, at most seven.

## Design

### Step 1: a million ticks, and ticks per second

- `motion_benchmark.cpp`: `ticks` defaults to 1,000,000 and accepts 1..100,000,000. The CSV gains `ticks_per_second`, the ticks run divided by the simulation's wall-clock time; `elapsed_ms` stays, so older CSVs compare column by column. Ticks run is counted, because a simulation that stops running ends the loop early.
- `kinetra_i4sim --ticks N` adds the rate to its summary: `i4sim: N ticks, M completed products, R ticks/s`. The rate covers the tick loop only, not loading or `--output`. The CTest case that matches the summary (`labs/shared/tests/inspector_trace.cmake`) still matches.

### Step 2: the hotspot of a long run

Profiled on 2026-11-27 with `perf` on a frame-pointer build ([how](../performance.md#how-to-measure)), `kinetra_i4sim --blueprint 8640x4800.json --ticks 300000 --seed 42`, 19,400 samples:

| Where | Self | With what it calls |
| --- | --- | --- |
| `kinetra::priority_steer` | 21% | 31% |
| `kinetra::validate` | 11% | 11%: 3.8% from `steer`, 3.6% from `collide()` in `priority_steer`, 3.0% from `priority_steer` itself |
| `MotionStep::surroundings` and `SpatialGrid::query` | 15% | 21% |
| `MotionStep::resolve_contacts` | 10% | 11% |
| `kinetra::steer` | 8% | 12% |

`priority_steer` is the largest. Per neighbor it validated the body, then called `collide()`, which validated both bodies again and rotated their axes, and then searched, for each of the mover's (horizon + 1) steps, the interval of the neighbor's steps that can overlap. Most neighbors touch nothing and meet at no step. Two changes in Kinetra's `priority_steering.cpp`, both with the same results:

- **Contact.** For two axis-aligned boxes, `collide()` computes each axis's overlap as `ha + hb - |d|` and reports penetration only when both are positive. `priority_steer` now computes the same two expressions first and calls `collide()` only when both are positive.
- **Steps.** Before the loop over the mover's steps, a bound per axis keeps only the steps where the mover's predicted position lies within the neighbor's whole predicted path, widened by the extent, the largest slack of `narrow()`, one step of the neighbor and the rounding of the mover's float position. Every step outside it is one the loop skipped, so the first pair found is the same.

The decisions allowed results that shift slightly; these keep them exact, so the golden lines stay as they are.

### Step 3: typing a number over a debug-menu row

- `kinetra::Input` gains `text`, the text typed since the previous poll (SDL's text events, switched on explicitly because SDL3 under sdl2-compat starts with them off), and `Key::backspace`.
- `DebugEntry` gains `set(std::uint64_t)`. `DebugMenu::update` starts a typed number when a digit arrives while the pointer is on a row with `set`, and shows the digits with a cursor in place of the value. Backspace removes one, Enter passes the number to `set`, and Escape, the pointer leaving the row, or `discard_typing()` throws it away; `SettingsOverlay::update_debug` calls the last when it hides the menu. The rows are built anew every frame, so the menu remembers the row's label next to its index and discards the number when another row takes the index. At most seven digits.
- While a number is typed, and on the frame that starts, applies or discards it, `keys_captured()` is true; Lab's sessions then hand their application `kinetra_lab::without_keys(input)`, so a digit does not also toggle an i4sim layer and Escape does not leave a screen.

### Step 4: tick rates and frame rates in Kinetra

- **`TickClock`** (`simulation/clock.h`) takes a rate in ticks per second and keeps its time in ticks, so a million ticks per second loses nothing to a tiny step. `advance(elapsed, limit)` returns the ticks due, at most `limit`, and drops the rest of the backlog but the fraction of the next tick; `rate()` starts from the last tick. `FixedClock` stays for its other users.
- **`TickBudget`** keeps an average cost per tick from `record(ticks, seconds)`, weighted by a quarter so one slow frame moves it a little, and `ticks(seconds)` returns how many fit, at least one. Both are headless: the caller supplies every time, so the tests are deterministic.
- **`Runtime`** gains `vsync(bool)`, which asks the platform to wait for the display or not and returns whether it does, `frame_limit(frames_per_second)` for the limiter (1 to 1000), and `refresh_interval()`, the display's. `frame_interval()` is the display's under VSync and the limiter's otherwise.
- **`debug_rate`** (`view/debug_menu.h`) is a row over a whole-number rate: `[-]` halves, `[+]` doubles, `set` takes a typed number, all clamped to the row's limits.
- **`FrameRateSetting`** (`view/frame_rate.h`) is the FPS row and its state: VSYNC, a limit or UNCAPPED; a click on the label moves to the next, and the buttons and a typed number set the limit. It is headless, because each lab's logic layer builds its menu: the presentation calls `apply(runtime)` once per drawn frame, a template so that the header needs no window. The first call adopts the runtime's pacing, so a session started from the launcher keeps the last one's choice and the capture tool's uncapped frames stay uncapped. `interval()` is the frame's time, 1/60 s when uncapped: the base of a tick budget.

### Step 5: every lab

| | Demo | Dotio | i4sim |
| --- | --- | --- | --- |
| Tick length | 1/120 s | 1/60 s | one tick of millimeter-per-tick motion |
| UPS at real time | 120 | 60 | 200 by default |
| Clock | `TickClock` in `Application` | `TickClock` in `Application` | `TickClock` in `SimulationCore` |
| Limit without a budget | 16 ticks per frame, as before | 8, as before | 256, as before |
| UPS row | `kinetra_lab::ups_entry`, in `DebugPanel` | `kinetra_lab::ups_entry`, in `Application` | `debug_rate` over the simulation's rate, in `FactoryController` |
| FPS row, owner | `DebugPanel` | `Application` | `FactoryController`, kept across blueprints |
| Measured UPS | statistics line, `UPS 236/240` when not at real time | statistics line, as before | HUD, `UPS 9800/1000000` |

- The SPEED row and its time scale are gone; Lab's UPS limits are 1 to 1,000,000 (`kinetra_lab::slowest_ups`, `fastest_ups`), and i4sim's `Simulation::updates_per_second` accepts up to `most_updates_per_second`, the same million. `--ups` follows.
- Each session sets the frame's tick limit from a `TickBudget` over `kinetra_lab::tick_share` (three quarters) of `FrameRateSetting::interval()`, times its application's advance, and records the ticks it ran.
- The settings panel's FRAME LIMIT row and `kinetra_lab::FrameLimitSwitch` are gone: the FPS row decides the pacing, and two controls for one setting would contradict each other.
- The capture tool follows the moved rows (`SettingsRow`, the menu clicks by number) and steps UPS where it stepped SPEED. Every capture is refreshed, because every one shows a changed panel, menu or HUD, and `Stage` changed.

## Steps

| # | Repository | Branch | Pull request | Content |
| --- | --- | --- | --- | --- |
| 1 | Kinetra-Lab | `ccr-74d1b15a-ig41um` | | Benchmark and `--ticks` report ticks per second; a million ticks by default |
| 2 | Kinetra, then Kinetra-Lab | Kinetra `refactor/priority-steer-pairs`; Kinetra-Lab `feat/i4sim-long-run-hotspot` | | Profile a long run and optimize the largest hotspot, `priority_steer`; Lab records the pointer and the measurement |
| 3 | Kinetra | `feat/typed-number-rows` | | Digit input in `Input`, typed number rows in the debug menu |
| 4 | Kinetra | `feat/tick-rates` | | `TickClock` and `TickBudget`, VSync and a frame limit in `Runtime`, `debug_rate` and `FrameRateSetting`; the refreshed shared-UI GIFs |
| 5 | Kinetra-Lab | `feat/tick-rates` | | Every lab uses them; SPEED becomes UPS; limits 1 to 1,000,000; FRAME LIMIT becomes the FPS row; captures refreshed |

## Affected areas

Step 1: `labs/i4sim/tests/motion_benchmark.cpp`, `labs/i4sim/src/main.cpp`, `labs/i4sim/README.md`, `labs/i4sim/tests/README.md`, `docs/performance.md`.

Step 2: Kinetra `src/simulation/priority_steering.cpp`, `tests/weighted_navigation_tests.cpp`, `tests/README.md`; Lab `external/Kinetra`, `docs/performance.md`.

Step 3: Kinetra `include/kinetra/view/input.h`, `view/debug_menu.h`, `runtime/debug_view.h`, `runtime/settings_view.h`, `src/runtime/runtime.cpp`, `src/runtime/settings_view.cpp`, their tests and `docs/engine/runtime.md`, `docs/engine/launcher-overlays.md`. Public API: `Input::text`, `Key::backspace`, `DebugEntry::set`, `DebugMenu::keys_captured`, `typing`, `discard_typing`, `max_typed_digits`.

Step 4: Kinetra `include/kinetra/simulation/clock.h`, `view/debug_menu.h`, the new `view/frame_rate.h`, `runtime/engine.h`, `src/runtime/runtime.cpp`, tests, the engine pages and module READMEs, `docs/media/` and `README.md`. Public API: `TickClock`, `TickBudget`, `debug_rate`, `FrameRateMode`, `FrameRateSetting`, `Runtime::vsync`, `frame_limit`, `refresh_interval`.

Step 5: Lab `labs/shared` (`debug.h`, `input.h`, `settings.h`), Demo, Dotio and i4sim logic, presentation and tests, `launcher/tests/runtime_tests.cpp`, the capture tool, every module README that names SPEED, FRAME LIMIT or the 10000 limit, `docs/launcher-overlays.md`, `docs/architecture.md`, `docs/media/`. Public behavior: `kinetra_lab::settings_rows` loses its `frame_limit` parameter; `--ups` accepts up to 1,000,000.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Step 1: the benchmark accepts `--ticks` up to 100,000,000 and rejects 0 and more | Run by hand with `--ticks 0`, `--ticks 100000001` (rejected) and `--ticks 100000000 --blueprint <MAS> --seed 42` stopped after its header; the CTest benchmark cases still pass | Targeted |
| Step 1: the CSV has `ticks_per_second`, and its other columns are unchanged | Run with `--ticks 50000` on the default set and compare with a run of the previous build | Targeted |
| Step 1: `--ticks` reports the rate | `kinetra_i4sim_inspector_trace` (it matches the summary), and a run by hand | Targeted |
| Step 2: the predictive term is unchanged | Kinetra's existing comparison with a reference that tests every pair of steps, at six scales; a new constructed case, *keeps a mover step that only rounding puts inside the extent*, which fails without the bound's slack | Full (engine) |
| Step 2: the collision term is unchanged | New Kinetra case: the term equals one built from `collide()` for every neighbor, in random scenes with many boxes exactly edge to edge | Full (engine) |
| Step 2: whole runs are unchanged | i4sim's golden, pressure and simulation tests; Demo's characterization tests; the benchmark's CSV columns before and after | Targeted, then Full in CI |
| Step 3: digits typed over a row apply on Enter, Backspace edits, Escape, leaving the row and hiding discard; keys are captured meanwhile; a row is known by its label; seven digits | Kinetra `kinetra_launcher` cases on typed numbers, `kinetra_settings_view:a debug menu that the debug mode hides discards a number being typed` | Full (engine) |
| Step 3: SDL text and Backspace reach `Input` | `kinetra_engine_graphics:input drawing and resizing reach SDL and OpenGL` | Full (graphics) |
| Step 4: `TickClock` runs its rate whatever the frame rate, up to a million, drops what the limit cuts off, restarts on a new rate; `TickBudget` divides time by the measured cost | `kinetra_behavior` tick clock and budget cases | Full (engine) |
| Step 4: `debug_rate` and the FPS row; `apply` adopts the runtime's pacing and passes changes on | `kinetra_launcher` rate and FPS cases (with a stand-in runtime), `kinetra_engine_graphics:the FPS setting starts from the runtime's pacing and passes a change on` | Full (engine, graphics) |
| Step 5: UPS and FPS rows in every lab, at their place in SIMULATION; UPS changes only the wall-clock rate; the tick limit caps a frame; a UPS of zero is rejected | Demo `application_tests` and `debug_panel_tests`, Dotio `dotio_tests`, i4sim `controller_tests`, shared `shared_tests` | Targeted |
| Step 5: the FPS row reaches the runtime through a real session, and the next session keeps it | `kinetra_graphics:the FPS row follows the window and switches its pacing` | Full (graphics) |
| Step 5: a million UPS keeps the window running | `kinetra_i4sim --blueprint 8640x4800.json --run --ups 1000000 --frames 120` offscreen, by hand | Targeted |
| Step 5: captures follow the moved rows | `kinetra_capture_tests`, `kinetra_capture_smoke`, every refreshed capture looked at | Full |

## Verification

Step 1, on 2026-11-27:

- `cmake --build --preset dev --target kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim kinetra_i4sim_motion_benchmark` and `ctest --preset dev -R '^kinetra_i4sim' -j2`: 165 of 165 passed, among them `kinetra_i4sim_inspector_trace`, which now also matches the rate in the summary, and the three benchmark cases.
- `release` preset: `kinetra_i4sim_motion_benchmark --ticks 0`, `--ticks 100000001` and `--ticks 1x` print *Ticks must be in 1..100000000*; `--ticks 100000000` runs. The default set at `--ticks 50000` gives the same first thirteen columns as the build before the change; only `ticks_per_second` is new.
- The default million-tick set (`base1m`): `MAS-demonstrator` completes 202 products with seed 42 but 29 with seed 915839214, which ends with a stuck mover: a stall that 50,000 ticks never showed, now on the roadmap.
- `kinetra_i4sim --blueprint labs/i4sim/blueprints/3840x2400.json --ticks 1000000 --seed 42` prints `i4sim: 1000000 ticks, 1034 completed products, 77502 ticks/s`, in 12.9 s.
- `clang-format --dry-run --Werror` on both changed sources; `check_docs.py`; `check_copies.py` with `50000`, `1000000`, `elapsed_ms` and `completed products` (every remaining hit names a recorded 50,000-tick run or is unrelated).

Step 2, on 2026-11-27:

- Kinetra `cmake --preset dev`, `cmake --build --preset dev`, `ctest --preset dev`: 313 of 313 passed. The new constructed case fails with the bound's slack removed (checked, then restored).
- Lab `ctest --preset dev -R '^kinetra_i4sim|^kinetra_demo(:|_passing|_crowd_characterization)'`: 210 of 210 passed, golden lines unchanged.
- `release` preset, `kinetra_i4sim_motion_benchmark --ticks 50000` and with `--predictive-passing`, before and after, three runs each, interleaved: medians 12.2 s to 10.0 s and 20.1 s to 17.1 s; the CSV columns other than `elapsed_ms` and `ticks_per_second` equal in every run. The default million-tick set after: the same results as before, `8640x4800` in 102 s instead of 148 s and 138 s.

## Review

## Pull request

## Open points
