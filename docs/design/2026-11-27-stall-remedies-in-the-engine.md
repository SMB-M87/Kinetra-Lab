# Stall remedies in the engine

Status: accepted · Branches: `feat/i4sim-departure-check` (step 1 of 5), `refactor/i4sim-passing-coordinator` (step 2) · Related: [Lab roadmap](../roadmap.md#now), [Kinetra roadmap](../../external/Kinetra/docs/roadmap.md), [stall detection](2026-11-26-i4sim-stall-detection.md), [MAKE WAY](2026-11-27-i4sim-make-way.md), [Kinetra ADR 0017](../../external/Kinetra/docs/decisions/0017-passing-coordinator.md)

This change finishes the first *Later* item of both roadmaps. In Lab that item is *Stall detection and remedies in i4sim*, which has two remedies left: the departure check and a temporary motion fallback. In Kinetra it was *i4sim on the passing coordinator*, which Kinetra#71 replaced with *Stall detection and remedies in the engine*. The detection data choose the fallback. i4sim then moves onto Kinetra's `PassingCoordinator`, and stall detection, the fallback and MAKE WAY move into the engine as mechanisms of their own. It is a stack of five steps. This document is shared by every step, and each step fills in its own part.

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

**Departures from the head of a queue.** A mover that already carries a product waits for its turn at its holding place, often its own parking place in a row of parked movers. When its product heads the queue, `Production::approach` ([production.cpp](../../labs/i4sim/src/simulation/production.cpp)) sends it to the processing goal over the plain grid route. Allocation, by contrast, plans a free mover's departure with `require_departure`, refined around stationary footprints ([routing.cpp](../../labs/i4sim/src/simulation/routing.cpp)). The stall-detection replay of mover 25 showed the gap ([stall detection](2026-11-26-i4sim-stall-detection.md#problem)). On `1680x960+` under Improved, every seed of the harness makes 20 contacts when mover 5 leaves at tick 4847 between parked movers 2 and 6.

To measure it, a copy of the code logged every such departure on the 48-run harness (four floors, six seeds, 12,000 ticks, Reciprocal and Improved, other traffic off). For about 330 departures per profile, the check found no route in 3 to 7 cases, and few of those were followed by a stall. Most stalls follow departures that pass the check. The check mostly helps through its refined route, not its veto.

**Long stalls after MAKE WAY.** A second copy of the code classified the motion around each stall at its onset. That covered the 50,000-tick benchmark (its default three floors, two seeds) and the harness, with MAKE WAY on:

| Profile, run | Episodes | ≥ 400 ticks after detection | Of those: *pushed back* | Detector's reason for those |
| --- | ---: | ---: | ---: | --- |
| Reciprocal, 50,000 ticks | 631 | 72 | 60 (56,279 ticks) | `unknown` |
| Improved, 50,000 ticks | 763 | 23 | 9 | mostly `unknown` |
| Reciprocal, harness | 139 | 7 | 3 | `unknown` |
| Improved, harness | 176 | 5 | 2 | `unknown` |

*Pushed back* means the stalled mover moves away from its route leg (`dot(velocity, leg) < −0.05` mm/tick) while a moving neighbor ahead, within 1.6 times their summed half diagonals, drives toward it. The stalled mover is typically within 150 mm of its goal, while the neighbor's route runs through that spot. Under Reciprocal both creep away at about 0.25 mm/tick for hundreds or thousands of ticks: replays of `4320x2400` seed 7 (mover 7 against mover 8, 1,183 ticks) and seed 1234 (mover 25 carried east by mover 0). The detector sees neither a stationary blocker nor an opposing standoff, so it reports `unknown`.

**Two passing coordinators.** Demo and Dotio use Kinetra's `PassingCoordinator`, while i4sim still runs its own `PredictivePassing` (356 lines). That is the same lifecycle that ADR 0017 extracted into the engine. Reading the two side by side shows three differences:

- After a goal change, i4sim waits `passing_retry_ticks` before a new plan, while the engine lets the mover join at once.
- The guard query margin is `|max_half| + speed·horizon` in i4sim and `2·|largest_half| + speed·horizon` in the engine.
- i4sim replans the routes of ended plans before it forms new plans in the same tick. The engine forms them from the headings at the start of the step.

## Goals and non-goals

- Make the departure from the head of a producer queue optionally checked, measured on the harness and the long benchmark.
- Choose the temporary motion fallback and its return rule from the detection data, opt-in and measured.
- Move i4sim onto `kinetra::PassingCoordinator`, and move stall detection, the fallback and MAKE WAY's generic part into `Kinetra::Simulation`. i4sim keeps the factory part: product, recipe step, queue slot and parking reservation.
- Remedies stay opt-in. With them off, every golden and pressure line stays as recorded, except the lines with PASSING on, which step 2 records again once.
- Non-goals: Demo and Dotio adopting the new remedies (roadmap items), the RECOVERY corner case (its own item), a policy that moves several blockers at once.

## Decisions

Settled with the user on 2026-11-27, in three rounds of forms.

| Decision | Choice | Why |
| --- | --- | --- |
| Departure check | *Gate*, opt-in as DEPARTURE CHECK: the refined route when one exists, otherwise wait at the holding place | The prototype, over 50,000 ticks: products +1 to 4% in every configuration; Reciprocal waiting 152,001 → 92,401 (MAKE WAY off) and 104,020 → 79,381 (on); Improved contacts 836 → 233 with MAKE WAY. *Refine* alone raised Reciprocal waiting with MAKE WAY on. The branch after review is in *Verification*: on Kinetra `9841a50` it still pays off under Reciprocal, but under Improved it trades 2 and 4 products for half the contacts. The user kept the gate for both profiles on 2026-11-27, with that trade-off stated in the README (review round 4 of #146) |
| Wait limit of the gate | 1,200 ticks (`10 × stall_ticks`), then depart as without the check | Agent's choice. Without a limit a mover sealed in for good never moves, so no stall starts and MAKE WAY never acts. The longest measured wait was 672 ticks, so the limit changes no measured run |
| Gate on a processing-place departure | No | A mover held at a processing place blocks its producer |
| Gate on a mover still driving to its holding place | No: only a mover that stands there is held back; review round 1 of #146 | Held back in the aisle, it would drive on into its parking row and could seal itself in, and its wait would count its drive |
| Gate on a product rescheduled away from a processing place | Yes, through the same `approach()`; review round 1 of #146 | A product that heads an empty queue the moment it is rescheduled, after a blocked producer for example, leaves its parking row as a queued one does |
| Showcase scene for the sealed departure | No | The scene needs a hold during allocation and timed goals after the product queues, which the scenario format cannot express; making it expressible would change a data contract for one clip |
| Fallback motion | *Right of way* (RIGHT OF WAY), opt-in, Reciprocal and Improved: the pushed mover keeps steering at its goal and the others treat it as not cooperating, so the pusher avoids alone | Prototype over 50,000 ticks, Reciprocal: products 344 → 346, waiting −12%, stuck movers 23 → 16, no contact. *Stand still* failed: Reciprocal 38 contacts and fewer products, Improved waiting ×25 |
| Return rule | A fixed window of 480 ticks (`4 × stall_ticks`), shorter when the mover arrives, changes goal, is held or comes under traffic control | 50,000 ticks: Reciprocal 344 → 351 products, waiting −27%, stuck 23 → 15; Improved 348 → 356, contacts 836 → 644, stuck 6 → 3. Returning on progress was worse than no fallback. In the harness the 480 window has two Reciprocal runs with a stuck mover at the end (base: none) |
| i4sim on `PassingCoordinator` | Yes; the lines with PASSING on are recorded again once the harness shows an equal flow | One lifecycle in three applications; the three differences above are not worth engine options |
| Where MAKE WAY and the fallback live | Both in Kinetra (the user's choice; the agent recommended i4sim) | The user wants the remedies reusable by every application |
| Engine form | Classes of their own beside `PassingCoordinator` in `Kinetra::Simulation` | Each has its own per-step contract and events; an application composes the ones it needs |
| Stall detection | Moves into the engine as well, with the new reason `pushed_back` (the user's choice; the agent recommended keeping it in i4sim) | Both remedies start from an episode, and Demo and Dotio can use the detector too |
| Stack | Five steps on branches named by the repository rule, each its own pull request, merged bottom-up | One question per step |

## Design

| Step | Branches | Content |
| --- | --- | --- |
| 1 | Lab `feat/i4sim-departure-check` (#146) | DEPARTURE CHECK in i4sim |
| 2 | Kinetra and Lab `refactor/i4sim-passing-coordinator` (Kinetra#71, merged; #149) | i4sim's passing runs on `kinetra::PassingCoordinator`; a Kinetra ADR amends 0017 |
| 3 | Kinetra and Lab `feat/stall-detector` (Kinetra#72, #150) | `kinetra::StallDetector`, the reasons of i4sim's detector plus `pushed_back`; i4sim uses it |
| 4 | Kinetra and Lab `feat/right-of-way` (Kinetra#74, #154) | `kinetra::RightOfWay`; i4sim's RIGHT OF WAY |
| 5 | Kinetra and Lab `refactor/make-way-engine` | `kinetra::MakeWay`, the request lifecycle; i4sim keeps refuge candidates, reservations and production |

**Step 1: departure check.** `MotionSettings::departure_check` (DEBUG → TRAFFIC → DEPARTURE CHECK, `scenario.motion.departureCheck`, `--departure-check`) is off by default. When a product with a mover heads its queue, `Production::departure_route` plans the mover's route with `require_departure`:

- When a route exists, the mover gets that refined route.
- When none exists, the mover stays at its holding place with its product and tries again in the next tick. `Production::departure_waits_` keeps the tick at which the wait began, per product.
- After `departure_wait_ticks` (1,200) without a route, it departs as without the setting.
- Turning the setting off releases a waiting mover in the next tick and drops its wait. A hold or a make-way trip drops it too, so the wait starts again when the mover is back.
- Only a mover standing at its holding place is held back; one still driving there turns toward the producer as before.
- `schedule()` leaves the departure of a product rescheduled away from a processing place to `approach()`, so it passes the same check; a mover finishing at a processing place leaves at once.

A departure from a processing place to the next producer is unchanged.

**Step 2: i4sim on the coordinator.** `PredictivePassing` keeps its interface and becomes an adapter over `kinetra::PassingCoordinator`; the 300 lines of its own plan lifecycle go. Per tick it maps every mover to a state:

- a mover that is not moving, or is held, is `standing`;
- one under traffic control, retreating in a corridor or on a make-way trip is `busy`;
- every other mover is `free`, heading along its route.

A lane check keeps each leg on the floor's grid and out of every traffic zone, as before. The coordinator's events go to the event log with the factory's reasons: `unavailable` becomes `participant_stopped`, `participant_held` or `traffic_control`, in the documented order, and `cancelled` becomes `goal_changed` (from `cancel`) or `disabled` (from `clear`). After each ended plan the adapter plans both routes again. Kinetra's [ADR 0018](../../external/Kinetra/docs/decisions/0018-stall-remedies-in-the-engine.md) amends 0017.

Three behaviors follow the engine now:

- After a goal change a mover may join a new plan at once. A goal change outside a plan leaves a running retry wait alone: the adapter cancels only a participant.
- The guard queries a wider margin.
- A mover whose plan ends forms its next plan one tick later, along the route planned again when the plan ended. The adapter gives a participant no heading, because the route it holds is its original one, overridden by the plan.

**Steps 3 to 5** are designed on their branches and written here when each starts. The contracts the user accepted:

- `StallDetector` takes, per mover, a progress counter, a state and a route heading from the application. It returns episodes with an onset diagnosis.
- `RightOfWay` takes episodes and movers, and returns who is not cooperating for how long.
- `MakeWay` owns a request: trigger, alternate route first, then the nearest refuge, a deadline, the return and events. The application supplies refuge candidates and a callback that checks a route against its world and reservations.

## Affected areas

- Step 1:
  - i4sim's `MotionSettings`, `Production`, `simulation_constants.h`, the scenario parser, the TRAFFIC menu and the motion benchmark.
  - The i4sim README, blueprint README, tests README and factory architecture page.
  - Default behavior and every recorded line are unchanged. No capture shows the TRAFFIC section, so no media changes.
- Steps 2 to 5:
  - Kinetra's `Kinetra::Simulation` (new headers beside `passing_coordinator.h`), its navigation contract page and an ADR.
  - i4sim's simulation core, passing, stall detector and MAKE WAY.

## Test plan

Step 1, Targeted level (Lab logic), with the full i4sim suites:

| Behavior | Test |
| --- | --- |
| Without the setting, a sealed queued mover leaves as soon as its product heads the queue | `kinetra_i4sim:without departure check a sealed queued mover leaves at once` |
| With it, the mover stays at its holding place with its product and the producer waits; once a neighbor drives away, it departs long before the limit and reaches the producer without contact | `…departure check keeps a sealed queued mover waiting until a checked route opens` |
| A mover sealed in for good departs exactly `departure_wait_ticks` after its product headed the queue | `…departure check lets a mover sealed in for good depart after the wait limit` |
| Turning the setting off releases a waiting mover in the next tick | `…turning departure check off releases a waiting mover` |
| A hold (or a make-way trip) during the wait starts the wait again, so the limit never lets the mover leave unchecked at once | `…a hold during a departure wait starts the wait again` (fails without the restart) |
| A mover that finishes at a processing place leaves at once for its next producer, with the check on | `…departure check lets a mover leave a processing place at once for its next producer` (fails with the processing-place test inverted) |
| A product rescheduled after a blocked producer, heading the empty queue at once, is held back too | `…departure check also holds back a product rescheduled after a blocked producer` (fails without the fix) |
| The recorded `1680x960+` departure (seed 42, Improved) makes contacts without the check and none with it | `…departure check routes a queued Improved departure around its parked neighbors` |
| The scenario key and the TRAFFIC row | the scenario case and two `kinetra_i4sim_controller` cases, extended |
| Off by default changes nothing | `kinetra_i4sim_golden_tests`, `kinetra_i4sim_pressure_tests` unchanged |

The harness and the 50,000-tick benchmark with `--departure-check` reproduce the measured gate results.

Step 2, Full level (an engine pointer moves; the engine itself is unchanged):

| Behavior | Test |
| --- | --- |
| Every plan outcome, arrival and contact count of the five pressure scenarios under both profiles stays; trajectories and waiting may change, and each change is recorded | `kinetra_i4sim_pressure_tests`, lines recorded again where they moved |
| The golden runs with PASSING on stay as recorded | `kinetra_i4sim_golden_tests` |
| A make-way mover joins no plan; the event log's reasons (`goal_changed`, `participant_held`, …) | the existing `kinetra_i4sim` passing cases |
| Each derived end reason (stopped, held, traffic control), first participant first, and within one participant stopped before control and held before control | `kinetra_i4sim:a participant that stops, is held or comes under traffic control ends its plan, in participant order` (fails with the order flipped) |
| A new goal outside a plan keeps a failed pair's retry wait, and one inside a plan imposes none | `kinetra_i4sim:a new goal outside a plan keeps a failed pair's retry wait; one inside a plan imposes none` (fails without the early return in `cancel`) |
| The flow with PASSING on is compared: identical on the harness; on the long runs products, plans and contacts in plans compared | the harness and the 50,000-tick benchmark with `--predictive-passing`, before and after |

## Verification

Step 1, after review round 3 and the merge of `main` (#142, Kinetra `9841a50`), Release with Lab and Kinetra warnings on:

```sh
cmake --preset release -DKINETRA_LAB_WARNINGS=ON -DKINETRA_WARNINGS=ON
cmake --build --preset release --target kinetra_i4sim_tests kinetra_i4sim_golden_tests \
  kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim kinetra_i4sim_motion_benchmark -j 4
ctest --preset release -R '^kinetra_i4sim' -j 4 --output-on-failure   # 173 of 173 passed
```

The first version of the branch reproduced the measured gate exactly: on the harness with `--make-way --departure-check`, Reciprocal had 111 products, 0 contacts and 19,812 waiting ticks; Improved 104 products, 5 contacts and 2,786 waiting ticks. A mover held by the gate stands idle, so `FlowMetrics` does not count it as waiting. Gated ticks summed to at most 2,016 in a harness profile and 143 over the 50,000-tick benchmark, so the reductions in waiting are not an artifact of that.

Review rounds 1 and 2 narrowed the gate to a standing mover, added the rescheduling path and staged a rescheduled mover at a holding place for its new producer. Measured again with the branch on Kinetra `9841a50`, *base → departure check*, as products / waiting ticks / contacts / runs with a stuck mover at the end:

| Run | MAKE WAY off | MAKE WAY on |
| --- | --- | --- |
| 50,000 ticks, Reciprocal | 334 / 152,001 / 0 / 3 → 347 / 84,776 / 0 / 3 | 344 / 104,020 / 0 / 0 → 349 / 62,636 / 0 / 1 |
| 50,000 ticks, Improved | 347 / 7,710 / 1,370 / 1 → 345 / 15,850 / 692 / 1 | 352 / 7,323 / 835 / 0 → 348 / 7,351 / 398 / 0 |
| Harness, Reciprocal | 110 / 20,382 / 0 / 2 → 113 / 20,874 / 0 / 0 | 110 / 17,434 / 0 / 0 → 113 / 20,874 / 0 / 0 |
| Harness, Improved | 103 / 3,606 / 186 / 0 → 103 / 2,557 / 6 / 0 | 102 / 4,063 / 177 / 2 → 104 / 2,845 / 5 / 2 |

Under Reciprocal the check raises products over 50,000 ticks (334 → 347, 344 → 349) and cuts waiting by 44 and 40%, without a contact. In the shorter harness Reciprocal waits more than its base: without MAKE WAY a little (20,382 → 20,874), with it by a fifth (17,434 → 20,874). Its runs with a stuck mover at the end fall from 2 to 0 without MAKE WAY. Under Improved the check cuts contacts everywhere (1,370 → 692 and 835 → 398 over 50,000 ticks, 186 → 6 and 177 → 5 in the harness) and harness waiting by about 30%. Over 50,000 ticks Improved makes 2 and 4 products fewer, and without MAKE WAY it waits twice as long (7,710 → 15,850).

The merge of `main` moved the pointer from `da98dfa` to `9841a50`, which brings in Kinetra #69, a change to priority steering at rest that i4sim steers with. On `9841a50` Reciprocal's long runs and its harness runs with the check are identical, its two base harness cells move by 1 and 15 waiting ticks, and Improved's runs change. On `da98dfa` Improved's products rose with the check too (343 → 350 and 348 → 352), with the same halving of contacts.

The two Reciprocal harness cells with the check are identical run for run because MAKE WAY never starts there. The parked-blocker departures it cleared on `4320x2400` now leave along checked routes, so no episode needs it. With the check, MAKE WAY starts in 2 harness runs under Improved and 3 and 6 times over the 50,000-tick benchmark.

The gate's figures for the branch after review rounds 1 to 3, on Kinetra `9841a50`, from an instrumented copy that logged every held tick:

- The longest wait is 672 consecutive ticks, the one recurring `4320x2400` mover under Reciprocal on the harness. No run reaches the 1,200-tick limit.
- Held ticks sum to 2,016 (Reciprocal) and 186 (Improved) on the harness, and to at most 79 and 7 over the 50,000-tick benchmark.

The longest wait and the harness sums equal the first version's; over 50,000 ticks the held ticks fell from 143 to at most 79. Round 2's staging of a rescheduled mover at a holding place for its new producer changed no measured run, and the figures are the same on `da98dfa` and `9841a50`.

Step 2, on step 1's branch with Kinetra `9841a50` (Kinetra `main`, which holds #71's ADR and #69), the same Release build: 176 of 176 i4sim cases pass, after the review cases of round 1 and the merges of step 1, the last with Lab `main`'s #142.

- The golden lines, those with PASSING on included, are unchanged.
- Six of the ten pressure lines were recorded again:
  - Every plan count, crossing count, end reason, arrival count and contact count is the same, and five waiting counts too.
  - The Reciprocal three-oncoming case waits 10 ticks instead of 9: its second plan forms a tick after the first completes.
  - The trajectory fingerprints differ.

The flow with PASSING on, before and after, both on Kinetra `9841a50`:

| Run, PASSING on | Products | Contacts (in plans) | Waiting ticks | Stuck movers | Runs stuck at end | Plans formed |
| --- | --- | --- | --- | --- | --- | --- |
| Harness, Reciprocal | 114 → 114 | 0 (0) → 0 (0) | 16,234 → 16,234 | 3 → 3 | 2 → 2 | 33 → 33 |
| Harness, Improved | 104 → 104 | 177 (0) → 177 (0) | 6,571 → 6,571 | 1 → 1 | 1 → 1 | 36 → 36 |
| 50,000 ticks, Reciprocal | 353 → 354 | 0 (0) → 0 (0) | 112,588 → 94,404 | 24 → 14 | 3 → 3 | 474 → 455 |
| 50,000 ticks, Improved | 351 → 353 | 1,049 (0) → 804 (0) | 37,659 → 34,875 | 7 → 10 | 0 → 0 | 426 → 446 |

The harness is identical run for run. The long runs diverge the way any small change makes them diverge:

- Products and plans stay level, and no contact happens in a plan.
- Waiting falls under both profiles, and Improved's own contacts beside parked movers fall.

The first measurement ran on Kinetra `482c072`. On `9841a50`, which brings in Kinetra #69's change to priority steering at rest, the Reciprocal rows are the same and the Improved rows move, with the same findings.

## Review

**Step 1, agent review round 1** ([#146](https://github.com/SMB-M87/Kinetra-Lab/pull/146), *Fix first*):

- *Blocking:* the branch conflicted with `main` (#143, #147). `main` is merged in (`28a8f29`), with both index rows kept and the TRAFFIC rows of #147's menu.
- *Should fix:* `schedule()` let a rescheduled product that heads an empty queue leave unchecked. It now leaves that departure to `approach()`, and a case pins it.
- *Should fix:* the showcase rule of #143. Not taken: the scene needs a hold and timed goals that the scenario format cannot express (*Decisions*).
- *Question:* was a mover still driving to its holding place meant to be held back? No: only a standing mover is now.
- Of the nits, the make-way trip is the same early return as the hold, which a case pins. A retry interval for the gated planning would bound a cost the measurements show is small (at most 2,016 gated ticks per harness profile). The roadmap's *Where* follows each merge.
- The proposed lesson is added to `docs/review-lessons.md`.

**Step 1, agent review round 2** ([#146](https://github.com/SMB-M87/Kinetra-Lab/pull/146), *Fix first*; the four threads of round 1 accepted):

- *Should fix:* `schedule()` indexed `processing_cell` with an unchecked cell id, which is `-1` off the grid. The id is now checked (`ff2f133`), and the proposed lesson is added.
- *Should fix:* two behaviors had no case.
  - A mover finishing at a processing place leaves at once: a new case covers it, and fails with the processing test inverted.
  - A moving mover is not held back: no case. Through the public API it needs a mover with a product that drives inside a sealed corner when its product heads the queue. An operator goal is refused for a mover with a product, and every other path makes the mover stand before `approach()` runs. The condition is one comparison, and the contract names it.
- *Question:* a rescheduled product that does not head its new queue kept its mover at the old producer's holding place. It is now sent to a holding place for the new producer, as a queued product is.
- *Question:* the figures.
  - The identical harness cells are real: MAKE WAY never starts there with the check.
  - The summary sentence now names both harness rises.
  - The gate's figures were measured again for this version.
- Nits: the `///` of `approach()` and `departure_route()` name the rescheduled product and a mover that is not idle (still driving or unreachable), and the decision row marks its figures as the prototype's.

**Step 1, agent review round 3** ([#146](https://github.com/SMB-M87/Kinetra-Lab/pull/146), *Fix first*):

- *Blocking:* the branch conflicted with `main` again (#142). `main` is merged in (`ba64805`), keeping both removals in the roadmap and both lines of the *Known issues*. #142 moves the pointer to Kinetra `9841a50`, which brings in Kinetra #69. Measured again on it (*Verification*): Reciprocal's long runs and the gate's figures are identical. Improved's runs change, and over 50,000 ticks the check now costs it 2 and 4 products while it still halves contacts.
- Nits taken:
  - the counts (173 cases, eight new ones);
  - the test-plan row for the processing-place case;
  - the sentence on the gate's figures;
  - the comment at the staging, and a check for an empty queue before its head, as `approach()` and `update_product()` do (`ac27d31`).
- Not taken: cases for the staging branch and the `cell < 0` guard. Both need an unusable parking place or a floor that is no multiple of its cells, which the public API does not build.

**Step 1, agent review round 4** ([#146](https://github.com/SMB-M87/Kinetra-Lab/pull/146), *Needs the user*):

- *For the user:* the gate no longer raises products in every configuration, the reason the user chose it on. The user chose to keep it for both profiles and to state the trade-off: the README's *Departure check* section and the *Departure check* row of *Decisions* now say that it pays off under Reciprocal and trades products for fewer contacts under Improved.
- The nit on the *Decisions* row is taken with it, and the proposed lesson is on the review-lessons page, together with #150's on the same pattern.

**Step 1, the author's review** (`/code-review` at medium). One finding in the change: the wait's start tick survived a hold or a make-way trip of the waiting mover, so the mover could leave on the plain route at once when it came back. The wait now starts again (`update_product` drops it on its early return), and a new case pins it. The review also reported issues in code this step does not change; they are listed under *Open points* for step 5 and for their owners.

**Step 2, the author's review** (`/code-review` at medium). It found two issues in the change:

- The coordinator's `cancel` also ends a retry wait, so a new goal for a mover outside any plan cut its 120-tick wait after a failed plan short. The adapter now cancels only a participant.
- A mover whose plan completed could form its next plan in the same step from its old, overridden route. The adapter now gives a participant no heading, so the next plan forms a tick later from the route planned again. Two pressure lines moved by a tick of waiting for it.

Two smaller fixes followed: an end reason for a participant on a make-way trip, and `details` builds the plan list only for a participant. Two findings are older than this change and are listed under *Open points*, and so is the cost of the coordinator's own spatial index.

**Step 2, agent review round 1** ([#149](https://github.com/SMB-M87/Kinetra-Lab/pull/149), *Fix first*):

- *Should fix:* no case pinned the goal-change retry rules. A new case keeps a failed pair's wait through a new goal outside a plan, coordinating once per tick as i4sim does, and lets a pair whose plan a goal ended form again at once. It fails without the early return in `cancel`.
- *Should fix:* the derived end reasons and their order had no constructed case. A new case gives each reason and both orders: across participants, and within one. It fails with the order flipped.
- Of the nits:
  - The include, the comments on the switched-off and make-way waits, and the wording of the test-plan rows are fixed.
  - So are the intro, the ADR link, the merge order, the pull request numbers per step and the performance row.
  - Ending every plan with `cancel` on a switch-off instead of `clear()` is documented, not changed: the waits of a switched-off feature have no plan left to protect.

## Pull request

Step 1. Title: `feat(i4sim): check the departure from the head of a producer queue`.

**Summary.** Step 1 of 5 of *Stall remedies in the engine*. DEPARTURE CHECK, off by default, sends a mover that carries a product from its holding place to the producer only along a route refined around stationary movers. Without such a route it waits there for at most 1,200 ticks.

**Changes.**
- `MotionSettings::departure_check`, with `Production::departure_route` and its wait limit.
- The TRAFFIC row, the scenario key `departureCheck` and the benchmark flag `--departure-check`.
- Eight simulation cases; the scenario and menu cases are extended.
- README, blueprint, tests and architecture pages.

**Design.** [Stall remedies in the engine](2026-11-27-stall-remedies-in-the-engine.md), which records the user's decisions for the whole stack.

**Verification.** Every i4sim suite passes (173 cases) with the golden and pressure lines unchanged. On the 50,000-tick benchmark the check raises Reciprocal's products (334 → 347, 344 → 349) and cuts its waiting by 40 to 44%, without a contact. Under Improved it halves contacts (1,370 → 692, 835 → 398) while products fall by 2 and 4, measured on Kinetra `9841a50` after the merge of `main` brought in Kinetra #69.

**Dependencies and merge order.** Lab only; the bottom of the stack, based on `main`.

**Follow-up.** Steps 2 to 5 (this document); Demo and Dotio adoption of the remedies becomes a roadmap item.

Step 2. Title: `feat(i4sim): run predictive passing on Kinetra's PassingCoordinator`.

**Summary.** Step 2 of 5. i4sim's passing becomes an adapter over the engine's coordinator, as Demo's and Dotio's already are, and its own plan lifecycle is removed. Kinetra's ADR 0018 records the move and the remedies to come.

**Changes.**
- `PredictivePassing` maps movers to `PassingMoverState`, checks lanes against the floor and its zones, logs events with the factory's reasons and restores routes. It cancels only a participant, and gives a participant no heading.
- `coordinate` no longer takes the spatial index.
- Six pressure lines are recorded again.
- The architecture, performance and i4sim pages are updated, the retry comment follows the engine, and the Kinetra pointer moves to the ADR branch.

**Design.** [Stall remedies in the engine](2026-11-27-stall-remedies-in-the-engine.md#design), step 2; Kinetra [ADR 0018](../../external/Kinetra/docs/decisions/0018-stall-remedies-in-the-engine.md).

**Verification.** 171 of 171 i4sim cases pass, and the golden lines are unchanged. The pressure lines keep every outcome; one wait grows by a tick and the trajectories differ. With PASSING on, the harness is identical. Over 50,000 ticks products stay level while waiting falls (Reciprocal −16%) and Improved's contacts fall from 1,049 to 804, on Kinetra `9841a50`; the tables are in the design document.

**Dependencies and merge order.** It sits on step 1 (#146) and merges after it. Kinetra's ADR pull request (Kinetra#71) has merged.

**Follow-up.** Steps 3 to 5.

## Open points

- The *refine* variant without the wait cut Improved contacts as well as the gate, but raised Reciprocal waiting with MAKE WAY on (104,020 → 111,295). The gate was chosen; the data stay here.
- Found by step 1's review in existing MAKE WAY code, for step 5, which moves it:
  - `checked_route` applies `traffic_clear` to the beneficiary's routes, so a stalled mover whose route crosses an enabled junction or corridor never gets help.
  - A yielding mover braking inside a zone switched on around it passes `controlled` to `integrate`, never snaps to idle and can hold the zone with no deadline.
  - A yielding mover that is held keeps its trip and goal, so its queued product freezes the producer's queue for the length of the hold.
- Found by step 2's review in behavior the adapter kept from i4sim's coordinator:
  - Restoring a participant's route resets its control, which on a `traffic_control` end drops the corridor recovery's control for that tick.
  - Restoring the route of a participant that stopped (went idle) sets it moving again toward its destination.
- With passing on, the coordinator builds its own spatial index over the movers each tick, next to the one `MotionStep` builds; the profile in [performance](../performance.md#i4sim) predates the move.
- Found by step 1's review, outside this stack:
  - i4sim's inspector STALLED row (from `stall_details`) and the HUD's STALLED count (from `stalled()`) use different definitions and can disagree for an exit-waiting mover.
  - Demo's `passing_events()` drops the events of plans that `end_passing()` ends, and Dotio clears them before they are read.
- In the 12,000-tick harness the gate holds one recurring mover for 672 ticks on `4320x2400` under Reciprocal. A route that opens only when a neighbor leaves its parking place cannot be found by refinement; MAKE WAY does not act on an idle mover.
