# i4sim stall detection

Status: accepted · Branch: `feat/i4sim-stall-detection` · Related: [Lab roadmap](../roadmap.md), [i4sim passing under pressure](2026-11-21-i4sim-passing-under-pressure.md#step-4-why-two-movers-stand-still-near-a-destination)

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

Observe the completed tick after motion, production and shuttle updates. Per mover, reset the episode when route progress returns or the mover becomes ineligible. Within a no-progress interval, track direction reversals with a two-millimeter deadband to ignore small position adjustments. At the stall threshold, examine the forward route leg within steering reach. A stationary mover on that leg supplies a blocker. Two active movers whose swept route legs are blocked by each other count as a standoff when their headings oppose and each moves at most 0.05 mm/tick along its leg. Two meaningful reversals supply evidence of oscillation. When none applies, report unknown.

The detector owns its observation state. The inspector reads the latest completed tick and the opt-in event log receives one start and end per observed episode. The existing HUD and benchmark criteria continue to use their respective route and straight-line thresholds. Begin with a paired Reciprocal sample on `8640x4800` at 50,000 ticks so the phase 7 comparison remains directly comparable.

## Affected areas

i4sim simulation, inspector snapshots and motion benchmark. The engine submodule is pinned at the Lab base revision.

## Test plan

- Verify that a head-on fixture reports both participants, a parked blocker identifies the parked mover, and oscillation requires two meaningful reversals.
- Verify start and end events, inspector fields, suppression for deliberate holds and traffic, and a fresh episode when the goal changes.
- Run the i4sim test, golden, pressure and controller suites and the headless inspector trace; compare the existing golden and pressure results to the base.
- Run paired benchmark seeds and report the distinct stuck movers and the number still stuck at the end. Run the documentation and whitespace checks.

## Verification

Pending.

## Review

Pending.

## Pull request

Title `feat(i4sim): diagnose mover stalls without changing motion`.

## Open points

- Decide how queue advancement should handle an obstructed departure before implementing MAKE WAY.
- Use the detection measurements to choose any temporary motion fallback and decide which recovery behavior belongs in shared passing.
