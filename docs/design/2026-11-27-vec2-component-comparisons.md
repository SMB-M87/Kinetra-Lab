# Vec2 equality in place of component comparisons

Status: accepted · Branch: `refactor/vec2-component-comparisons` · Related: [Kinetra's equality operator](https://github.com/SMB-M87/Kinetra/pull/52), [engine adoption](https://github.com/SMB-M87/Kinetra/pull/60)

Use Kinetra's exact vector equality in Demo, i4sim and the capture tool wherever both coordinates already have to match.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)

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

Compare whole vectors when identifying an agent's home, detecting changes to a presented position or asserting exact positions in tests. Keep assertions about an individual axis separate.

## Affected areas

- [Demo](../../labs/demo/README.md) agent selection, inspector, pose interpolation and tests.
- [i4sim](../../labs/i4sim/README.md) simulation and controller tests.
- [Capture](../../tools/capture/README.md) pointer-path tests.

## Test plan

Run the existing Demo application, terrain and crowd cases, i4sim simulation, golden, pressure and controller cases, and the capture tests. The existing cases exercise agent selection, deterministic terrain, route targets, camera fitting and pointer-path endpoints and seed reproducibility. Run the capture smoke test with software OpenGL and check the docs and formatting.
