# i4sim stall detection

Status: implemented · Branch: `feat/i4sim-stall-detection` · Related: [Lab roadmap](../roadmap.md), [i4sim passing under pressure](2026-11-21-i4sim-passing-under-pressure.md#step-4-why-two-movers-stand-still-near-a-destination)

Record why movers stop making route progress and measure whether predictive passing affects the number of stuck movers. This is the first step of stall detection and remedies, which informs adoption of shared passing in Demo and Dotio.

## Table of contents

- [Problem](#problem)
- [Goals](#goals)
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

The HUD's existing stall flag reports lack of route progress. It gives no cause or record of the affected mover. Phase 7 found 11 distinct stuck movers with passing and 6 without on the large floor for seed 42 under Reciprocal. A larger paired sample is needed to judge whether that difference persists.

The phase 7 replay on `4320x2400`, seed 2026, also left an open question about mover 25. At tick 7730, product 20 reached the front of its producer queue. [Production::approach](../../labs/i4sim/src/simulation/production.cpp) already had a mover, so it set the processing goal directly. [The route planner](../../labs/i4sim/src/simulation/routing.cpp) was called without the departure flag and returned the grid route. Its first leg toward (3480, 2280) swept through the clearance-expanded footprint of parked mover 26. Repeating that exact request against the same fleet with the departure flag returned an empty route. The stricter check is used when allocating a free mover to a product.

## Goals

- Report long enough lack of route progress with observed evidence of a stationary blocker, an opposing standoff or oscillation. Show the onset tick and any implicated neighbor in the inspector.
- Record episode transitions in the existing opt-in log. Keep traffic, routing, steering, and factory scheduling behavior identical.
- Compare passing on and off over more paired seeds using the existing, separate flow metrics.

## Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| Order | Start with i4sim detection and measurement | The user chose this prerequisite before integrating Demo and Dotio. |
| Trigger | Reuse the 120-tick route-progress threshold. Exclude held movers, inactive routes and deliberate coordination. | This uses the simulation's existing progress counter and avoids diagnosing a deliberate wait. |
| Diagnostic evidence | Record the first evidenced cause, or unknown. Keep the diagnosis for the episode. | A local sweep or repeated reversal is evidence at detection time. It does not prove that a mover has no possible departure route. |
| Remedies | Reserve MAKE WAY, temporary motion fallback and changes to departure screening for follow-up. | The replay and measurements inform those decisions. |

## Design

Observe the completed tick after motion, production and shuttle updates. Reset the episode when the route progress counter restarts, the destination changes or the mover becomes ineligible. Compare successive excursions of at least two millimeters. A change of more than 120 degrees counts as a reversal, even if the mover started out along another axis. At the stall threshold, examine the forward route leg within steering reach. A stationary mover on that leg supplies a blocker. Two active movers whose swept route legs are blocked by each other count as a standoff when their headings oppose and each moves at most 0.05 mm/tick along its leg. Two meaningful reversals supply evidence of oscillation. When none applies, report unknown.

The detector owns its observation state. The inspector reads the latest completed tick and the opt-in event log receives episode transitions. Events include the onset information even if logging began after the episode started. The existing route flag and benchmark criteria continue to use their respective route and straight-line thresholds.

## Affected areas

i4sim simulation, inspector snapshots and motion benchmark. The engine submodule is pinned at the Lab base revision.

## Test plan

- Verify that a head-on fixture reports both participants, a parked blocker identifies the parked mover, and oscillation requires two meaningful reversals.
- Verify start and end events, inspector fields, suppression for deliberate holds and traffic, and a fresh episode when the goal changes.
- Verify oscillation after an initial perpendicular drift and reject same-stem blueprints and repeated seeds before writing logs.
- Parse the benchmark summary with a CSV reader to verify a blueprint stem containing a comma and quotes.
- Run the i4sim test, golden, pressure and controller suites and the headless inspector trace; compare the existing golden and pressure results to the base.
- Run paired benchmark seeds and report the distinct stuck movers and the number still stuck at the end. Run the documentation and whitespace checks.

## Verification

Local checks on Lab base `87f7a1a` with Kinetra `2f85f15`:

- `cmake --build --preset dev --target kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim kinetra_i4sim_motion_benchmark -j 3` with the runtime enabled and developer warnings.
- `ctest --preset dev -R '^kinetra_i4sim' -j 3 --output-on-failure` passed all 151 cases. The pressure and golden lines retain their recorded values.
- `python3 external/Kinetra/tools/check_docs.py`, `check_copies.py`, `clang-format --dry-run --Werror` on changed C++ files, and `git diff --check`.

The [software OpenGL capture](assets/2026-11-26-i4sim-stall-inspector.png) uses the i4sim session in the corridor fixture with corridor traffic disabled. The west mover's inspector identifies the east mover and shows tick 576 as the onset. The headless inspector also reports those fields.

The benchmark round-trips a stem with a comma and quotes through a CSV reader. The same-stem check returned an error before emitting summary rows and preserved an existing log in the output directory.

### Paired measurement

Release build on `8640x4800`, Reciprocal, 50,000 ticks, eight seeds, with other traffic policies and recovery off. The first two seeds repeat phase 7's large-floor comparison. Every cell is **passing off → on**.

| Seed | Stuck during run | Stuck at end | Products | Waiting ticks |
| --- | ---: | ---: | ---: | ---: |
| 42 | 6 → 11 | 0 → 1 | 95 → 100 | 48,187 → 41,273 |
| 915839214 | 15 → 11 | 1 → 0 | 93 → 104 | 64,702 → 33,500 |
| 7 | 7 → 3 | 1 → 0 | 98 → 100 | 39,559 → 26,483 |
| 1234 | 14 → 7 | 0 → 0 | 94 → 100 | 60,118 → 29,604 |
| 99 | 7 → 7 | 0 → 1 | 100 → 101 | 49,146 → 30,646 |
| 2026 | 4 → 6 | 0 → 1 | 101 → 108 | 41,195 → 29,263 |
| 314159 | 10 → 9 | 0 → 1 | 102 → 97 | 34,841 → 44,412 |
| 8675309 | 5 → 7 | 1 → 1 | 101 → 101 | 37,267 → 36,290 |
| Sum of runs | 68 → 61 | 3 → 5 | 784 → 811 | 375,015 → 271,471 |

The increase seen at seed 42 recurs in two other seeds. Four seeds have fewer distinct stuck movers with passing and one is tied. Five movers remain stuck at the end with passing and three without, across the eight runs in each mode. There were no contacts in either mode. The sample covers one floor and one steering profile, and does not settle whether passing affects long-lived stalls across other layouts.

```bash
for mode in no-predictive-passing predictive-passing; do
  ./build-release/kinetra_i4sim_motion_benchmark --profile reciprocal --ticks 50000 \
    --blueprint labs/i4sim/blueprints/8640x4800.json \
    --seed 42 --seed 915839214 --seed 7 --seed 1234 --seed 99 --seed 2026 \
    --seed 314159 --seed 8675309 "--$mode" --events "/tmp/i4sim-$mode" > "/tmp/i4sim-$mode.csv"
done
```

The `4320x2400` seed-2026 replay also reports `stationary_blocker` on mover 25 at tick 8569. By then mover 25 had tried to go west and its next leg was blocked by mover 22. The departure check replay above concerns its earlier eastbound leg and mover 26.

## Review

The head-on test showed that Reciprocal can move each body sideways while making almost no forward progress. The classifier now checks velocity projected onto both route legs. A route reset can also occur between completed ticks while preserving the destination. Tests cover a counter reset as well as a changed goal, which prevents stale episodes from being carried across either boundary.

[Round 1](https://github.com/SMB-M87/Kinetra-Lab/pull/135#pullrequestreview-6081888053) found three gaps. The detector now compares successive excursions and the regression starts along one axis before oscillating on another. The benchmark rejects duplicate event targets before running and quotes CSV punctuation in the blueprint name. The design includes the actual inspector capture. The module guide and architecture now describe the new events and their position in the tick.

## Pull request

Title `feat(i4sim): diagnose mover stalls without changing motion`.

**Summary.** Step 1 of Stall detection and remedies in i4sim. The inspector and event log report stall episodes and the evidence available when they start. The current traffic and motion paths do not consume the diagnostics.

**Changes.** Observe the existing route progress counter after each tick. Report stationary blockers, opposing movers, oscillation or unknown. Accept repeatable benchmark seeds and blueprints, reject duplicate event targets and record the diagnosis in event CSVs.

**Design.** [i4sim stall detection](2026-11-26-i4sim-stall-detection.md).

**Verification.** All 151 i4sim checks passed, including the original golden and pressure lines, the inspector trace and regressions for axis changes and duplicate event targets. Eight 50,000-tick pairs reproduced phase 7's results for its two seeds and measured six additional seeds. Documentation links, format and whitespace passed.

**Review notes.** Controlled holds end observations. Same-destination route resets begin a new observation. The diagnosis refers to onset. The review fixes also cover axis changes and duplicate filenames. [Inspector capture](assets/2026-11-26-i4sim-stall-inspector.png)

**Dependencies and merge order.** Lab only, based on `main`. This merges before any remedy branch.

**Follow-up.** Decide whether dispatch should apply the strict departure check to a mover that already has a product. MAKE WAY and temporary fallback follow that decision.

## Open points

- Decide how queue advancement should handle an obstructed departure before implementing MAKE WAY.
- Use the detection measurements to choose any temporary motion fallback and decide which recovery behavior belongs in shared passing.
