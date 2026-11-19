# Record Kinetra main after the debug menu steppers

Status: implemented · Branch: `build/kinetra-main-pointer` · Related: [i4sim debug menu](2026-11-19-i4sim-debug-menu.md), [Record Kinetra main](2026-11-19-submodule-kinetra-main.md), [External dependencies](../../external/README.md)

Lab records `d0877b6`, the last commit of Kinetra's `feat/debug-menu-steppers` branch. Kinetra `main` is its merge commit `4af2d97` (Kinetra PR #10), with an identical tree. With the submodule on `main`, Lab shows `modified: external/Kinetra`. This branch records `4af2d97`. It is the same situation as [Record Kinetra main](2026-11-19-submodule-kinetra-main.md) after PR #9.

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

- Lab `main` (after Lab PR #21) records `d0877b6`; Kinetra `main` is `4af2d97`. `git -C external/Kinetra diff --stat d0877b6 4af2d97` is empty.
- Kinetra PR #10 was already merged when Lab PR #21 was opened, so the pointer could have moved to the merge commit on that branch. It was not, and the user saw the submodule as modified again.

## Goals and non-goals

- Goal: Lab records Kinetra `main` (`4af2d97`), so the submodule on `main` matches Lab.
- Non-goal: any engine or Lab code change.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Record the merge commit | Yes | Asked by the user on 2026-11-19 ("die moet weer gepushed worden") |
| Kinetra pull request | Not needed | `4af2d97` is already on Kinetra `main` |

## Design

One pointer commit, following [External dependencies](../../external/README.md).

## Affected areas

- `external/Kinetra` pointer; this document and the design index.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| The engine sources do not change | `git -C external/Kinetra diff --stat d0877b6 4af2d97` is empty | Full is not needed: the tree is identical, so every build input stays the same |
| The pointer is on a Kinetra branch | `git -C external/Kinetra branch -r --contains 4af2d97` lists `origin/main` | |

## Review

- The diff is the pointer line, this document and its index row; nothing else.

## Verification

| Command | Result |
| --- | --- |
| `git -C external/Kinetra diff --stat d0877b6 4af2d97` | Empty: identical tree |
| `git -C external/Kinetra branch -r --contains 4af2d97` | `origin/main` |
| Full Lab run with the identical tree at `d0877b6` ([i4sim debug menu](2026-11-19-i4sim-debug-menu.md#verification)) | `ci` 194/194, `headless` 187/187, `graphics` 207/207 |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |

## Pull request

Title: `build(deps): record Kinetra main (4af2d97)`

```markdown
## Summary
Records Kinetra's `main` merge commit `4af2d97` (SMB-M87/Kinetra#10) instead of its branch commit `d0877b6`. The trees are identical, so nothing changes in the build; the submodule on Kinetra `main` no longer shows as modified in Lab.

## Changes
- `external/Kinetra`: `d0877b6` → `4af2d97`.
- Design document and index row.

## Design
[docs/design/2026-11-19-kinetra-main-pointer.md](https://github.com/SMB-M87/Kinetra-Lab/blob/build/kinetra-main-pointer/docs/design/2026-11-19-kinetra-main-pointer.md)

## Verification
- `git -C external/Kinetra diff --stat d0877b6 4af2d97`: empty (identical tree).
- `4af2d97` is on Kinetra `origin/main`.
- The identical tree passed the full Lab run of SMB-M87/Kinetra-Lab#21: `ci` 194/194, `headless` 187/187, `graphics` 207/207.
- `check_docs.py`: all links resolve.

## Review notes
Only the pointer line and the design document change.

## Dependencies and merge order
None; `4af2d97` is already on Kinetra `main`.

## Follow-up
None.
```

## Open points

- None.
