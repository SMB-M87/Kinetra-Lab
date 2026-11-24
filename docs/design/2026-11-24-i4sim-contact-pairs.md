# i4sim contact resolution without every pair

Status: accepted · Branch: `ccr-0b1cf811-ur7qrd` · Related: [performance, hotspot 2](../performance.md#hotspots), roadmap item *i4sim contact resolution without every pair*

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

## Verification

## Pull request

## Open points
