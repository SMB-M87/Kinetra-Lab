# Record Kinetra main after the weighted grid search

Status: implemented · Branch: `build/record-kinetra-weighted-grid-search` · Related: [Dotio restructuring](2026-11-21-dotio-restructuring.md), [External dependencies](../../external/README.md)

SMB-M87/Kinetra#17 adds `WeightedGrid::search` for Dotio's shelter search and was merged as Kinetra `main` `55e690e`. Lab `main` records `3622849`, the branch commit of that pull request, because the Dotio stack was merged before the pointer moved. This branch records `55e690e`, as [AGENTS.md](../../AGENTS.md#git-with-the-engine-submodule) asks when the Lab pull request was merged first.

## Table of contents

- [Decisions](#decisions)
- [Verification](#verification)
- [Pull request](#pull-request)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Pointer | `55e690e`, Kinetra `main` | `3622849` is its second parent, so no engine commit drops out, and Lab `main` then records a commit on Kinetra `main`'s first-parent line |
| Verification | The verification of the Dotio stack stands; this pull request's CI | `git -C external/Kinetra diff --stat 3622849 55e690e` is empty: the engine tree is identical |

## Verification

- `git -C external/Kinetra diff --stat 3622849 55e690e`: empty.
- `git -C external/Kinetra merge-base --is-ancestor 3622849 55e690e`: true.
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.
- `ci`, `headless` and native Windows: this pull request's CI.

## Pull request

**build(deps): record Kinetra main after the weighted grid search**

Records `55e690e` (merge commit of SMB-M87/Kinetra#17, `WeightedGrid::search`) instead of its branch commit `3622849`. The engine trees are identical, so the Dotio stack's verification stands. No Lab code change.
