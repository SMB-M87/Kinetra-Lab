# i4sim session fixes

Status: implemented · Branch: `fix/i4sim-session` · Related: [i4sim](../../labs/i4sim/README.md), [roadmap](../roadmap.md)

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

The controller, session and direct executable, their contracts and tests, and the i4sim module and test guides.

## Test plan

- Exercise `R` with a non-default startup rate and with a rate changed during the session. Keep the existing automatic replay check.
- Edit the MOTION values, reset them through the menu, and check that speed, force and horizon match the simulation defaults.
- Reject a goal, close the blueprint via BLUEPRINTS and verify the message expires on the blueprint screen. Keep the load-error persistence case.
- Check the exact type of the five-argument factory at compile time and run the batch executable with `--output`.
- Build the i4sim logic and presentation targets locally. The pull request's CI supplies Full verification.

## Review

Checked the deferred BLUEPRINTS transition, the automatic restart's run state and rate, and the persistent load-error path. The documentation copies check found valid references to the existing controls, `Runtime`, and the remaining five-argument factory.

## Verification

- Before the fix, the restart case observed 200 instead of 360 ticks per second, the reset case observed an 8-tick horizon, and the blueprint screen kept the rejected-goal message after four seconds.
- Built the i4sim logic suites and executable with the `dev` preset. `ctest --preset dev -R '^kinetra_i4sim_controller:'` passed all 44 cases.
- Built the logic and presentation suites and executable with the `graphics` preset. `ctest --preset graphics -R '^kinetra_i4sim|^kinetra_smoke:i4sim' -j 4 --output-on-failure` passed all 153 checks, including the golden and pressure suites, graphics tests, and both smoke runs.
- `kinetra_i4sim --blueprint labs/i4sim/blueprints/showcase/corridor-passing.json --ticks 3 --output <temporary-directory>` wrote a valid `summary.json`.
- Documentation links, copies and changed-file formatting checked with `check_docs.py`, `check_copies.py` and clang-format 23.1.5.
- This cloud checkout needed the existing workspace toolchain activated before it could configure. clang-format 23.1.5 came from a temporary user-level install.

## Pull request

`fix(i4sim): correct restart and blueprint-screen behavior`

### Summary

`R` keeps the current tick rate, RESET PARAMETERS restores the simulation defaults, and a rejected-goal message expires on the blueprint screen. The unused output argument is removed from the graphical session factory. Batch mode still exports summaries through `--output`.

### Changes

- Preserve the rate for both manual and automatic restart, and retain the manual restart's paused state.
- Reset the three numeric settings from `MotionSettings`, including the 32-tick horizon.
- Advance the transient message timer on both screens and assert the reduced factory signature at compile time.

### Design

[i4sim session fixes](2026-11-23-i4sim-session-fixes.md)

### Verification

The controller suite passed 44 cases. The i4sim selection of the `graphics` preset passed 153 checks. The batch export, formatting, docs links and copies checks passed. Pull request CI supplies the Full level.

### Review notes

The BLUEPRINTS transition still starts accepting blueprint-screen input on the following frame. Load and catalog errors remain visible until replaced or dismissed.

### Dependencies and merge order

None.

### Follow-up

None.
