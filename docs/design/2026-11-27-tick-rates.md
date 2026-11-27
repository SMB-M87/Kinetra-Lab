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

### Steps 2 to 5

Written in detail when each step starts; the decisions above fix their shape.

## Steps

| # | Repository | Branch | Pull request | Content |
| --- | --- | --- | --- | --- |
| 1 | Kinetra-Lab | `ccr-74d1b15a-ig41um` | | Benchmark and `--ticks` report ticks per second; a million ticks by default |
| 2 | Kinetra-Lab or Kinetra | | | Profile a long run and optimize the largest hotspot |
| 3 | Kinetra | | | Digit input in `Input`, typed number rows in the debug menu |
| 4 | Kinetra | | | Budgeted fixed clock, FPS limit in `Runtime`, UPS and FPS rows |
| 5 | Kinetra-Lab | | | Every lab uses them; SPEED becomes UPS; limits 1 to 1,000,000 |

## Affected areas

Step 1: `labs/i4sim/tests/motion_benchmark.cpp`, `labs/i4sim/src/main.cpp`, `labs/i4sim/README.md`, `labs/i4sim/tests/README.md`, `docs/performance.md`.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Step 1: the benchmark accepts `--ticks` up to 100,000,000 and rejects 0 and more | Run by hand with `--ticks 0`, `--ticks 100000001` (rejected) and `--ticks 100000000 --blueprint <MAS> --seed 42` stopped after its header; the CTest benchmark cases still pass | Targeted |
| Step 1: the CSV has `ticks_per_second`, and its other columns are unchanged | Run with `--ticks 50000` on the default set and compare with a run of the previous build | Targeted |
| Step 1: `--ticks` reports the rate | `kinetra_lab:inspector_trace` cases (they match the summary), and a run by hand | Targeted |

## Verification

Step 1, on 2026-11-27:

- `cmake --build --preset dev --target kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim kinetra_i4sim_motion_benchmark` and `ctest --preset dev -R '^kinetra_i4sim' -j2`: 165 of 165 passed, among them `kinetra_i4sim_inspector_trace`, which now also matches the rate in the summary, and the three benchmark cases.
- `release` preset: `kinetra_i4sim_motion_benchmark --ticks 0`, `--ticks 100000001` and `--ticks 1x` print *Ticks must be in 1..100000000*; `--ticks 100000000` runs. The default set at `--ticks 50000` gives the same first thirteen columns as the build before the change; only `ticks_per_second` is new.
- `kinetra_i4sim --blueprint labs/i4sim/blueprints/3840x2400.json --ticks 1000000 --seed 42` prints `i4sim: 1000000 ticks, 1034 completed products, 77502 ticks/s`, in 12.9 s.
- `clang-format --dry-run --Werror` on both changed sources; `check_docs.py`; `check_copies.py` with `50000`, `1000000`, `elapsed_ms` and `completed products` (every remaining hit names a recorded 50,000-tick run or is unrelated).

## Review

## Pull request

## Open points
