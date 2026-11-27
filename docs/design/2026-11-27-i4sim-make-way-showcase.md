# i4sim make-way showcase scene

Status: implemented · Branch: `ccr-4f45aa8d-yy7omd` · Related: [i4sim make way](2026-11-27-i4sim-make-way.md), [i4sim showcase](2026-11-19-i4sim-showcase.md)

MAKE WAY ([#140](https://github.com/SMB-M87/Kinetra-Lab/pull/140)) came with a small test scene, a parked mover in a one-cell lane with a side bay, which its design capture shows. The user saw that capture and asked for such scenes to be added to the i4sim showcase. This change turns the scene into the sixth showcase scene, with its clip in the README, and records the expectation for the next policy.

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

- The parked-blocker fixture lives only in `labs/i4sim/tests/simulation_tests.cpp` (`parked_blocker_fixture`, `start_parked_blocker`). The SHOWCASE page lists five scenes, none with MAKE WAY, so the policy can only be seen by building the scene by hand or on a large floor where it rarely shows.
- The showcase cases assume that every mover has a goal and shuttles: *the bundled showcase holds five scenes* requires at least two goals and every mover shuttling, and *every shuttling showcase scene repeats its traffic situation* requires every mover to shuttle. A make-way scene needs a mover without a goal: MAKE WAY moves only an idle mover at its own parking place with an automatic goal, and never a shuttle participant (`labs/i4sim/src/simulation/make_way.cpp`, `at_parking` and `try_make_way`).
- The how-to table in the i4sim README asked for "a showcase scene when it is worth showing", which left the step easy to skip.

## Goals and non-goals

Goals:

- A showcase scene `make-way.json` that shows a parked mover yielding into a side bay and parking again, on every leg, without contacts.
- Its clip in the root README's i4sim gallery and in the media guide.
- A clear expectation: a test scene that shows a new policy at work becomes a showcase scene in the same pull request.

Non-goals:

- No change to MAKE WAY itself, the scenario format or the stall detector.
- No new clip for the other scenes; their captures are not stale.

## Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| Scene | The parked-blocker fixture of the make-way tests, unchanged: a 1400 × 600 floor, a one-cell lane at y 200–400 and a bay below it at x 600–800 | It is the scene of the design capture the user saw, and the tests already pin its behavior |
| Repeat | Shuttle with one goal: the through mover drives to (1100, 300) and back; the parked mover has no goal | A shuttling blocker cannot make way, and a mover with a goal is not parked. The through mover meets the parked one on both legs, so every leg shows the policy, and batch mode repeats it too, unlike Restart |
| Settings | Reciprocal, heatmap off, MAKE WAY on | The settings of `start_parked_blocker` |
| Showcase rule | A mover without a goal stays parked and does not shuttle; the tests check that the movers with goals shuttle and the others do not | The smallest change to the convention that admits the scene |
| Expectation | A row in the i4sim how-to table and a review lesson | The user's request is a recurring expectation for policy work, and those are the places the feature and review skills read |

## Design

- `labs/i4sim/blueprints/showcase/make-way.json` holds the floor, the two movers (`through`, `parked`), three forbidden zones and a scenario with one goal.
- The showcase cases in `simulation_tests.cpp` accept a scene with fewer goals than movers, list the scene with its hand-built setup, and count a leg in which a mover has the `making_way` goal as coordinated.
- The capture tool records `i4sim-showcase-make-way-clip` like the other showcase clips; its cycle has two standstills, like the other shuttles.

## Affected areas

- i4sim blueprints (data only) and the i4sim simulation tests.
- `tools/capture/`: one scene entry and the capture tests.
- Docs: root README, media guide, blueprint guide, i4sim README and tests README, Lab review lessons.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| The scene loads, applies, sends its one goal and shuttles only the through mover | *the bundled showcase holds six scenes whose scenarios load and apply* | Targeted |
| The scene runs tick for tick like the fixture built by hand | *each showcase scene runs exactly like its hand-configured setup* | Targeted |
| Every leg of four shows a make-way trip, without contacts | *every shuttling showcase scene repeats its traffic situation* | Targeted |
| The clip has two pauses per cycle and is reproducible; it is in the README and the media guide | `kinetra_capture_tests` | Targeted, then the smoke test in CI |

## Verification

Release build with `KINETRA_LAB_WARNINGS=ON`, and the capture tool in the `graphics` preset:

```sh
cmake --preset release -DKINETRA_LAB_WARNINGS=ON
cmake --build --preset release --target kinetra_i4sim_tests kinetra_i4sim_golden_tests \
  kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim \
  kinetra_i4sim_motion_benchmark kinetra_capture_tests kinetra_capture -j 4
ctest --preset release -R '^kinetra_(i4sim|capture)' -j 4
cmake --preset graphics && cmake --build --preset graphics --target kinetra_capture
tools/capture/capture_media.sh build-graphics i4sim-showcase-make-way-clip
python3 external/Kinetra/tools/check_docs.py
clang-format --dry-run --Werror labs/i4sim/tests/simulation_tests.cpp tools/capture/src/scenes/scenes.cpp \
  tools/capture/tests/capture_tests.cpp
```

All 199 i4sim and capture tests passed, without new warnings; before the clip and its docs existed, the capture case *every scene has its file in its docs/media and media guide, and every clip is in its README* failed as expected. The docs check and formatting passed. The graphics suites and the capture smoke test run in CI.

The clip is 190 frames (12.7 s) and 0.4 MB. Its frames show the parked mover entering the bay as the through mover arrives, the through mover passing, and the parked mover returning, on the way out and on the way back, with CONTACTS 0 throughout.

## Review

To be filled in after review.

## Pull request

**Title** `feat(i4sim): add the make-way showcase scene`

**Summary.** Turn MAKE WAY's parked-blocker test scene into the sixth i4sim showcase scene, with its clip in the README, and make adding such a scene part of adding a policy.

**Changes.** `make-way.json` in `labs/i4sim/blueprints/showcase/`; showcase cases that allow a parked mover without a goal and count a make-way trip as coordination; the clip in the capture tool, the README gallery and the media guide; the blueprint guide, the i4sim how-to table and a review lesson.

**Design.** [i4sim make-way showcase scene](2026-11-27-i4sim-make-way-showcase.md).

**Verification.** See the [design document](2026-11-27-i4sim-make-way-showcase.md#verification).

**Review notes.** The showcase convention changes: a scene may hold a parked mover without a goal, which does not shuttle.

**Dependencies and merge order.** Lab only; Kinetra pointer unchanged.

**Follow-up.** None.

## Open points

None.
