# Shift+right-click inspects in i4sim without holding

Status: implemented · Branch: `ccr-ddbf0b01-gbkylj` (set by the session environment) · Related: [Dotio agent inspector](2026-11-22-dotio-agent-inspector.md), [i4sim controls](../../labs/i4sim/README.md#controls)

The user tried the Windows release zip and reported that in i4sim, Shift+right-click on a mover opens the inspector but also holds the mover. Shift+right-click is Dotio's inspect gesture, so the same hand movement in i4sim should inspect and nothing else. This change makes Shift turn off the hold and block of a right click.

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

`FactoryController::handle_pointer` (`labs/i4sim/src/controller.cpp`) reads Shift only for a left click, which sends the selected mover to a goal. Every right click, with or without Shift, goes to `pick(point, true)`, which selects the mover or producer under the pointer and toggles its hold or block. The [i4sim README](../../labs/i4sim/README.md#controls) documents right-click as "inspect and also hold", but Dotio's [README](../../labs/dotio/README.md) teaches Shift+right-click as "inspect the enemy under the pointer". A user who inspects a mover that way stops it, and has to right-click it again to release it.

A reproduction case, written before the fix, failed on all three points: Shift+right-click held mover 1, blocked producer 0, and kept the producer selected on empty floor.

## Goals and non-goals

Goals:

- Shift+right-click on a mover or producer selects it, as a left click does, and changes nothing in the simulation.
- The other pointer bindings stay as they are: left click selects, right-click selects and holds or blocks, Shift+left-click sends the selected mover.

Non-goals:

- One pointer scheme for every lab. Demo clears its selection with right-click and Dotio fires with left-click, so the labs keep their own bindings; only this gesture, which already means "inspect" in Dotio, is aligned.
- The help lines in the settings panel.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| What Shift+right-click does | It selects exactly as a left click does: the mover or producer under the pointer, and on empty floor it clears the producer selection. It holds and blocks nothing, and sets no goal | The gesture means "inspect" in Dotio; reusing the left click's `pick(point, false)` keeps one selection rule instead of a third variant |
| Plain right-click | Unchanged: it selects and holds or blocks | It is documented and tested, and the user reported only the Shift case |
| How Shift is read | `input.down(Key::shift)`, the check the Shift+left-click goal already uses | Both Shift gestures in i4sim decide the same way. Dotio also accepts `hit`, which differs only when Shift goes down and up within one frame |
| Settings panel help | The SIM CONTROLS lines stay `CLICK SELECT / RIGHT BLOCK` and `SHIFT CLICK FREE MOVER GOAL` | The panel has four short lines per column; the README's controls table is the reference, and a new line would change the panel's layout for a gesture the user already uses |

## Design

`handle_pointer` reads Shift once. A Shift+left-click with a selected mover still sends the goal. Every other click goes to `pick`, whose `toggle` argument is now `input.right_clicked && !shift`: a right click without Shift toggles the hold or block as before, and a right click with Shift selects as a left click does.

## Affected areas

| Area | Change |
| --- | --- |
| `labs/i4sim/src/controller.cpp` | `handle_pointer` passes no toggle for Shift+right-click |
| `labs/i4sim/include/i4sim/controller.h` | The `handle_input` contract names the Shift+right-click case |
| `labs/i4sim/tests/controller_tests.cpp` | A case for Shift+right-click on a mover, a producer and empty floor |
| `labs/i4sim/README.md`, `labs/i4sim/tests/README.md` | The controls table and the test overview's Pointer row |

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Shift+right-click selects a mover without holding it, a producer without blocking it and keeping the mover, and on empty floor clears the producer and sets no goal | New `kinetra_i4sim_controller:a shift right click selects like a left click and holds or blocks nothing`, run failing before the fix | Targeted |
| A right click without Shift still holds and blocks | Existing `a right click holds a mover or blocks a producer and keeps the selection on empty floor` | Targeted |
| Shift+left-click still sends the selected mover | Existing `shift-click sends the selected mover and a rejected goal shows a message for four seconds` | Targeted |
| The rest of i4sim is unchanged | `ctest -R '^kinetra_i4sim'` | Targeted, then Full through the pull request's CI |

## Review

`/code-review` at `low` on `origin/main...HEAD` reported no findings. The own review against the [workflow checklist](../../external/Kinetra/docs/workflow.md#6-review) found nothing either: a Shift+left-click with a selected mover still sends its goal before any pick, and both clicks in one frame behave as before. The copies check (`--term 'right click' --term 'right-click' --term 'RIGHT BLOCK' --term 'holds or blocks' --term 'hold the mover'`) found only lines that stay true: the plain right click in the i4sim README and the test overview, Dotio's README, and this document's index row.

## Verification

| Command | Result |
| --- | --- |
| `cmake --preset dev`, `cmake --build --preset dev --target kinetra_i4sim_controller_tests`, `ctest --preset dev -R '^kinetra_i4sim_controller'` before the fix | The new case failed on the hold, the block and the producer selection on empty floor; 42 others passed |
| The same after the fix | 43 of 43 passed, no warnings in the changed files |
| `cmake --build --preset release --target kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim`, `ctest --preset release -R '^kinetra_i4sim'` | 146 of 146 passed |
| `clang-format` 18 on the changed C++ files | Formatted; CI's pinned version decides |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |
| Full level | The pull request's CI |

## Pull request

**Title** `fix(i4sim): Shift+right-click selects without holding or blocking`

**Description**

### Summary

Feedback on the release zip: in i4sim, Shift+right-click on a mover opened the inspector but also held the mover. Shift+right-click is Dotio's inspect gesture, so in i4sim it now selects a mover or producer as a left click does and holds or blocks nothing. A plain right click still selects and holds or blocks.

### Changes

- `FactoryController::handle_pointer` passes no hold toggle for a right click with Shift held.
- A controller case for Shift+right-click on a mover, a producer and empty floor, run failing before the fix.
- The `handle_input` contract, the i4sim controls table and the test overview name the gesture.

### Design

This document, `docs/design/2026-11-23-i4sim-shift-right-click.md`. Shift is read with `input.down`, as for the Shift+left-click goal; the help lines in the settings panel stay as they are.

### Verification

- Before the fix the new case failed on the hold, the block and the producer selection; after it, `ctest --preset dev -R '^kinetra_i4sim_controller'` passes 43 of 43.
- `ctest --preset release -R '^kinetra_i4sim'`: 146 of 146.
- `check_docs.py` passes. The Full level is this pull request's CI.

### Review notes

`/code-review` at `low` found nothing; the copies check found only lines that stay true.

### Dependencies and merge order

None; the Kinetra pointer is unchanged.

### Follow-up

None.

## Open points

None.
