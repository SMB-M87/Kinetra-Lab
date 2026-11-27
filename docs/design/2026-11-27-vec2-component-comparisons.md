# Vec2 equality in place of component comparisons

Status: implemented · Branch: `refactor/vec2-component-comparisons` · Related: [Kinetra's equality operator](https://github.com/SMB-M87/Kinetra/pull/52), [engine adoption](https://github.com/SMB-M87/Kinetra/pull/60)

Use Kinetra's exact vector equality in Demo, i4sim and the capture tool wherever both coordinates already have to match.

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

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

Demo identifies agents by their two home coordinates and checks both coordinates of a pose before interpolating it. Demo and i4sim tests repeat the same comparison, and capture's pointer-path tests have their own equality helper.

## Goals and non-goals

Use `Vec2::operator==` and C++20's corresponding `!=` for existing exact coordinate pairs. Keep geometric distance, tolerance and single-axis checks as written.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Equality semantics | Use the existing Kinetra operator | It compares the same two floats exactly, including signed zeros and NaNs. |
| Test diagnostics | Capture coordinates where separate assertions printed them | Doctest has no formatter for `Vec2`. |

## Design

Compare whole vectors when identifying an agent's home, detecting changes to a presented position or asserting exact positions in tests. Compare the optional agent selections directly when deciding whether to reset the inspector. Keep assertions about an individual axis separate.

## Affected areas

- [Demo](../../labs/demo/README.md) agent selection, inspector, pose interpolation and tests.
- [i4sim](../../labs/i4sim/README.md) simulation and controller tests.
- [Capture](../../tools/capture/README.md) pointer-path tests.

## Test plan

Run the existing Demo application, terrain and crowd cases, i4sim simulation, golden, pressure and controller cases, and the capture tests. The existing cases exercise agent selection, deterministic terrain, route targets, camera fitting and pointer-path endpoints and seed reproducibility. Build the Demo presentation tests and executable, run the Demo graphics cases and the capture smoke test with software OpenGL, and check the docs and formatting.

## Review

The new comparisons preserve the float comparisons on both axes. Comparing the two optional selections also preserves the previous behavior for an empty selection. Camera assertion messages retain the actual and expected coordinates.

## Verification

- `cmake --build --preset dev --target` built the Demo, i4sim and capture targets listed under [Verification in Lab](../../AGENTS.md#verification-in-lab), plus `kinetra_demo` for the inspector trace and `kinetra_i4sim_motion_benchmark` for the benchmark CLI tests.
- `ctest --preset dev -E '^kinetra_demo_crowd:' -R '^kinetra_(demo(:|_terrain|_crowd|_passing|_inspector)|i4sim|capture)'` passed 270 cases. The three benchmark CLI cases passed on rerun with `ctest --preset dev -R '^kinetra_i4sim_benchmark_'` after building their executable. The five Demo soak cases are left to CI.
- `cmake --build --preset graphics --target kinetra_capture kinetra_demo kinetra_demo_presentation_tests` passed. `ctest --preset graphics -L graphics -R '^kinetra_demo_graphics:'` passed both Demo cases and `ctest --preset graphics -R '^kinetra_capture_smoke$'` passed with software OpenGL.
- `check_docs.py`, `check_copies.py` and clang-format 23.1.5 passed. The remaining copies describe Dotio's vector types and the two design records.

## Pull request

**Title** `refactor(lab): use Vec2 equality for exact comparisons`

### Summary

Use Kinetra's existing exact vector equality in Demo, i4sim and capture.

### Changes

- Compare agent homes, optional inspector selections and rendered positions directly in Demo.
- Use vector comparisons in the Demo, i4sim and capture tests that expect both coordinates to match exactly.
- Remove the completed item from the Lab roadmap.

### Design

See [Vec2 equality in place of component comparisons](https://github.com/SMB-M87/Kinetra-Lab/blob/refactor/vec2-component-comparisons/docs/design/2026-11-27-vec2-component-comparisons.md).

### Verification

The targeted development build and 273 Demo, i4sim and capture cases passed, including the three benchmark CLI cases after building their executable. Both Demo graphics cases and the capture smoke test passed on software OpenGL. Documentation and formatting checks passed.

### Review notes

The underlying float comparisons are exact. The camera tests still show their coordinates on failure.

### Dependencies and merge order

[Kinetra #52](https://github.com/SMB-M87/Kinetra/pull/52) is already included in Lab's engine revision.

### Follow-up

None.
