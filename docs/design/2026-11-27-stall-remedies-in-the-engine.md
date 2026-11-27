# Stall remedies in the engine

Status: accepted · Branches: `feat/i4sim-departure-check` (step 1 of 5), `refactor/i4sim-passing-coordinator` (step 2), `feat/stall-detector` (step 3) · Related: [Lab roadmap](../roadmap.md#now), [Kinetra roadmap](../../external/Kinetra/docs/roadmap.md), [stall detection](2026-11-26-i4sim-stall-detection.md), [MAKE WAY](2026-11-27-i4sim-make-way.md), [Kinetra ADR 0017](../../external/Kinetra/docs/decisions/0017-passing-coordinator.md)

This change finishes the first *Later* item of both roadmaps. In Lab that item is *Stall detection and remedies in i4sim*, which has two remedies left: the departure check and a temporary motion fallback. In Kinetra it is *i4sim on the passing coordinator*. The detection data choose the fallback. i4sim then moves onto Kinetra's `PassingCoordinator`, and stall detection, the fallback and MAKE WAY move into the engine as mechanisms of their own. It is a stack of five steps. This document is shared by every step, and each step fills in its own part.

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
| Departure check | *Gate*, opt-in as DEPARTURE CHECK: the refined route when one exists, otherwise wait at the holding place | Over 50,000 ticks: products +1 to 4% in every configuration; Reciprocal waiting 152,001 → 92,401 (MAKE WAY off) and 104,020 → 79,381 (on); Improved contacts 836 → 233 with MAKE WAY. *Refine* alone raised Reciprocal waiting with MAKE WAY on |
| Wait limit of the gate | 1,200 ticks (`10 × stall_ticks`), then depart as without the check | Agent's choice. Without a limit a mover sealed in for good never moves, so no stall starts and MAKE WAY never acts. The longest measured wait was 672 ticks, so the limit changes no measured run |
| Gate on a processing-place departure | No | A mover held at a processing place blocks its producer |
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
| 1 | Lab `feat/i4sim-departure-check` | DEPARTURE CHECK in i4sim |
| 2 | Kinetra and Lab `refactor/i4sim-passing-coordinator` | i4sim's passing runs on `kinetra::PassingCoordinator`; a Kinetra ADR amends 0017 |
| 3 | Kinetra and Lab `feat/stall-detector` | `kinetra::StallDetector`, the reasons of i4sim's detector plus `pushed_back`; i4sim uses it |
| 4 | Kinetra and Lab `feat/right-of-way` | `kinetra::RightOfWay`; i4sim's RIGHT OF WAY |
| 5 | Kinetra and Lab `refactor/make-way-engine` | `kinetra::MakeWay`, the request lifecycle; i4sim keeps refuge candidates, reservations and production |

**Step 1: departure check.** `MotionSettings::departure_check` (DEBUG → TRAFFIC → DEPARTURE CHECK, `scenario.motion.departureCheck`, `--departure-check`) is off by default. When a product with a mover heads its queue, `Production::departure_route` plans the mover's route with `require_departure`:

- When a route exists, the mover gets that refined route.
- When none exists, the mover stays at its holding place with its product and tries again in the next tick. `Production::departure_waits_` keeps the tick at which the wait began, per product.
- After `departure_wait_ticks` (1,200) without a route, it departs as without the setting.
- Turning the setting off releases a waiting mover in the next tick and drops its wait. A hold or a make-way trip drops it too, so the wait starts again when the mover is back.

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

**Step 3: stall detection in the engine.** `kinetra::StallDetector` ([navigation contract](../../external/Kinetra/docs/engine/navigation.md#stall-detection)) takes over i4sim's observation and diagnosis. Each step the caller gives it, per mover:

- the body;
- the point it heads for (i4sim: the next waypoint);
- the goal;
- the steps without progress (i4sim: the route-progress counter);
- a `StallMoverState`.

i4sim marks a mover on its own route, outside every coordination, `observed`; one that is not moving or is held `standing`; any other mover `busy`. That is the eligibility of its former detector. The engine adds the reason `pushed_back` after `opposing_mover`: the mover moves back along its leg faster than `stop_speed` while a moving neighbor within 1.6 combined half diagonals, within about 78 degrees of the leg ahead, approaches faster than `stop_speed`. i4sim's `StallReason` and `StallDetails` become aliases of the engine's `StallReason` and `StallDiagnosis`.

**Steps 4 and 5** are designed on their branches and written here when each starts. The contracts the user accepted:

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
| The recorded `1680x960+` departure (seed 42, Improved) makes contacts without the check and none with it | `…departure check routes a queued Improved departure around its parked neighbors` |
| The scenario key and the TRAFFIC row | the scenario case and two `kinetra_i4sim_controller` cases, extended |
| Off by default changes nothing | `kinetra_i4sim_golden_tests`, `kinetra_i4sim_pressure_tests` unchanged |

The harness and the 50,000-tick benchmark with `--departure-check` reproduce the measured gate results.

Step 2, Full level (an engine pointer moves; the engine itself is unchanged):

| Behavior | Test |
| --- | --- |
| Every plan outcome, arrival, contact and waiting count of the five pressure scenarios under both profiles stays; only trajectories may change | `kinetra_i4sim_pressure_tests`, lines recorded again where the fingerprint moved |
| The golden runs with PASSING on stay as recorded | `kinetra_i4sim_golden_tests` |
| A make-way mover joins no plan; the event log's reasons (`goal_changed`, `participant_held`, …) | the existing `kinetra_i4sim` passing cases |
| The flow with PASSING on is equal | the harness and the 50,000-tick benchmark with `--predictive-passing`, before and after |

Step 3, Full level (a new public engine header):

| Behavior | Test |
| --- | --- |
| The detector's contract at two world scales: threshold and end, each reason and its order, what is no blocker and no push, oscillation, restarts, absent movers, a shorter span, a goal at the origin, invalid input that changes nothing | Kinetra `kinetra_stall_detector_tests` (15 cases; the push case fails without the push branch, the last two without their fixes) |
| An external consumer can use it | Kinetra's consumer check |
| i4sim's diagnoses stay, and motion does not change | the existing stall cases of `kinetra_i4sim`, the golden and pressure suites, and the long benchmark with MAKE WAY |
| The recorded `4320x2400` stall of mover 7 (seed 7, Reciprocal, MAKE WAY) is `pushed_back` by mover 8 | `kinetra_i4sim:the recorded 4320x2400 stall of a mover pushed from its goal is diagnosed as pushed back` |
| A force so small that the braking distance is infinite still lets a tick observe stalls | `kinetra_i4sim:a force so small that braking never ends still lets stalls be observed` (fails without the clamp) |

## Verification

Step 1, on Lab `main` `39cb628` with Kinetra `e3ad07c`, Release with Lab and Kinetra warnings on:

```sh
cmake --preset release -DKINETRA_LAB_WARNINGS=ON -DKINETRA_WARNINGS=ON
cmake --build --preset release --target kinetra_i4sim_tests kinetra_i4sim_golden_tests \
  kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim kinetra_i4sim_motion_benchmark -j 4
ctest --preset release -R '^kinetra_i4sim' -j 4 --output-on-failure   # 171 of 171 passed
```

The branch reproduces the measured gate exactly. On the harness with `--make-way --departure-check`:

- Reciprocal: 111 products, 0 contacts, 19,812 waiting ticks.
- Improved: 104 products, 5 contacts, 2,786 waiting ticks.

A mover held by the gate stands idle, so `FlowMetrics` does not count it as waiting. Gated ticks summed to at most 2,016 in a harness profile and 143 over the 50,000-tick benchmark, so the reductions in waiting are not an artifact of that.

The 50,000-tick rows come from the measuring copy of the code. The branch matches it on every harness run, and no run reaches the wait limit:

| 50,000 ticks, products / waiting / contacts / runs stuck at end | Base | Departure check |
| --- | --- | --- |
| Reciprocal, MAKE WAY off | 334 / 152,001 / 0 / 3 | 346 / 92,401 / 0 / 3 |
| Reciprocal, MAKE WAY on | 344 / 104,020 / 0 / 0 | 351 / 79,381 / 0 / 1 |
| Improved, MAKE WAY off | 343 / 8,022 / 1,386 / 1 | 356 / 8,331 / 1,430 / 2 |
| Improved, MAKE WAY on | 348 / 7,638 / 836 / 1 | 352 / 8,109 / 233 / 0 |

Step 2, on step 1's branch with Kinetra `e81fab3` (`refactor/i4sim-passing-coordinator`, ADR only), the same Release build: 171 of 171 i4sim cases pass.

- The golden lines, those with PASSING on included, are unchanged.
- Six of the ten pressure lines were recorded again:
  - Every plan count, crossing count, end reason, arrival count and contact count is the same, and five waiting counts too.
  - The Reciprocal three-oncoming case waits 10 ticks instead of 9: its second plan forms a tick after the first completes.
  - The trajectory fingerprints differ.

The flow with PASSING on, before and after:

| Run, PASSING on | Products | Contacts (in plans) | Waiting ticks | Stuck movers | Runs stuck at end | Plans formed |
| --- | --- | --- | --- | --- | --- | --- |
| Harness, Reciprocal | 114 → 114 | 0 (0) → 0 (0) | 16,234 → 16,234 | 3 → 3 | 2 → 2 | 33 → 33 |
| Harness, Improved | 106 → 106 | 168 (0) → 168 (0) | 6,326 → 6,326 | 0 → 0 | 0 → 0 | 38 → 38 |
| 50,000 ticks, Reciprocal | 353 → 354 | 0 (0) → 0 (0) | 112,588 → 94,404 | 24 → 14 | 3 → 3 | 474 → 455 |
| 50,000 ticks, Improved | 347 → 349 | 1,045 (0) → 800 (0) | 38,347 → 35,563 | 9 → 12 | 0 → 0 | 426 → 446 |

The harness is identical run for run. The long runs diverge the way any small change makes them diverge:

- Products and plans stay level, and no contact happens in a plan.
- Waiting falls under both profiles, and Improved's own contacts beside parked movers fall.

Step 3, with Kinetra `24fb31d` (`feat/stall-detector`):

- Kinetra's release preset with warnings as errors: 327 of 327 tests pass, and the external consumer builds and runs headless.
- Lab: 173 of 173 i4sim cases pass.
- The 50,000-tick benchmark with MAKE WAY gives the base's numbers exactly (Reciprocal 344 products, 104,020 waiting ticks, no contact; Improved 348, 7,638, 836), so detection changes no motion.
- Its episodes, by reason:
  - Reciprocal: 412 `pushed_back` of 631, the count the classifying replay found, with 166 `unknown`, 42 `opposing_mover`, 6 `stationary_blocker` and 5 `oscillating`.
  - Improved: 322 `pushed_back` of 763.

## Review

**Step 1, the author's review** (`/code-review` at medium). One finding in the change: the wait's start tick survived a hold or a make-way trip of the waiting mover, so the mover could leave on the plain route at once when it came back. The wait now starts again (`update_product` drops it on its early return), and a new case pins it. The review also reported issues in code this step does not change; they are listed under *Open points* for step 5 and for their owners.

**Step 2, the author's review** (`/code-review` at medium). It found two issues in the change:

- The coordinator's `cancel` also ends a retry wait, so a new goal for a mover outside any plan cut its 120-tick wait after a failed plan short. The adapter now cancels only a participant.
- A mover whose plan completed could form its next plan in the same step from its old, overridden route. The adapter now gives a participant no heading, so the next plan forms a tick later from the route planned again. Two pressure lines moved by a tick of waiting for it.

Two smaller fixes followed: an end reason for a participant on a make-way trip, and `details` builds the plan list only for a participant.

**Step 3, the author's review** (`/code-review` at high, the Lab diff and Kinetra's). Three issues:

- A shorter movers span left the episodes of the indices it dropped active, and a returning index took them over. Those episodes now end, with events.
- A fresh track's zero goal and anchor matched a mover whose goal is the world origin, so its first move toward the goal read as a reversal. A track now anchors at the mover on its first observed step.
- In i4sim a tiny force that `configure_motion` accepts made the lookahead infinite, which the engine rejects in every tick, after motion. The adapter clamps it.

Each now has a case that fails without its fix. Three documentation gaps were closed too: the README's module row, the testing table, and the validation of absent movers. Two findings are older than this change and are listed under *Open points*, and so is the cost of the coordinator's own spatial index.

## Pull request

Step 1. Title: `feat(i4sim): check the departure from the head of a producer queue`.

**Summary.** Step 1 of 5 of *Stall remedies in the engine*. DEPARTURE CHECK, off by default, sends a mover that carries a product from its holding place to the producer only along a route refined around stationary movers. Without such a route it waits there for at most 1,200 ticks.

**Changes.**
- `MotionSettings::departure_check`, with `Production::departure_route` and its wait limit.
- The TRAFFIC row, the scenario key `departureCheck` and the benchmark flag `--departure-check`.
- Six simulation cases; the scenario and menu cases are extended.
- README, blueprint, tests and architecture pages.

**Design.** [Stall remedies in the engine](2026-11-27-stall-remedies-in-the-engine.md), which records the user's decisions for the whole stack.

**Verification.** Every i4sim suite passes (171 cases) with the golden and pressure lines unchanged. On the 50,000-tick benchmark the check raises products by 1 to 4% in every configuration, cuts Reciprocal waiting by 24 to 39%, and cuts Improved contacts from 836 to 233 with MAKE WAY.

**Dependencies and merge order.** Lab only; the bottom of the stack, based on `main`.

**Follow-up.** Steps 2 to 5 (this document); Demo and Dotio adoption of the remedies becomes a roadmap item.

Step 3. Title: `feat(i4sim): observe stalls with Kinetra's StallDetector`. Kinetra's: `feat(simulation): add StallDetector for episodes without progress`.

**Summary.** Step 3 of 5. Stall detection moves into the engine as `StallDetector`, with a new reason, `pushed_back`, for a mover pushed away from its goal by a neighbor driving through its place. i4sim feeds it and records its episodes. Motion does not change.

**Changes.**
- Kinetra: `stall_detector.h` and `.cpp`, 15 contract cases, the consumer check, the navigation contract, the README, the testing table, and the module and test READMEs. The roadmap item moves to *Now*.
- Lab:
  - i4sim's `StallDetector` becomes an adapter over the engine's.
  - `StallReason` and `StallDetails` alias the engine's types, and the event name `pushed_back` is added.
  - A recorded case pins `pushed_back`.
  - The architecture, i4sim and tests pages are updated.

**Verification.** Kinetra 327 of 327, the consumer, Lab i4sim 173 of 173. The long benchmark gives the base's numbers exactly, and 412 of Reciprocal's 631 episodes are `pushed_back`.

**Dependencies and merge order.** Kinetra's step 3 sits on Kinetra's step 2 (ADR) and merges after it; Lab's step 3 sits on Lab's step 2 and merges after it and after Kinetra's step 3.

**Follow-up.** Steps 4 and 5.

Step 2. Title: `feat(i4sim): run predictive passing on Kinetra's PassingCoordinator`.

**Summary.** Step 2 of 5. i4sim's passing becomes an adapter over the engine's coordinator, as Demo's and Dotio's already are, and its own plan lifecycle is removed. Kinetra's ADR 0018 records the move and the remedies to come.

**Changes.**
- `PredictivePassing` maps movers to `PassingMoverState`, checks lanes against the floor and its zones, logs events with the factory's reasons and restores routes. It cancels only a participant, and gives a participant no heading.
- `coordinate` no longer takes the spatial index.
- Six pressure lines are recorded again.
- The architecture, performance and i4sim pages are updated, the retry comment follows the engine, and the Kinetra pointer moves to the ADR branch.

**Design.** [Stall remedies in the engine](2026-11-27-stall-remedies-in-the-engine.md#design), step 2; Kinetra [ADR 0018](https://github.com/SMB-M87/Kinetra/blob/refactor/i4sim-passing-coordinator/docs/decisions/0018-stall-remedies-in-the-engine.md).

**Verification.** 171 of 171 i4sim cases pass, and the golden lines are unchanged. The pressure lines keep every outcome; one wait grows by a tick and the trajectories differ. With PASSING on, the harness is identical. Over 50,000 ticks products stay level while waiting falls (Reciprocal −16%) and Improved's contacts fall from 1,045 to 800; the tables are in the design document.

**Dependencies and merge order.** It sits on step 1 (#146) and merges after it. Kinetra's ADR pull request merges first, with a merge commit.

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
