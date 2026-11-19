# Submodule detached HEAD

Status: implemented · Branch: `docs/submodule-detached-head` · Related: [External dependencies](../../external/README.md), [Git with the engine submodule](../../AGENTS.md#git-with-the-engine-submodule)

`external/README.md` gets a section on the detached HEAD of `external/Kinetra`: why it is detached, how to put it on Kinetra's `main` with git or lazygit, and what that does to Lab's recorded pointer. The branch also applies that procedure once, recording Kinetra's `main` merge commit.

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

- After `git submodule update`, `external/Kinetra` is on a detached HEAD. [External dependencies](../../external/README.md#change-kinetra-from-inside-lab) mentions this in one sentence but does not say how to get back on a branch.
- Switching the submodule to `main` moves it off the recorded commit even when nothing changed in the files: Lab records `7c845e0`, and Kinetra `main` is its merge commit `0c8d5b8` (PR #7), with an identical tree. Lab then shows `modified: external/Kinetra (new commits)`, and the docs do not say whether to revert or commit that.

## Goals and non-goals

- Goal: one section that explains the detached HEAD, gives the git and lazygit steps to reach `main`, and the two choices for the resulting pointer change, with commands for each.
- Goal: record Kinetra `main` (`0c8d5b8`) in Lab, so the submodule on `main` matches Lab.
- Non-goal: changing the submodule rules in `AGENTS.md` or Kinetra's workflow; the new section follows them.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Do it | Document it in Lab | Asked by the user on 2026-11-19 |
| Place | A new last section in `external/README.md`, plus a link from the merge paragraph and a wider question in `docs/README.md` | That page holds all manual submodule steps; one authoritative place |
| Pointer bump | Commit `0c8d5b8` on this branch, as a separate `build` commit | The user asked to record the correct reference; the tree is identical to `7c845e0`, so behavior does not change |
| Kinetra pull request for a pointer to `origin/main` | Not needed | The commit is already published on Kinetra `main`; the AGENTS.md rule only forbids pointers to commits on no Kinetra branch |

No decision meets the ADR criteria.

## Design

The section *Put the submodule on Kinetra main* covers: why `git submodule update` leaves a detached HEAD and that this is intended; the switch-and-pull commands with a status check first; the lazygit keys; why the checkout then differs from the recorded commit even after a merge-commit merge; a table with the two choices (restore with `git submodule update`, or record the pointer); the precondition that the commit is on `origin/main`; and the commands to record it on a Lab branch and open a pull request.

## Affected areas

- `external/README.md`: table of contents, one sentence in *Change Kinetra from inside Lab*, the new section.
- `docs/README.md`: the submodule question in *Architecture guides*.
- `external/Kinetra`: pointer `7c845e0` → `0c8d5b8`.
- `docs/design/`: this document and its index row.

## Test plan

Documentation and a pointer to an identical tree, so the **Docs** level applies: `python3 external/Kinetra/tools/check_docs.py` passes. `git diff --stat 7c845e0 0c8d5b8` in Kinetra is empty, which shows the pointer move builds the same sources; no build is needed.

## Review

A read of `git diff main...HEAD` against the review list in the [workflow](../../external/Kinetra/docs/workflow.md#6-review); for a documentation diff and a pointer to an identical tree this replaces `/code-review`.

| Finding | Resolution |
| --- | --- |
| The `pull` step fails without SSH keys, as it did in this session | Added a link to the HTTPS switch in the README quick start |
| The `[ahead N]` check is only meaningful after a fetch | Kept: the documented `pull` fetches first |
| The existing sentence "no further Lab commit is needed" could read as contradicting the new section | Kept it, since it stays true, and added a link that recording the merge commit is optional |

## Verification

Level **Docs**; no build, because only Markdown and a pointer to an identical tree changed.

- `python3 external/Kinetra/tools/check_docs.py`: "All links resolve in 41 Markdown files".
- `git -C external/Kinetra diff --stat 7c845e0 0c8d5b8`: empty, so the engine sources are unchanged.
- `git -C external/Kinetra status -sb`: `## main...origin/main`, so `0c8d5b8` is published on Kinetra `main`.

## Pull request

Title: `docs: explain the detached submodule HEAD and record Kinetra main`

```markdown
## Summary

After `git submodule update`, `external/Kinetra` sits on a detached HEAD, and the docs did not say how to put it on Kinetra `main` or what to do with the pointer change that follows. `external/README.md` now explains this, with git and lazygit steps. This branch also records Kinetra `main` (merge commit `0c8d5b8`) instead of `7c845e0`; both have the same tree.

## Changes

- `external/README.md`: new section *Put the submodule on Kinetra main*, covering why HEAD is detached, switching and pulling, lazygit keys, why the pointer then shows as modified, the choice between restoring and recording, the `origin/main` precondition, and the commands to record it on a Lab branch. Plus a link from the merge paragraph.
- `docs/README.md`: the submodule question in *Architecture guides* now names the detached HEAD.
- `external/Kinetra`: `7c845e0` → `0c8d5b8` (merge of Kinetra PR #7; no file changes).
- `docs/design/`: design document and index row.

## Design

[docs/design/2026-11-19-submodule-detached-head.md](docs/design/2026-11-19-submodule-detached-head.md). No ADR.

## Verification

Docs level:
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve in 41 Markdown files.
- `git -C external/Kinetra diff --stat 7c845e0 0c8d5b8`: empty, so the engine sources are unchanged.
- `git -C external/Kinetra status -sb`: `## main...origin/main`, so the new pointer is published.

No build, because the pointer moves to an identical tree.

## Review notes

Self-review of the diff: added an HTTPS hint for the pull step. The existing sentence "no further Lab commit is needed" stays true and now links to the optional procedure.

## Dependencies and merge order

None. The Kinetra commit is already on Kinetra `main`, so no Kinetra pull request is involved.

## Follow-up

None.
```

## Open points

None.
