# Record Kinetra main after the include layout

Status: implemented · Branch: `build/record-kinetra-main` · Related: [Include layout](2026-11-20-include-layout.md), [External dependencies](../../external/README.md)

Kinetra-Lab#34 was merged while it recorded `265ef02`, the last commit of Kinetra's `refactor/include-layout` branch. Kinetra `main` is its merge commit `92db1b6` (SMB-M87/Kinetra#14), with an identical tree. This branch records `92db1b6`, as [AGENTS.md](../../AGENTS.md#git-with-the-engine-submodule) asks when the Lab pull request merges first.

## Table of contents

- [Decisions](#decisions)
- [Verification](#verification)
- [Pull request](#pull-request)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Pointer | `92db1b6`, Kinetra `main` | `265ef02` is reachable but not on `main`, so the submodule on `main` shows as modified |
| Verification | No new build | `git -C external/Kinetra diff --stat 265ef02 92db1b6` is empty, and `265ef02` is an ancestor of `main`; the verification of Kinetra-Lab#34 stands |

## Verification

- `git -C external/Kinetra merge-base --is-ancestor 265ef02 92db1b6`: true.
- `git -C external/Kinetra diff --stat 265ef02 92db1b6`: empty.
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.

## Pull request

**build(deps): record Kinetra main after the include layout**

Records `92db1b6` (merge commit of SMB-M87/Kinetra#14) instead of its branch commit `265ef02`; the trees are identical. No code change.
