# i4sim contact resolution without every pair

Status: implemented · Branch: `ccr-0b1cf811-ur7qrd` · Related: [performance, hotspot 2](../performance.md#hotspots), roadmap item *i4sim contact resolution without every pair*

`MotionStep::resolve_contacts` was the largest single cost of i4sim's motion benchmark: it computed both movers' bounds again for every pair of movers, every tick. This change computes them once per mover and keeps every other part of the step, so the run stays the same to the bit and the benchmark gets faster. It is hotspot 2 of the [performance baseline](../performance.md); the engine hotspots run in their own stack in Kinetra.

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

`MotionStep::resolve_contacts` (`labs/i4sim/src/simulation/motion_step.cpp:223`) tests every unordered pair of movers with `bounds(integrated[i]).overlaps(bounds(integrated[j]))` before the narrow phase. On `8640x4800`, with 106 movers, that is 5,565 pairs and 11,130 calls of `kinetra::bounds` per tick, where 106 would do; each call of a box projects it twice, with two `sincosf`. The baseline of 2026-11-23 put the function at 46% of the motion benchmark, 32% with predictive passing, and `bounds` alone at 30% ([performance, i4sim](../performance.md#i4sim)).

## Goals and non-goals

- Goal: the motion benchmark is measurably faster, by the method of the [performance page](../performance.md#how-to-measure).
- Goal: the run does not change: the golden and pressure lines, and every column of the motion benchmark except `elapsed_ms`, for the default profile, predictive passing and the other profiles and policies.
- Goal: the performance page shows the new numbers and the hotspot table the remaining work.
- Non-goal: `kinetra::bounds` itself (hotspot 5) and `SpatialGrid` (hotspot 3); both are Kinetra's, in the engine stack.
- Non-goal: any change to which pairs touch, the order they are resolved in, or the contact counters.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Where the time goes | Compute each mover's bounds once per call, before the pair loop | 11,130 calls of `bounds` per tick become 106; this is most of the hotspot ([measured](#verification)) |
| Broad phase | None: every pair is tested on the ready boxes, as before | The roadmap proposed the tick's `SpatialGrid` as broad phase, and the page left the choice to this pull request. Measured on one machine, a fresh index over the moved movers with one query per mover took 25.7 s against 22.4 s without it (default), and 44.4 s against 39.3 s with passing: at about a hundred movers, a query that hashes its cells, allocates and sorts costs more than 5,565 comparisons of four floats. Both kept every result |
| The tick's index from `MotionStep::run` | Not used | It holds the movers' bounds from before `integrate` moves them, while contacts are tested after it; using it would need a rebuild or a query inflated by the tick's travel, and no index pays off here anyway |
| Where the numbers go | A new section *Measured changes* on the performance page, one row per optimization with before and after from one machine; the i4sim profile and the hotspot table are profiled again | This cloud machine is about 1.45 times slower than the baseline's, so its times do not belong in the baseline tables; the engine stack adds its rows to the same table (agreed with its session) |
| The border loop | Left as it is | After this change the whole function is 9% of the run, about a third of it the distance from every mover to every border; skipping far borders would need an argument about rounding near the threshold for little gain. An open point on the page |

## Design

`resolve_contacts` keeps its structure: it copies the movers' bodies after integration (`integrated`), finds pairs and border contacts on those copies, and resolves each on the live bodies in the order `(i, j)` for `j > i`, then the borders of `i`. The only change is that it also takes `bounds(integrated[i])` once per mover into `boxes`, and the pair loop tests `boxes[i].overlaps(boxes[j])`. `bounds` is a pure function of the body, so each box has the same bits as before, every pair test gives the same answer, and the pairs, their order and the resolution stay the same. The loop bound is the mover count, read once.

## Affected areas

- `labs/i4sim/src/simulation/motion_step.cpp`: `MotionStep::resolve_contacts`.
- `docs/performance.md`: i4sim numbers, the summary and the hotspot table.
- `docs/roadmap.md`: the item moves to *Now*, and is removed when this merges.

## Test plan

The change must not alter behavior, so the tests that pin it are the existing ones:

| Behavior | Test | Level |
| --- | --- | --- |
| Seeded runs stay the same to the bit, contacts included | `kinetra_i4sim_golden_tests` (ten lines with a fingerprint of every mover's position and velocity after every tick) | Targeted |
| Passing under pressure stays contact-free and the same | `kinetra_i4sim_pressure_tests` | Targeted |
| Contact counting and resolution cases | `kinetra_i4sim_tests`, `kinetra_i4sim_controller_tests` | Targeted |
| The large layouts, where the pairs matter, stay the same | Motion benchmark CSV before and after, every column but `elapsed_ms`: default and `--predictive-passing` over 50,000 ticks, `original`, `rvo`, `reciprocal`, `--recovery` and `reciprocal` with the three traffic policies over 20,000 ticks | Targeted, by hand |
| It is faster | Motion benchmark, release preset, before and after on one machine, three runs each, the median | By hand |

## Review

- `/code-review` (low) on `main...HEAD`: no findings.
- Correctness: `bounds` is a pure function of the body, and `Aabb::overlaps` is unchanged, so every pair test has the same answer as before; the pairs are found on `integrated` and `boxes`, which resolving a pair does not touch, so the pairs and their order cannot change. The benchmark CSVs confirm it on the large layout for five profiles and policies.
- Readability: `resolve_contacts` grows from 68 to 77 lines, past the conventions' 60; it was past them before. Splitting off the border loop is a refactor without a performance reason, an open point below.
- Copies: `check_copies.py` with "tick's spatial index", "broad phase", "hotspot 2", "4,000 ticks per second", "goes with 2", "bounds once per mover" and "contact resolution". The hits are the roadmap item this branch removes, the updated performance page, and Kinetra's pages on `World`'s own broad phase, which this change does not touch.
- The roadmap item *Engine hotspots from the performance baseline* still quotes the baseline's 7% for `bounds`; it says what the baseline found, and its session decides on hotspot 5 with the new numbers, which it has.

## Verification

Measured on a cloud machine with four cores of an Intel Xeon at 2.1 GHz, GCC 13.3, release preset, nothing else running. The baseline is Lab `401cdc8` with Kinetra `73adc66`, built in `build-before` before the change.

| Version | Default, three runs | Median | With `--predictive-passing` | Median |
| --- | --- | --- | --- | --- |
| Before | 40.4–41.1 s | 40.6 s | 58.7–60.8 s | 59.0 s |
| Bounds once per mover, this branch | 21.2–21.7 s | 21.6 s | 37.4–38.6 s | 38.2 s |
| First version, which read the mover count in every loop test | 21.7–22.4 s | 22.4 s | 39.3–39.6 s | 39.3 s |
| The first version plus a `SpatialGrid` over the moved movers, one query per mover (not kept) | 25.5–26.7 s | 25.7 s | 43.0–44.7 s | 44.4 s |

- Results: for every version, all eleven CSV files (three default and three passing runs at 50,000 ticks; `--profile original`, `--profile rvo`, `--profile reciprocal`, `--recovery` and `--profile reciprocal --parking-traffic --junction-traffic --corridor-traffic` at 20,000 ticks) are identical to the baseline in every column but `elapsed_ms`.
- Profile: `perf record -F 499 --call-graph fp` on a frame-pointer build, default (10,671 samples) and with passing (18,792 samples), as on the [performance page](../performance.md#how-to-measure).
- Targeted level: `cmake --preset dev`, `cmake --build --preset dev --target kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim` without warnings, `ctest --preset dev -R '^kinetra_i4sim'`: 147 of 147 passed in 104 s.
- `clang-format --dry-run --Werror labs/i4sim/src/simulation/motion_step.cpp`: clean.
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.
- Full level: the pull request's CI.

## Pull request

Title: `perf(i4sim): compute contact bounds once per mover`

**Summary.** `MotionStep::resolve_contacts` computed both movers' bounds again for every pair of movers, every tick: 46% of the i4sim motion benchmark. It now computes them once per mover. Every result stays the same, and the benchmark takes 47% less time, 35% less with predictive passing. Hotspot 2 of the performance baseline.

**Changes.**
- i4sim: `resolve_contacts` takes each mover's bounds once, and the pair loop tests the ready boxes; pairs, order and resolution are unchanged.
- Performance page: a new section *Measured changes* with this change's before and after, the i4sim profile and the hotspot table profiled again, the i4sim summary and an open point on the border loop.
- Roadmap: the item *i4sim contact resolution without every pair* is removed.

**Design.** This document, `docs/design/2026-11-24-i4sim-contact-pairs.md`. The roadmap proposed the tick's `SpatialGrid` as broad phase; measured, a fresh index over the moved movers made it slower (25.7 s against 22.4 s for the same code without it), so every pair is still tested, on boxes computed once.

**Verification.** Motion benchmark before and after on one machine, three runs each: 40.6 → 21.6 s, with passing 59.0 → 38.2 s; all eleven CSVs (five profiles and policies) identical apart from `elapsed_ms`. `ctest --preset dev -R '^kinetra_i4sim'`: 147 of 147 passed; no new warnings; docs check clean.

**Review notes.** The numbers come from a cloud machine about 1.45 times slower than the baseline's, so the page keeps the baseline tables and puts this change's before and after in *Measured changes*; the engine stack adds its rows there. The i4sim ceiling in the summary is scaled from the baseline by the measured ratio, and says so.

**Dependencies and merge order.** None. The engine hotspots run as a separate stack in Kinetra and Lab and edit the same performance page and roadmap; whichever merges second merges `main` in.

**Follow-up.** Hotspot 5 (`bounds` with `sincosf`) is down to about 1% of i4sim; the engine stack decides whether it still gets a pull request. The border loop of `resolve_contacts` is an open point on the performance page.

## Open points

- `resolve_contacts` is 77 lines; splitting off the border loop would bring it under the conventions' 60, as a refactor of its own.
- The border loop is about a third of what is left of the function, 3% of the run; on the [performance page](../performance.md#open-points).
