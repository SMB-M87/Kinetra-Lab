# Record the Kinetra corner slide fix

Status: implemented · Branch: `fix/record-kinetra-corner-slide` · Related: [Kinetra corner slide design](../../external/Kinetra/docs/design/2026-11-19-border-corner-slide.md), [H crossing motion](2026-11-19-i4sim-h-crossing-motion.md)

Three i4sim tests fail on Lab `main` since the Kinetra pointer moved to a Kinetra `main` without the border corner slide fix. This change records the Kinetra `main` merge commit that contains the fix.

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

- The [H crossing motion](2026-11-19-i4sim-h-crossing-motion.md) change depends on the Kinetra fix "let a box sliding along a wall pass a convex corner" (Kinetra pull request 11). Lab recorded that fix's branch commit `745ac3c`.
- Lab pull request 27 then recorded Kinetra `main` (`4af2d97`) while Kinetra pull request 11 was still open. `745ac3c` is not an ancestor of `4af2d97`, so the fix dropped out of Lab.
- Since then the `headless` and `ci` workflows fail on Lab `main` with three `kinetra_i4sim` cases: *without path planning a mover steers straight at its goal and still needs a route* ("The mover stuck on the way"), *every shuttling showcase scene repeats its traffic situation* (`h-crossing.json` completes 0 of 4 legs), and *the h-crossing scene completes its legs without contacts when a mover starts late* ("The movers locked"). A box mover stops at a convex wall corner.
- Reproduced locally on Lab `main` (`cmake --workflow --preset headless`: 3 of 189 failed). With pull request 11 merged locally into Kinetra `main`, all 189 passed.

## Goals and non-goals

- Goal: Lab `main` builds and passes its tests again, with a pointer to a commit on Kinetra `main`.
- Non-goal: changes to i4sim or to the engine fix itself.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Fix | Record the Kinetra `main` merge commit `52238f5` of pull request 11 | Lab `main` must record a commit on Kinetra `main` ([AGENTS.md](../../AGENTS.md#git-with-the-engine-submodule)); moving back to the branch commit would break that rule and need a second pointer change. |
| Prevention | Before recording Kinetra `main`, check that the old pointer is an ancestor of the new one (`git -C external/Kinetra merge-base --is-ancestor <old> <new>`) | A pointer change that is not a fast-forward silently drops engine commits. |

## Design

Only the `external/Kinetra` pointer moves, from `4af2d97` to `52238f5`. The engine diff between them is the corner slide fix (`src/simulation/priority_steering.cpp`, `include/kinetra/steering.h`, a navigation test and docs) and the README icon.

## Affected areas

- `external/Kinetra` pointer
- This document and the [design index](README.md#index)

## Test plan

Full level, as for every pointer change: the `headless` and `ci` workflows in Lab, and Kinetra's own `ci` workflow. The three failing cases above are the regression tests; they exist already.

## Review

Checked that `745ac3c` is an ancestor of `52238f5`, that the engine diff contains no runtime or presentation change (so the graphics workflow is not needed), and that the branch changes only the pointer and this document. No findings.

## Verification

- `git -C external/Kinetra merge-base --is-ancestor 745ac3c 52238f5`: true.
- `cmake --workflow --preset headless`: passed, 189 of 189 tests.
- `cmake --workflow --preset ci`: passed, 214 of 214 tests. The build reports only the existing report-only Lab warnings in `labs/demo`; this branch changes no Lab code.
- In `external/Kinetra`, `cmake --workflow --preset ci`: passed, 223 of 223 tests, no warnings.
- `python3 external/Kinetra/tools/check_docs.py`: passed.

## Pull request

**Title:** `fix(deps): record the Kinetra corner slide fix`

**Summary:** Lab `main` fails three i4sim tests because pull request 27 moved the Kinetra pointer to a `main` without the border corner slide fix. This records the Kinetra `main` merge commit of that fix.

**Changes:** `external/Kinetra` from `4af2d97` to `52238f5` (merge of SMB-M87/Kinetra#11); this design document.

**Design:** [docs/design/2026-11-19-record-kinetra-corner-slide.md](https://github.com/SMB-M87/Kinetra-Lab/blob/fix/record-kinetra-corner-slide/docs/design/2026-11-19-record-kinetra-corner-slide.md)

**Verification:** see the design document; `headless` and `ci` in Lab and Kinetra's `ci` all pass, including the three failing i4sim cases.

**Review notes:** pointer change only; Kinetra pull request 11 is already merged with a merge commit.

**Dependencies and merge order:** none left; Kinetra is merged.

**Follow-up:** before future pointer changes, check that the old pointer is an ancestor of the new one.

## Open points

- None.
