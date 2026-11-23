# Demo help overlay removal

Status: implemented · Branch: `refactor/demo-help-overlay` · Related: [Demo](../../labs/demo/README.md), [roadmap decisions](2026-11-21-roadmap-priorities.md#decisions)

Demo presents its controls in Settings. Its earlier help overlay has no application entry point.

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

The renderer includes `help_overlay.h` without calling the overlay. The command adapter never sets `toggle_help`. Both the drawing wrapper and the `PresentationOptions` flag are exercised only by tests.

## Goals and non-goals

Remove the unused drawing wrapper, its build entry, and the command and state flag. Keep Demo's SIM CONTROLS and DEBUG CONTROLS in Settings, along with its pause, route and steering-probe coverage.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Demo help | Use the existing Settings panel sections | The user approved removing the old overlay on 2026-11-22 |

## Design

Delete Demo's overlay source and header, its renderer include and the orphaned command and flag. Keep the pause and drawing assertions in the tests that also exercised the old overlay.

## Affected areas

Demo logic, presentation, build definitions, tests and module documentation.

## Test plan

Run the existing Demo logic, terrain, crowd characterization and graphics suites and the Demo smoke tests. Compare seeded captures before and after the change. CI supplies the Full level.

## Review

Checked the session's Settings rows and the route, steering-probe and pause assertions. The copies check still finds references to Kinetra's shared `draw_help` function and `Runtime`, which this change leaves available.

## Verification

- `ctest --preset graphics -R '^kinetra_demo|^kinetra_smoke:demo$' -j 5 --output-on-failure` passed all 70 checks, including the five crowd soak cases and the Demo smoke run.
- `kinetra_capture` produced byte-identical `demo.png` and `demo-debug.png` before and after the change.
- Documentation links, copies, formatting and `git diff --check` passed. Pull request CI supplies the Full level.

## Pull request

`refactor(demo): remove the unused help overlay`

### Summary

Remove Demo's unused help overlay, command and presentation flag. Demo's controls remain available in Settings.

### Changes

Remove the overlay's implementation and its tests. Keep the pause, route and steering-probe assertions.

### Design

[Demo help overlay removal](2026-11-23-demo-help-overlay-removal.md)

### Verification

70 Demo checks passed locally. The two seeded Demo screenshots match byte for byte. Docs and formatting checks passed.

### Review notes

The copies check found valid references to shared engine APIs. The Demo scenario menu is a separate roadmap item.

### Dependencies and merge order

None.

### Follow-up

None.
