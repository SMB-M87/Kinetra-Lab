# Record Kinetra main

Status: implemented · Branch: `chore/submodule-kinetra-main` · Related: [Submodule detached HEAD](2026-11-19-submodule-detached-head.md), [External dependencies](../../external/README.md)

Lab records `b0076a9`, the last commit of Kinetra's `feat/frame-times` branch. Kinetra `main` is its merge commit `59295f9` (PR #9), with an identical tree. With the submodule on `main`, Lab shows `modified: external/Kinetra`. This branch records `59295f9`.

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

- Lab `main` records `b0076a9`; Kinetra `main` is `59295f9`. `git -C external/Kinetra diff --stat b0076a9 59295f9` is empty.

## Goals and non-goals

- Goal: Lab records Kinetra `main` (`59295f9`), so the submodule on `main` matches Lab.
- Non-goal: any engine or Lab code change.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Record the merge commit | Yes | Asked by the user on 2026-11-19 |
| Kinetra pull request | Not needed | `59295f9` is already on Kinetra `main` |

## Design

One pointer commit, following [Submodule detached HEAD](../../external/README.md).

## Affected areas

- `external/Kinetra` pointer.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| The engine sources do not change | `git -C external/Kinetra diff --stat b0076a9 59295f9` is empty | Full is not needed: the tree is identical, so every build input stays the same |
| The pointer is on a Kinetra branch | `git -C external/Kinetra branch -r --contains 59295f9` lists `origin/main` | |

## Review

- The diff is the pointer line and this document; nothing else.

## Verification

| Command | Result |
| --- | --- |
| `git -C external/Kinetra diff --stat b0076a9 59295f9` | Empty: identical tree |
| `git -C external/Kinetra branch -r --contains 59295f9` | `origin/main` |
| `./build.sh` with the submodule at `59295f9` (the run that verified pull request #16) | Kinetra 200/200, Lab 146/146 |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |

## Pull request

Title: `build(deps): record Kinetra main (59295f9)`

Summary: records Kinetra's `main` merge commit `59295f9` (PR #9) instead of its branch commit `b0076a9`. The trees are identical, so no behavior changes. Kinetra needs no pull request; the commit is on Kinetra `main`.

## Open points

- None.
