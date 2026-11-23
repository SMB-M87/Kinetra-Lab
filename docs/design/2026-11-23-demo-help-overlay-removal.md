# Demo help overlay removal

Status: accepted · Branch: `refactor/demo-help-overlay` · Related: [Demo](../../labs/demo/README.md), [roadmap decisions](2026-11-21-roadmap-priorities.md#decisions)

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

Pending.

## Verification

Pending.

## Pull request

`refactor(demo): remove the unused help overlay`
