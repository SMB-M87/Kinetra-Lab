# i4sim make way

**Status** Implemented · **Branch** `feat/i4sim-make-way`

Let a parked mover clear the route of a stalled mover when a checked refuge can open the route. This is step 2 of stall detection and remedies. [Lab roadmap](../roadmap.md#later) · [Stall detection](2026-11-26-i4sim-stall-detection.md)

## Table of contents

- [Problem](#problem)
- [Design](#design)
- [Test plan](#test-plan)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

A mover at the front of a producer queue can get boxed into its own parking row when it follows a route past an already parked neighbor. The Reciprocal replays on `4320x2400` and `MAS-demonstrator` show the same mechanism. Route recovery has no path to follow while the blocking footprint stays in place. The stall detector from [PR #135](https://github.com/SMB-M87/Kinetra-Lab/pull/135) records the episode, which provides the starting point for the remedy. [Phase 7 replays](2026-11-21-i4sim-passing-under-pressure.md#step-4-why-two-movers-stand-still-near-a-destination)

## Design

Enable the policy through DEBUG → TRAFFIC → MAKE WAY, `scenario.motion.makeWay` or the benchmark's `--make-way` flag. It starts disabled. It uses the merged observer's active episode as a trigger and checks current route geometry before taking action. The observer's onset diagnosis can be different from the obstacle currently in the way.

For each episode, first try a checked alternate route. Otherwise look for one nearby parked mover whose temporary departure opens a route while its footprint is in the refuge. Reuse the existing footprint-aware route refinement to check the stalled mover's route and each proposed trip. Refuges stay clear of parking and processing reservations and the checked paths avoid enabled traffic zones and their reserved exits. The search covers two cells on each axis around the parked blocker and picks the nearest valid refuge in stable order.

An idle mover at its own parking place can yield while holding a product. Production defers a new movement while the mover is away and retains its product, recipe step and queue reservation. Exclude a held mover, a mover under other traffic control, a shuttle participant and a mover processing a product. The maneuver uses existing steering and contact rules.

The request ends when the beneficiary clears, is held, changes goals, the policy is disabled or the 1,200-tick deadline passes. If the return route is clear, the blocker can turn back immediately. Otherwise it uses the checked outbound leg or stops on encountering a new stationary obstruction and waits for a checked return. The event log records attempted relief, an unavailable path, and the request's outcome. A failed geometry search may retry once after 30 ticks, so a nearby mover can finish parking. Only one trip starts per episode.

## Test plan

- A parked blocker moves to a checked refuge and returns to its reservation. The stalled mover reaches its original goal and both remain contact-free.
- A narrow floor with no refuge retains its stall and records the unavailable attempt. Disabling the feature during an outbound trip returns the blocker safely.
- Check both recorded `4320x2400` seeds and the `MAS-demonstrator` product-bearing blocker. The mover retains its product while yielding, and the original trip resumes.
- Run the i4sim controller, simulation, golden and pressure suites. Compare paired runs across Improved and Reciprocal on the 48-run harness and in the long benchmark.

## Verification

The Release build with project warnings passed all 157 i4sim checks, including the golden and pressure suites, the inspector trace and the no-refuge and cancellation cases. Formatting, documentation links and whitespace checks passed.

| Blueprint and seed | Ticks | Products, off → on | Waiting ticks, off → on | Stuck at end, off → on | Contacts, off → on |
| --- | ---: | ---: | ---: | ---: | ---: |
| `4320x2400`, 2026 | 12,000 | 6 → 6 | 4,383 → 2,132 | 1 → 0 | 0 → 0 |
| `4320x2400`, 915839214 | 12,000 | 7 → 7 | 4,155 → 3,472 | 2 → 0 | 0 → 0 |
| `MAS-demonstrator`, 42 | 50,000 | 10 → 15 | 24,006 → 1,646 | 1 → 0 | 0 → 0 |

These runs used Reciprocal with other traffic policies and RECOVERY off. In the MAS replay, the first search at tick 27,028 finds no route. Thirty ticks later mover 4 can yield while retaining its product. The original transport resumes at tick 27,440.

In the 48-run harness, Reciprocal went from two runs with a stuck mover at the end to zero, with zero contacts throughout. Improved went from zero such runs to one, with 170 → 167 contacts and 3,487 → 3,832 waiting ticks. Both profiles completed the same number of products in aggregate. The remedy stays off by default.

## Pull request

**Title** `feat(i4sim): let parked movers clear stalled routes`

**Summary.** Add optional MAKE WAY to i4sim. One parked mover can yield to a diagnosed stall using a checked refuge and return route, while retaining its production reservations.

**Changes.** Add the traffic setting to the debug menu and scenario loader, mark temporary goals in the shared inspector, and record remedy outcomes in the benchmark event CSV. Reuse the stall detector merged in #135.

**Design.** [i4sim make way](2026-11-27-i4sim-make-way.md).

**Verification.** All 157 i4sim checks passed on Linux. The paired 48-run harness and the 50,000-tick MAS replay cover the effect of enabling the policy. Documentation, formatting and whitespace checks passed.

**Review notes.** The queue-departure check and the separate Improved corner case remain open. The remedy stays opt-in.

**Dependencies and merge order.** Lab only, on top of already-merged PR #135. The Kinetra pointer stays at the base revision.

## Open points

- Decide how to apply the strict departure check when a product already has a mover and reaches the front of the producer queue.
- Measure and choose a temporary motion fallback. The separate RECOVERY corner case remains open.
- A single parked mover can fail to open a checked route. Keep such stalls visible and use the recorded cases to evaluate any future policy that moves several blockers.
