# i4sim session fixes

Status: accepted · Branch: `fix/i4sim-session` · Related: [i4sim](../../labs/i4sim/README.md), [roadmap](../roadmap.md)

The November 22 roadmap decision groups four i4sim session bugs in one pull request.

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

The [controller](../../labs/i4sim/src/controller.cpp) reconstructs the simulation on `R` and loses its tick rate. RESET PARAMETERS carries its own horizon of eight ticks, while `MotionSettings` starts at 32. The controller skips its message countdown when no blueprint is loaded, and the [session](../../labs/i4sim/src/presentation/session.cpp) skips the controller's frame start entirely on the blueprint screen. The [direct executable](../../labs/i4sim/src/main.cpp) forwards `--output` to a graphical factory that has no use for it.

## Goals and non-goals

Preserve the active tick rate across a restart, restore the three numeric motion defaults, expire transient messages on either screen, and remove the unused graphical factory argument. The batch command continues to write summaries with `--output`.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Restart rate | Keep the live value, whether set through `--ups`, the UPS row or Q/E | The user asked to keep the rate on `R`. The automatic scenario restart already preserves the live rate |
| Motion reset | Read speed, force and horizon from `MotionSettings` defaults | The user chose the new simulation's 32-tick horizon |
| Message lifetime | Count down on every frame. Catalog and load errors stay until dismissed or replaced | The user asked for transient messages to age on the blueprint screen |
| Graphical output argument | Remove it from `create_session` | Summary export belongs to batch mode |

## Design

Move rate preservation into the restart shared by `R` and automatic scenario replay. Start every session frame by advancing the controller's message countdown and keep the blueprint screen's input routing. Set the numeric fields from a default-constructed `MotionSettings` and pass five arguments from the direct graphical executable.

## Affected areas

The controller, session and direct executable, their contracts and tests, and the i4sim module and architecture guides.

## Test plan

- Exercise `R` with a non-default startup rate and with a rate changed during the session. Keep the existing automatic replay check.
- Edit the MOTION values, reset them through the menu, and check that speed, force and horizon match the simulation defaults.
- Reject a goal, close the blueprint via BLUEPRINTS and verify the message expires on the blueprint screen. Keep the load-error persistence case.
- Check the exact type of the five-argument factory at compile time and run the batch executable with `--output`.
- Build the i4sim logic and presentation targets locally. The pull request's CI supplies Full verification.

## Review

Pending.

## Verification

Pending.

## Pull request

`fix(i4sim): preserve session rate and expire blueprint-screen messages`
