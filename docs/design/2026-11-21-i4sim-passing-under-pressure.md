# i4sim passing under pressure

Status: accepted · Branches: `test/i4sim-passing-causes` (step 1), `fix/i4sim-guarded-passing` (steps 2 and 3), `fix/i4sim-destination-standstill` (step 4) · Related: [quality roadmap, phase 7](2026-11-18-quality-roadmap.md#phase-7-predictive-passing-under-pressure), [i4sim motion changes keep routes](2026-11-20-i4sim-tuning-stall.md), [i4sim restructuring](2026-11-19-i4sim-restructuring.md), [i4sim tests](../../labs/i4sim/tests/README.md), [factory simulation architecture](../factory-simulation-architecture.md#predictive-passing)

Roadmap phase 7. Predictive passing in i4sim must stay safe and keep traffic flowing under pressure, with Reciprocal as the reference profile, and the standstills near a destination found on 2026-11-20 must be explained. This document first establishes the causes from recorded runs, then changes behavior in two steps, each on its own branch and pull request, stacked in that order.

The user asked for the phase while away and asked not to wait for answers: every decision the agent took on its own is marked ✱ in [Decisions](#decisions) and listed for review under [Open points](#open-points).

## Table of contents

- [Problem](#problem)
  - [Baseline at the start](#baseline-at-the-start)
  - [Step 1: why Improved has more contacts with passing](#step-1-why-improved-has-more-contacts-with-passing)
  - [Where a plan goes wrong](#where-a-plan-goes-wrong)
  - [Step 4: why two movers stand still near a destination](#step-4-why-two-movers-stand-still-near-a-destination)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
  - [Steps](#steps)
  - [Step 1: diagnostics and a new pressure case](#step-1-diagnostics-and-a-new-pressure-case)
  - [Step 2: guarded maneuvers](#step-2-guarded-maneuvers)
  - [Step 3: measured result](#step-3-measured-result)
  - [Step 4: a way out for a boxed-in mover](#step-4-a-way-out-for-a-boxed-in-mover)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull requests](#pull-requests)
- [Measurement harness](#measurement-harness)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

### Baseline at the start

Measured on 2026-11-21 at `2cd2c8a` (Lab `main`, Kinetra `7d3e586`) with `kinetra_i4sim_motion_benchmark`, Release, 50,000 ticks, recovery and traffic policies off. Each cell is passing **off / on**.

| Profile | Blueprint | Seed | Completed | Contacts | Contact episodes | Waiting ticks | Stuck (at end) | Plans (cancelled) | Contacts in a plan |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Reciprocal | MAS-demonstrator | 42 | 10 / 10 | 0 / 0 | 0 / 0 | 24006 / 25041 | 1 (1) / 1 (1) | 4 (1) | 0 |
| Reciprocal | MAS-demonstrator | 915839214 | 16 / 16 | 0 / 0 | 0 / 0 | 12872 / 10842 | 2 (1) / 1 (1) | 5 (0) | 0 |
| Reciprocal | 3840x2400 | 42 | 59 / 59 | 0 / 0 | 0 / 0 | 1583 / 3095 | 0 / 0 | 11 (5) | 0 |
| Reciprocal | 3840x2400 | 915839214 | 61 / 61 | 0 / 0 | 0 / 0 | 651 / 3177 | 0 / 0 | 7 (3) | 0 |
| Reciprocal | 8640x4800 | 42 | 95 / 99 | 0 / 0 | 0 / 0 | 48187 / 45668 | 6 (0) / 6 (0) | 244 (87) | 0 |
| Reciprocal | 8640x4800 | 915839214 | 93 / 103 | 0 / 0 | 0 / 0 | 64702 / 58203 | 15 (1) / 10 (2) | 219 (74) | 0 |
| Improved | MAS-demonstrator | 42 | 16 / 14 | 53 / 60 | 9 / 10 | 278 / 5452 | 0 / 1 (1) | 3 (2) | 0 |
| Improved | MAS-demonstrator | 915839214 | 16 / 14 | 204 / 212 | 12 / 16 | 288 / 2019 | 0 / 1 (0) | 4 (4) | 0 |
| Improved | 3840x2400 | 42 | 54 / 58 | 1 / 0 | 1 / 0 | 853 / 1573 | 1 (0) / 2 (0) | 7 (4) | 0 |
| Improved | 3840x2400 | 915839214 | 60 / 56 | 18 / 0 | 4 / 0 | 1055 / 1538 | 1 (0) / 2 (0) | 4 (3) | 0 |
| Improved | 8640x4800 | 42 | 98 / 96 | 384 / 767 | 71 / 81 | 2595 / 34343 | 2 (0) / 4 (0) | 242 (96) | 17 |
| Improved | 8640x4800 | 915839214 | 99 / 101 | 726 / 823 | 78 / 71 | 2953 / 33940 | 4 (1) / 6 (0) | 224 (89) | 5 |

The Reciprocal rows and the Improved `8640x4800` rows equal the [phase 4 baseline](../../labs/i4sim/tests/README.md#motion-benchmark). The Improved `MAS-demonstrator` and `3840x2400` rows differ: changes merged since 2026-11-19 (motion changes keep routes, MAS as a test fixture, the Kinetra navigation rebuild) moved them; which one was not bisected. *Contact episodes* is a new column of this phase: contacts of one pair grouped while less than 500 ticks lie between them.

### Step 1: why Improved has more contacts with passing

Replayed from the recorded runs of `8640x4800` (seed and tick from the event log; harness in [Measurement harness](#measurement-harness)). Every contact of the passing-on run was replayed from a copy of the simulation taken at most 300 ticks before it, with passing switched off at that copy:

| Seed | Contacts with passing | Also without passing, from 300 ticks before | Only with passing | Of those: in a plan | Within 400 ticks after a plan ended | Later |
| --- | --- | --- | --- | --- | --- | --- |
| 42 | 767 | 564 | 203 | 17 | 23 | 163 |
| 915839214 | 823 | 769 | 54 | 5 | 6 | 43 |

- **Most contacts do not need passing.** 74% and 93% of the contacts happen just the same when passing is turned off 300 ticks before. Passing changes *which* situations a run reaches, not how often Improved touches in them.
- **The count is driven by a few jostling pairs.** Improved's contacts come in episodes: one pair touches, separates and touches again, for hundreds or thousands of ticks, usually a moving mover beside a parked or processing one in a parking area (442 of 767 and 604 of 823 contacts involve a mover slower than 0.05 mm/tick). The number of episodes barely changes with passing (71 → 81 and 78 → 71); the ten largest episodes hold 489 of 767 and 596 of 823 contacts. With seed 42 the largest single episode is movers 81 and 87 at ticks 18595 to 21243: 191 contacts while 81 leaves its parking place at (2168, 2808) and 87 arrives at the next one, 300 ticks after both left their plans. Without passing the largest episode has 49 contacts (movers 28 and 29, ticks 25937 to 26568).
- So the doubling with seed 42 is mostly one long episode that passing happened to steer the run into, and the same profile without passing has episodes of the same kind. **This is a limit of the Improved profile** (seek-and-avoid steering that keeps re-touching a stationary neighbor), not of passing. Reciprocal has no contact in any of the twelve runs.

### Where a plan goes wrong

The contacts *in* a plan (17 and 5) and the pressure scenarios show what passing itself gets wrong. Every case below was replayed tick by tick from its seed:

1. **The lanes are checked only when the plan forms.** Plan 216 (movers 7 and 25, seed 42) formed at tick 45829 with clear lanes. Mover 9 then parked at (2225, 1780), on the far side of mover 7's exit leg. Mover 7 pressed against it from tick 46432, ten contacts in 120 ticks, until the plan ended at tick 46550 for lack of progress. Movers 81 (plan 91, tick 23390) and 37 (plan 149, tick 34899) touched movers that drove into their lanes after formation. The new pressure case below reproduces this with three movers.
2. **The progress check sums both participants.** A plan only ends after 240 ticks in which the *summed* distance to both targets did not shrink. Plan 216 crossed for 620 ticks: its partner's progress kept it alive while mover 7 stood against mover 9.
3. **A completed plan blocks the next one.** In `pressure-three-oncoming` under Reciprocal, mover 0 completes its pass with mover 1 at tick 558 and is then 480 mm from mover 2, enough room for a new plan (at least 258 mm). But both participants of any ended plan wait `passing_retry_ticks` (120) before they may join a new one; by tick 678 they are 117 mm apart, too close to form a plan, and Reciprocal holds both in front of each other until the end (both creep at 0.052 mm/tick, `stuck=1`).
4. **A participant drives on through a mover that stopped on its target.** In the new case under Reciprocal, mover 0 completes its pass and then pushes the mover standing on its exit target ahead of it through reciprocal avoidance for about 2000 ticks.
5. **The formation check swept the wrong path.** Found while building step 2: `lanes_clear` called `sweep_translation(moving, to - from, other)`, but `sweep_translation` takes the *end position*, not the displacement. Every lane was swept from its start toward a point near the floor's origin, so third movers and traffic zones on the real lanes went unnoticed. In the coordinated MAS golden run a plan formed at tick 19820 with a lane across a parked mover.

### Step 4: why two movers stand still near a destination

The [measurement harness of the tuning-stall work](2026-11-20-i4sim-tuning-stall.md#measurement-harness) (48 runs: four blueprints, six seeds, Improved and Reciprocal, 12,000 ticks, no change) ends with a stuck mover in 2 runs: `4320x2400` under Reciprocal with seeds 915839214 and 2026. Both end in the same place. Replayed with seed 2026:

- The parking places of movers 22 to 27 lie in the bottom row of `4320x2400` (cells 240 mm high, y 2160 to 2400), below the producer cells at (2520, 2040) and (3240, 2040). A parked APM4220 (127 mm) leaves a strip of 108 mm above it, too low for another 127 mm mover, so a mover enters and leaves this row only through the free cells above it, (2760, 2040), (3000, 2040) and (3480, 2040).
- At tick 7800 mover 25 leaves its parking place (3359.5, 2331.5) with a product for (3039.5, 1839.5). It drives up into the strip, cannot pass parked mover 26 to the east, and a replanned route sends it west along the strip. At tick 8400 it stands at (3085, 2223.5): the producer cell's corner (3120, 2160) is 100.5 mm to its right and parked mover 22's corner (3019.5, 2268) 108 mm below it, so its 127 mm body fits neither up nor west. The only way out is back east.
- Its two routes, (2760, 2040) and (2760, 2280), both lead west. Every 100 ticks without progress `MotionStep::follow_route` replans, and `grid_route` refines the grid route around stationary movers (`labs/i4sim/src/simulation/routing.cpp`), but the refinement only adds the corners of obstacles near the grid route to its nodes. Both grid routes lead west, so no refined route leads east; the refinement fails and the mover keeps the obstructed route. It slides back and forth for 1900 ticks, a stuck mover on its own.
- At tick 10340 mover 23 returns to its parking place (3090.5, 2331.5), which mover 25's body overlaps by 19 mm. Reciprocal avoidance stops it at (3214, 2336.5), 123.6 mm from its destination, and now mover 23 also closes the way east. Neither moves again: the *two movers blocking each other about 120 mm from a destination* of the roadmap. Mover 23's route is its destination alone, so it has nothing to replan.
- RECOVERY (route recovery with `kinetra::Navigation` around blocked cells and movers) frees mover 25 in this run, but the 48 runs with RECOVERY on end with 7 stuck runs. Six of them are mover 18 on `4320x2400` under Improved at (800.5, 1757.5): its recovery detour runs straight to (800.5, 1359.5), past the corner of the blocked cell at (600, 1560) with 3 mm to spare, and the border constraint stops it there. That is a separate failure of route recovery, recorded under [Open points](#open-points).

The trap is not caused by Reciprocal: under Improved the same runs take other paths. A blueprint with a narrower aisle or a different seed can lead any profile into it.

## Goals and non-goals

**Goals:**
- Name the causes of Improved's extra contacts and of the standstills from replayed runs, with seed and tick, and keep the tools that found them.
- Passing that rechecks its maneuver while it runs: a mover that enters, crosses or stops in a lane makes the participant wait, and an obstruction that does not clear ends the plan, under one fixed priority.
- The pressure scenarios end without contacts and with every mover arrived, under Reciprocal and Improved, and become required outcomes.
- With passing on, Reciprocal shows no regression in the benchmark against passing off.
- A mover boxed in by stationary movers on its route finds the way out that exists.

**Non-goals:**
- Making Improved contact-free. Its jostling episodes happen without passing; they are a profile limit.
- Turning passing on by default, unless the measurements justify it.
- Changing route recovery (RECOVERY) or its corner failure.
- Moving passing to Kinetra (phase 8).

## Decisions

✱ marks a decision the agent took without the user, recorded for review under [Open points](#open-points).

| Decision | Choice | Why |
| --- | --- | --- |
| Pull requests ✱ | Three, stacked: diagnostics and causes; guarded passing with the measured result; the standstill | Each changes behavior for one reason, so a changed golden or pressure line has one explanation. The standstill happens without passing, but stacking avoids conflicts in the same files |
| Improved's extra contacts ✱ | Documented as a profile limit; no change to Improved steering | 74–93% of them recur without passing, and their count follows a few long jostling episodes that also occur without passing |
| Contact episodes ✱ | A new benchmark column, contacts of one pair grouped while less than 500 ticks lie between them | Contact starts overstate how often Improved meets a new conflict; episodes show it |
| Event log export ✱ | `kinetra_i4sim_motion_benchmark --events <directory>` writes each run's events as CSV | A contact or plan can be replayed from blueprint, seed and tick without a debugger |
| Retry after a completed plan ✱ | None; the 120-tick retry applies only after a plan that ended without completing | The retry protects against thrashing after a failure. After a completed pass it only delays the next pass until the movers are too close for one (cause 3) |
| Who yields ✱ | A participant yields to every mover outside its plan and to participants of older plans (lower id); it never yields to a younger plan | One fixed order, so two plans cannot wait for each other and a decision does not flip back and forth |
| How a participant yields ✱ | It stops at its braking point and waits until the lane ahead has been clear for 20 ticks; a mover that stands still on a remaining leg, or a wait of 240 ticks, ends the plan as `obstructed` | Waiting suits a mover that passes by; a stationary one will not clear. The braking point keeps the stop within the acceleration limit. 240 ticks equals the existing no-progress limit |
| Progress check ✱ | Unchanged: the summed check stays; a per-participant check was built and removed again | The guard ends the case of cause 2 (a stationary mover on a leg) at once; the extra check changed no measured run and had no case of its own to test |
| Formation sweep | `sweep_translation(moving, to, other)`, the end position | Cause 5, a plain bug; the coordinated golden line changes with it (totals equal) |
| Movers behind a participant ✱ | Ignored by the guard | In the first measurement a third of the 240-tick waits that ended a plan were for a mover following the participant, which can never clear its lane |
| Oncoming mover in the lane ✱ | Ends the plan at once as `obstructed` instead of waiting | The other large group of expired waits: a mover that drives toward the participant does not leave its lane while the participant waits for it |
| New end reason ✱ | `PassingEnd::obstructed`, after `traffic_control` and before `no_progress` in the order of checks | The event log must say why a plan was abandoned; `no_progress` would hide the guard |
| Passing default ✱ | Stays off | Reciprocal now gains in every run, but Improved, the default profile, stays mixed: fewer contacts on the large floor, more on MAS seed 42, and 4–5 times the waiting of passing off ([step 3](#step-3-measured-result)). A default that helps one profile and not the other is the user's call |
| Boxed-in mover ✱ | When the stall replanning finds the route still obstructed by stationary movers, plan a `Navigation` route around the bodies, as route recovery does, even with RECOVERY off | The existing refinement cannot leave the grid route's topology; Navigation plans in free space and finds the way back east |

## Design

### Steps

| Step | Branch | Behavior change |
| --- | --- | --- |
| 1 Causes | `test/i4sim-passing-causes` | None: diagnostics, a new pressure case that records today's outcome, this document |
| 2–3 Guarded passing, measured | `fix/i4sim-guarded-passing` | Passing only: retry, guard, progress per participant; pressure lines become required outcomes; benchmark before and after |
| 4 Standstill | `fix/i4sim-destination-standstill` | Stall replanning of a boxed-in mover |

### Step 1: diagnostics and a new pressure case

- `test::contact_episodes` in `event_counts.h` groups contact starts per pair (or mover and border) while less than `contact_episode_gap_ticks` (500) lie between two of them. The benchmark prints it as `contact_episodes`.
- `--events <directory>` makes the benchmark write `<blueprint>-<seed>.csv` per run: tick, kind, plan, both movers, end reason, border and passing flags.
- A fifth pressure scenario on the `pressure-lane-intrusion` fixture: movers 0 and 1 pass head-on, and mover 2 drives to (1330, 636), mover 0's exit target, and stops there after the plan formed. The case first checks that mover 2 stands within 130 mm of an active passing target, then compares its recorded line. Today: under Reciprocal mover 2 is stuck for more than 2000 ticks (cause 4); under Improved the plan ends for lack of progress.

### Step 2: guarded maneuvers

Three changes, one commit each, in [predictive_passing.cpp](../../labs/i4sim/src/simulation/predictive_passing.cpp):

1. **Formation sweep.** `lanes_clear` sweeps each leg to its end position (cause 5).
2. **Retry.** `restore_routes` sets the 120-tick retry only after a plan that did not complete (cause 3).
3. **Guard.** `advance_plans` calls `guard` for each plan after the end reasons that come from the participants (disabled, stopped, held, traffic control) and before the progress check:

```mermaid
flowchart TD
    A["advance_plans: each tick, each plan"] --> E{"disabled, stopped, held,<br/>traffic control?"}
    E -->|yes| X[end with that reason]
    E -->|no| S{"a stationary mover<br/>on a remaining leg?"}
    S -->|yes| O[end as obstructed]
    S -->|no| H{"a mover with priority ahead,<br/>predicted in the lane ahead?"}
    H -->|"driving toward the participant"| O
    H -->|"otherwise"| W["participant waits at its braking point"]
    H -->|"none for 20 ticks"| G[participant drives on]
    W --> T{"waiting 240 ticks?"}
    T -->|yes| O
    T -->|no| P
    G --> P{"no summed progress<br/>for 240 ticks?"}
    P -->|yes| N[end as no_progress]
    P -->|no| C{"both at exit targets?"}
    C -->|yes| D["end as completed, no retry delay"]
```

*Takeaway:* a plan is checked against the movers around it in every tick, not only when it forms; a passing mover makes a participant wait, a standing or oncoming one ends the plan.

- *Remaining legs:* before crossing, from the participant's position to its entry target and on to its exit target; while crossing, to its exit target. Swept with the footprint plus `clearance_mm`, with `crosses_mover`, so touching does not count; the formation check is stricter (touching counts), so a mover that the formation accepted next to a lane does not end the plan in the next tick.
- *Lane axis:* the direction from the participant's entry to its exit target. Ahead, behind and oncoming are judged along it, because the leg to the entry target runs mostly sideways.
- *Lane ahead:* the first `speed * horizon + braking_distance(speed, force)` millimeters of the remaining legs (71 mm at the defaults). Another mover is *predicted in it* when its footprint at its position plus its velocity times 0, 4, 8, … ticks and the horizon itself overlaps.
- *Priority:* a participant yields to movers in no plan and to participants of older plans, and ignores movers (moving or stationary) whose center lies behind its own along the lane axis.
- *Oncoming:* a mover in the lane ahead whose velocity against the lane axis exceeds a quarter of the maximum speed.
- *Stationary:* idle, unreachable or held. A moving mover that only stands still is handled by waiting and the 240-tick limit.
- *Waiting:* the participant's control target becomes its braking point, `braking_distance(|velocity|, force)` ahead along its velocity, with arrival braking; `Simulation::passing_target` reports that point, so the mint arrow shows it.
- `PassingEnd::obstructed` is appended to the enum, so the numeric values of the other reasons, which the pressure fingerprints hash, stay the same.

### Step 3: measured result

`kinetra_i4sim_motion_benchmark` in Release, 50,000 ticks, recovery and traffic policies off, same seeds and layouts as the [baseline](#baseline-at-the-start). With passing off every column is identical to the baseline, tick for tick. Each cell: passing on **before → after** the guard; passing off in brackets where it helps the comparison.

| Profile | Blueprint | Seed | Completed | Contacts (episodes) | Waiting ticks | Stuck (at end) | Plans (cancelled) | Contacts in a plan |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Reciprocal | MAS-demonstrator | 42 | 10 → 10 [10] | 0 → 0 | 25041 → 23983 [24006] | 1 (1) → 1 (1) | 4 (1) → 3 (1) | 0 → 0 |
| Reciprocal | MAS-demonstrator | 915839214 | 16 → 16 [16] | 0 → 0 | 10842 → 10663 [12872] | 1 (1) → 1 (1) | 5 (0) → 3 (0) | 0 → 0 |
| Reciprocal | 3840x2400 | 42 | 59 → 60 [59] | 0 → 0 | 3095 → 1358 [1583] | 0 → 0 | 11 (5) → 6 (1) | 0 → 0 |
| Reciprocal | 3840x2400 | 915839214 | 61 → 63 [61] | 0 → 0 | 3177 → 1811 [651] | 0 → 0 | 7 (3) → 6 (1) | 0 → 0 |
| Reciprocal | 8640x4800 | 42 | 99 → 100 [95] | 0 → 0 | 45668 → 41273 [48187] | 6 (0) → 11 (1) [6 (0)] | 244 (87) → 236 (127) | 0 → 0 |
| Reciprocal | 8640x4800 | 915839214 | 103 → 104 [93] | 0 → 0 | 58203 → 33500 [64702] | 10 (2) → 11 (0) [15 (1)] | 219 (74) → 220 (93) | 0 → 0 |
| Improved | MAS-demonstrator | 42 | 14 → 16 [16] | 60 (10) → 134 (11) [53 (9)] | 5452 → 633 [278] | 1 (1) → 0 | 3 (2) → 2 (2) | 0 → 0 |
| Improved | MAS-demonstrator | 915839214 | 14 → 16 [16] | 212 (16) → 203 (11) [204 (12)] | 2019 → 872 [288] | 1 (0) → 0 | 4 (4) → 3 (2) | 0 → 0 |
| Improved | 3840x2400 | 42 | 58 → 59 [54] | 0 → 2 (1) [1 (1)] | 1573 → 1042 [853] | 2 (0) → 1 (0) | 7 (4) → 7 (2) | 0 → 0 |
| Improved | 3840x2400 | 915839214 | 56 → 55 [60] | 0 → 1 (1) [18 (4)] | 1538 → 1402 [1055] | 2 (0) → 2 (0) | 4 (3) → 5 (4) | 0 → 0 |
| Improved | 8640x4800 | 42 | 96 → 99 [98] | 767 (81) → 350 (55) [384 (71)] | 34343 → 18281 [2595] | 4 (0) → 3 (0) | 242 (96) → 212 (116) | 17 → 0 |
| Improved | 8640x4800 | 915839214 | 101 → 102 [99] | 823 (71) → 355 (43) [726 (78)] | 33940 → 16117 [2953] | 6 (0) → 3 (0) | 224 (89) → 197 (108) | 5 → 0 |

Run time on `8640x4800` under Reciprocal, measured one run after another on an otherwise idle machine (this container is about twice as slow as the machine of the roadmap's baseline): passing off 18.4 and 18.6 s before, 18.9 and 18.6 s after; passing on 27.3 and 26.7 s before, 29.4 and 27.9 s after (a first run gave 32.8 and 28.5 s). The guard adds about 5% to a passing run; passing as a whole costs about 50% over passing off, as in the roadmap's baseline.

- **Reciprocal, the reference profile:** with the guard, passing on completes as many products as passing off or more in all six runs (100 and 104 against 95 and 93 on `8640x4800`), still without a contact, and waits less than before the guard and than passing off. Against passing on before the guard: equal or better in all six runs. No regression in products, contacts or waiting. One measure is worse: with seed 42 on `8640x4800`, 11 movers are stuck at some point (one at the end) against 6 before the guard and with passing off; with the other seed 11 against 10 before and 15 off. A run this large is chaotic (a different plan changes every later encounter), so one seed does not establish a trend, but it is recorded as an open point.
- **Improved:** no contact in a plan any more (17 and 5 before); on `8640x4800` 350 and 355 contacts against 384 and 726 with passing off and 767 and 823 before the guard; MAS completes 16 again, like passing off. Seed 42 on MAS touches more (134 contacts in 11 episodes against 53 in 9), and Improved still waits 6–7 times as long as without passing on the large floor. Its remaining contacts are the jostling episodes of [step 1](#step-1-why-improved-has-more-contacts-with-passing): a profile limit.
- **Plans:** about half end without completing, mostly as `obstructed` (109 and 73 of 236 and 220 under Reciprocal on `8640x4800`). The guard gives up maneuvers the traffic around them no longer allows; giving up costs less than the old 240-tick standstill before `no_progress`.
- **`MAS-demonstrator` seed 42:** Reciprocal completes 10 products with or without passing, against 16 with the other seed, and ends with one stuck mover either way. Passing does not cause the gap. It is examined with step 4.

### Step 4: a way out for a boxed-in mover

Planned; refined on its branch. In `MotionStep::follow_route`, after the stall replanning: if the planned route is still obstructed by a stationary mover (the same test `grid_route` uses), the mover gets a route from `recover_route`, which plans with `kinetra::Navigation` around blocked cells and all other bodies. A found route is a detour, as with RECOVERY; without one the mover keeps its route and tries again after the next replanning period.

## Affected areas

- Step 1: `labs/i4sim/tests/include/i4sim/testing/event_counts.h`, `labs/i4sim/tests/motion_benchmark.cpp`, `labs/i4sim/tests/pressure_tests.cpp`, `labs/i4sim/tests/simulation_tests.cpp`, `labs/i4sim/tests/README.md`, this document and the design index.
- Step 2: `labs/i4sim/src/simulation/predictive_passing.cpp`, `include/i4sim/simulation/detail/predictive_passing.h`, `include/i4sim/simulation/events.h`, `simulation_constants.h`, the pressure and simulation suites, golden lines if they change, `labs/i4sim/README.md`, `docs/factory-simulation-architecture.md`, the roadmap's Status row.
- Step 4: `labs/i4sim/src/simulation/motion_step.cpp`, `routing.cpp` and `detail/routing.h`, the simulation suite, golden lines if they change, the i4sim README.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Contact episodes group repeated contacts per pair, by the gap | `kinetra_i4sim:contact episodes group the repeated contacts of one pair` | Targeted |
| A mover stopping on a passing target: today's outcome recorded | `kinetra_i4sim_pressure:a mover stopping on a passing target under {reciprocal,improved} steering ...` | Targeted |
| Measuring changes nothing | The benchmark with the new column reproduces every count of the baseline above | Manual |
| Step 2: all five pressure scenarios end with every mover arrived, no contact and no stuck mover | The pressure suite, renamed to required outcomes | Targeted |
| Step 2: a plan never forms with a lane across a parked mover | `kinetra_i4sim:a passing plan never forms with a lane across a parked mover` (fails before the sweep fix) | Targeted |
| Step 2: a completed plan lets its movers form a new plan at once; a cancelled one still waits 120 ticks | `kinetra_i4sim:a completed passing plan lets its movers pass the next oncoming mover at once` (fails before), `...:movers of a cancelled passing plan wait before they form a new one` | Targeted |
| Step 2: a mover that stops on a remaining leg ends the plan as `obstructed` in the next tick; a crossing mover makes the participant wait and the plan completes; an oncoming mover in the lane ends the plan before any wait could expire | `kinetra_i4sim:a mover that stops on a remaining passing leg ends the plan as obstructed`, `...:a mover crossing a passing lane makes the participant wait and the plan still completes`, `...:a mover driving toward a participant along its lane ends the plan as obstructed` (all fail before the guard; the last also without its oncoming rule) | Targeted |
| Step 2: no regression for Reciprocal with passing on | Benchmark, both seeds, three layouts | Manual, recorded here |
| Step 4: the trapped mover of `4320x2400` seed 2026 leaves; the 48-run harness ends without the standstill | A simulation case on a small fixture; the harness | Targeted, manual |
| Everything else keeps its behavior | Golden, simulation, controller suites; changed lines explained | Targeted, then Full |

## Review

### Steps 2 and 3

`/code-review` (medium) on the branch diff against `test/i4sim-passing-causes`:

| Finding | Resolution |
| --- | --- |
| A crossing intruder found first hid an oncoming mover found later in the same tick | Fixed: every mover is classified |
| The heading came from the first leg, which runs mostly sideways before crossing, so an oncoming mover could be taken for a crossing one | Fixed: ahead, behind and oncoming are judged along the lane axis (entry to exit); the oncoming test was rebuilt so it fails without the rule |
| The hold point assumed full deceleration over half the braking distance the lane reserves; the participant could overshoot and back up | Fixed: the hold point lies `braking_distance` ahead along the velocity |
| A stationary mover behind a participant, within clearance, ended the plan | Fixed: movers behind are ignored, stationary or not |
| Predictions skipped the last ticks of a horizon that is not a multiple of four | Fixed: the horizon itself is always checked |
| The parked-mover test indexed a two-element array with any mover id | Fixed: positions of all movers |
| `obstructed` documents "240 ticks" instead of the constant's name | Kept: the neighboring `no_progress` documents its 240 ticks the same way, and the header is public while the constant is internal |

## Verification

### Step 1

| Check | Result |
| --- | --- |
| `cmake --build --preset release --target kinetra_i4sim_tests kinetra_i4sim_pressure_tests kinetra_i4sim_motion_benchmark` | Built, no warning |
| `ctest --preset release -R '^kinetra_i4sim'` | See the pull request |
| Benchmark with `contact_episodes` | Every count of the baseline unchanged |

### Steps 2 and 3

| Check | Result |
| --- | --- |
| New tests before each fix | The sweep, retry and guard tests failed before their commit, as listed in the [test plan](#test-plan) |
| `ctest --preset release -R '^kinetra_i4sim'` | All pass except the RVO golden line, which also fails on `main` here (Ubuntu GCC 13, not CI's pinned Arch toolchain) |
| Golden lines | One deliberate change: the coordinated factory (sweep fix; totals equal, distance 28894.994 → 28843.187 mm) |
| Pressure lines | Changed deliberately in three scenarios (three oncoming, lane intrusion, lane stop); all ten cases now require no contact, every mover arrived and no stuck mover |
| `cmake --build --preset dev` with `KINETRA_LAB_WARNINGS` for the changed targets | No warning |
| Benchmark | [Step 3](#step-3-measured-result); passing off identical to the baseline |
| `cmake --workflow --preset graphics` (Mesa llvmpipe, offscreen) | 305/310. The five failures also fail on `main` on this machine and pass in CI: four Demo crowd characterization lines and the i4sim RVO golden line, all exact floating-point fingerprints of code this branch does not touch (Ubuntu GCC 13 instead of CI's pinned Arch toolchain). No compiler warning |
| Smoke runs, offscreen, 120 frames: `kinetra --variant demo/dotio/i4sim`, `kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim` | All exit 0 |
| `/code-review` (medium) | See [Review](#review) |
| CI | On the pull request |

## Pull requests

### Step 1

Title: `test(i4sim): replay the causes of passing contacts and standstills`

**Summary.** Roadmap phase 7, step 1. Establishes from replayed runs why Improved has more contacts with predictive passing on, where a passing plan goes wrong, and why two movers stand still near a destination on `4320x2400`. No behavior change.

**Changes.**
- Benchmark: a `contact_episodes` column and `--events <directory>`, which writes each run's event log as CSV.
- `test::contact_episodes` in the shared event counts, with a test.
- A fifth pressure scenario: a mover stopping on a passing target, recorded with today's (failing) outcome under both profiles.
- Design document with the causes, the plan for steps 2 to 4, and the decisions taken without the user.
- i4sim test guide: the new column, option and case.

**Design.** `docs/design/2026-11-21-i4sim-passing-under-pressure.md`

**Verification.** i4sim suites pass; the benchmark reproduces every count of the baseline.

**Review notes.** The causes are in the design document's Problem section, with seed and tick. Decisions marked ✱ were taken without the user, as asked, and are listed under Open points.

**Dependencies and merge order.** None. Steps 2–3 and 4 are stacked on this branch.

**Follow-up.** `fix/i4sim-guarded-passing`, then `fix/i4sim-destination-standstill`.

### Steps 2 and 3

Title: `fix(i4sim): guard predictive passing while a maneuver runs`

**Summary.** Roadmap phase 7, steps 2 and 3. Predictive passing rechecks every plan in every tick: a mover that stops on a lane or drives toward a participant ends the plan, one that passes through makes the participant wait at its braking point, and movers behind are ignored, under one fixed priority. A completed plan no longer delays the next one, and the formation check now sweeps the lanes it means to check. The five pressure scenarios become required outcomes. Passing stays off by default.

**Changes.**
- `lanes_clear` sweeps each lane to its end position; it swept toward the floor origin before (`sweep_translation` takes an end point).
- The 120-tick retry follows only a plan that did not complete.
- `PredictivePassing::guard`: stationary or oncoming movers on a remaining leg end the plan as the new `PassingEnd::obstructed`; a mover predicted in the lane ahead (horizon plus braking distance) makes the participant wait until the lane has been clear for 20 ticks, at most 240; participants yield to movers outside every plan and to older plans. `passing_target` reports the braking point while a participant waits.
- Pressure suite: every case requires no contact, every mover arrived and no stuck mover; five new simulation cases.
- i4sim README, test guide (after-phase-7 benchmark table), factory architecture, roadmap Status row.

**Measured** (Release, 50,000 ticks, both seeds, three layouts; table in the design document): Reciprocal with passing on completes as many products as passing off or more in all six runs (100 and 104 against 95 and 93 on `8640x4800`), without a contact. Improved: no contact in a plan any more (17 and 5 before); on `8640x4800` 350 and 355 contacts against 384 and 726 with passing off. The guard adds about 5% run time. One seed shows more movers stuck at some point on `8640x4800` (11 against 6); recorded as an open point.

**Design.** `docs/design/2026-11-21-i4sim-passing-under-pressure.md`

**Verification.** `ctest --preset release -R '^kinetra_i4sim'` passes except the RVO golden line, which also fails on `main` on this machine (Ubuntu GCC 13 instead of CI's Arch toolchain). Local `cmake --workflow --preset graphics` and CI: see the checks.

**Review notes.** Deliberate golden change: the coordinated factory (sweep fix; totals equal). Deliberate pressure line changes: three oncoming, lane intrusion, lane stop, explained in the commits. Decisions marked ✱ were taken without the user and are listed under Open points.

**Dependencies and merge order.** Stacked on #49 (`test/i4sim-passing-causes`); merge that first. `fix/i4sim-destination-standstill` is stacked on this branch.

**Follow-up.** Step 4, the standstill near a destination.

## Measurement harness

The replay table of [step 1](#step-1-why-improved-has-more-contacts-with-passing) comes from this program. It links `kinetra_i4sim_logic` and `kinetra_simulation` from a Release build; run it as `causes 8640x4800 42` (about two minutes). It takes a copy of the simulation every 250 ticks, and for each contact starts from the latest copy at least 300 ticks before, turns passing off and runs to 50 ticks after the contact. A contact counts as *also without passing* when the same pair starts a contact within 50 ticks of the original tick.

```cpp
#include "i4sim/simulation/simulation.h"
#include <iostream>
#include <map>
using namespace i4sim;
int main(int argc, char **argv) {
    const std::string name = argv[1];
    Simulation s(load_blueprint(default_blueprint_directory() / (name + ".json")), std::stoul(argv[2]));
    auto motion = s.motion_settings();
    motion.predictive_passing = true; // Improved, the default profile
    s.configure_motion(motion);
    s.record_events(true);
    s.run();
    std::map<std::uint64_t, Simulation> copies;
    std::vector<SimulationEvent> contacts;
    for (std::size_t seen = 0; s.ticks() < 50000;) {
        if (s.ticks() % 250 == 0)
            copies.emplace(s.ticks(), s);
        s.tick();
        for (const auto events = s.events(); seen < events.size(); ++seen)
            if (events[seen].kind == EventKind::contact)
                contacts.push_back(events[seen]);
    }
    int also = 0;
    for (const auto &c : contacts) {
        auto from = copies.upper_bound(c.tick > 300 ? c.tick - 300 : 0);
        Simulation replay = std::prev(from)->second;
        auto off = replay.motion_settings();
        off.predictive_passing = false;
        replay.configure_motion(off);
        bool found = false;
        for (std::size_t seen = replay.events().size(); replay.ticks() < c.tick + 50;) {
            replay.tick();
            for (const auto events = replay.events(); seen < events.size(); ++seen)
                found = found || (events[seen].kind == EventKind::contact && events[seen].movers == c.movers &&
                                  events[seen].border == c.border && events[seen].tick + 50 >= c.tick);
        }
        also += found;
    }
    std::cout << contacts.size() << " contacts, " << also << " also without passing\n";
}
```

The episode counts come from the benchmark's `contact_episodes` column; the largest episodes from its `--events` files, grouped the same way. The standstill was found with the [tuning-stall harness](2026-11-20-i4sim-tuning-stall.md#measurement-harness) and replayed by printing the movers within 300 mm of mover 25 every 30 ticks from tick 7800.

## Open points

For the user's review (decisions taken without the user):
- The three stacked pull requests, and Improved's extra contacts documented as a profile limit rather than changed.
- The yield order (outside movers, then older plans), ignoring movers behind, ending at once for an oncoming mover, the 20-tick resume and the 240-tick limit before a plan ends as `obstructed`; the new end reason; no retry delay after a completed plan.
- Passing stays off by default although Reciprocal now gains in every measured run: Improved, the default profile, stays mixed.
- With seed 42 on `8640x4800` under Reciprocal, more movers are stuck at some point with the guard (11) than without passing (6); throughput and waiting are better. More seeds would show whether this is a trend.
- A boxed-in mover gets a `Navigation` detour even with RECOVERY off.

Known and not addressed here:
- Route recovery (RECOVERY) can plan a detour that passes a blocked cell's corner with 3 mm to spare, which the border constraint then refuses: mover 18 on `4320x2400` under Improved, six of the 48 runs with RECOVERY on.
- Improved's jostling episodes beside parked movers, with or without passing.
