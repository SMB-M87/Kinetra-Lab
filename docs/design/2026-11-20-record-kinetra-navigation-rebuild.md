# Record Kinetra main after the navigation rebuild fix

Status: implemented · Branch: `build/record-kinetra-navigation-rebuild` · Related: [MAS demonstrator as a test fixture](2026-11-20-mas-test-fixture.md), [External dependencies](../../external/README.md)

SMB-M87/Kinetra#16 validates every navigation body before a rebuild changes state, and was merged as Kinetra `main` `7d3e586`. Lab records `92db1b6`. The user asked to record the new `main` in SMB-M87/Kinetra-Lab#42, but that pull request had already been merged. This branch therefore records `7d3e586` on its own, as [AGENTS.md](../../AGENTS.md#git-with-the-engine-submodule) asks.

## Table of contents

- [Decisions](#decisions)
- [Verification](#verification)
- [Pull request](#pull-request)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Pointer | `7d3e586`, Kinetra `main` | User's request. `92db1b6` is an ancestor, so no engine commit drops out, and no Kinetra pull request is open |
| Verification | Full level with graphics, locally; then this pull request's CI | The engine sources change (`navigation.cpp` and its tests). The local run used the tree of Lab `main` `f54a45e` with this pointer |

## Verification

- `git -C external/Kinetra merge-base --is-ancestor 92db1b6 7d3e586`: true. `git -C external/Kinetra diff --stat 92db1b6 7d3e586`: 7 files, `src/simulation/navigation.cpp` and `tests/navigation_tests.cpp` among them.
- Kinetra's own checks: SMB-M87/Kinetra#16 and the `main` run on `7d3e586` passed.
- `cmake --workflow --preset graphics`: 295/295 passed, 0 warnings.
- Smoke runs (offscreen, software Mesa): `kinetra --variant demo|i4sim|dotio --frames 120` exit 0; `kinetra_i4sim --blueprint labs/i4sim/tests/MAS-demonstrator.json --ticks 50000` completes 16 products, as before.
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.
- `ci`, `headless` and native Windows: this pull request's CI.

## Pull request

**build(deps): record Kinetra main after the navigation rebuild fix**

Records `7d3e586` (merge commit of SMB-M87/Kinetra#16, navigation bodies validated before a rebuild) instead of `92db1b6`. `92db1b6` is an ancestor. No Lab code change. Verified locally with `cmake --workflow --preset graphics` (295/295, 0 warnings) and the smoke runs; Kinetra#16 and Kinetra `main` passed their CI.
