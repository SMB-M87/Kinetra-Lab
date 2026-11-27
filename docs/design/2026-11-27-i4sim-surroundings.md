# i4sim surroundings without allocations or a scan of every border

Status: implemented · Branch: `refactor/i4sim-surroundings` · Related: [roadmap: i4sim toward a million ticks per second](../roadmap.md#next), [tick rates](2026-11-27-tick-rates.md#step-2-the-hotspot-of-a-long-run), [performance](../performance.md#hotspots)

The next hotspot toward the user's goal of a million ticks per second (asked for on 2026-11-27, "pak daarna de volgende hotspot richting 1M ticks/s op"). After hotspot 7, `MotionStep::surroundings` is the largest part of a long i4sim run. It now reuses its lists from mover to mover and finds nearby borders through an index, with the same results.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Verification](#verification)
- [Review](#review)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

Profiled on 2026-11-27 on top of hotspot 7 and Kinetra `main` of that day ([how](../performance.md#how-to-measure)): `kinetra_i4sim --blueprint 8640x4800.json --ticks 300000 --seed 42`, 10,000 ticks per second.

| Where | Self | With what it calls |
| --- | --- | --- |
| `MotionStep::surroundings` | 6.8% | 27%, of which `SpatialGrid::query` 16% |
| `kinetra::steer` | 11% | 16% |
| `MotionStep::resolve_contacts` | 11% | 13% |
| `kinetra::priority_steer` | 9.4% | 12% (31% before hotspot 7) |
| `Production::update` | 0.4% | 11% |

For every moving mover in every tick, `surroundings` built a new `Surroundings` (three vectors) and took the result of `SpatialGrid::query` as a new vector, and it measured the distance from the mover to every border of the floor (`labs/i4sim/src/simulation/motion_step.cpp`, the loop over `goals.floor.borders()`). Allocation and freeing were 6 to 7% of the run.

## Goals and non-goals

Goals: a shorter long run, with every result unchanged (golden lines, the benchmark's columns but the times).

Non-goals: the other hotspots of the table, which stay on the roadmap item; a different neighbor search for the movers.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Exact or approximate | Exact | The user allowed small shifts for these optimizations (tick rates, 2026-11-27), but this change needs none |
| Border lookup | A `SpatialGrid` over the borders' bounding boxes, built once per floor | A border within the search distance has its bounding box within it too, and a query returns ascending indices, the order of the loop it replaces |
| Where the lists live | Members of `MotionStep`, refilled per mover | The lists are used only until the next mover's; members keep their capacity across ticks |

## Design

- `FloorPlan` builds `border_index()` from the bounding box of every border when it is made, with the floor's cell size.
- `MotionStep::surroundings` fills `MotionStep::around_` and queries into `ids_`, both kept across movers and ticks. Movers come from the existing query, now into the buffer. Borders come from a query of `border_index()` over the mover's position widened by the search distance, followed by the same distance test, in ascending order.
- Why it is exact: the neighbor query is the same call, writing into a buffer; a border whose distance is at most `limit` has a bounding box that touches the square of half side `limit` around the mover (touching counts as overlap), so the index keeps every border the distance test accepts, in the same order.

## Affected areas

`labs/i4sim/include/i4sim/simulation/detail/floor_plan.h`, `labs/i4sim/src/simulation/floor_plan.cpp`, `labs/i4sim/include/i4sim/simulation/detail/motion_step.h`, `labs/i4sim/src/simulation/motion_step.cpp`, `docs/performance.md`, `docs/roadmap.md`.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Whole runs unchanged | i4sim's golden, pressure, simulation and controller tests (`^kinetra_i4sim`) | Targeted |
| The benchmark's results unchanged | `kinetra_i4sim_motion_benchmark --ticks 50000`, default and with `--predictive-passing`, before and after: every column but `elapsed_ms` and `ticks_per_second` equal | Targeted |
| Shorter run | The same runs, three each, interleaved; the median | Targeted |

## Verification

## Review

## Pull request

## Open points
