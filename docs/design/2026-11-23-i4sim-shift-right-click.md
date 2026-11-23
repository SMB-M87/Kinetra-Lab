# Shift+right-click inspects in i4sim without holding

Status: accepted · Branch: `ccr-ddbf0b01-gbkylj` (set by the session environment) · Related: [Dotio agent inspector](2026-11-22-dotio-agent-inspector.md), [i4sim controls](../../labs/i4sim/README.md#controls)

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

To be filled in after the review.

## Verification

To be filled in after verification.

## Pull request

To be filled in before the push.

## Open points

None.
