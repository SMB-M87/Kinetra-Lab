# H crossing without stations

Status: implemented · Branch: `fix/i4sim-h-crossing-producers` · Related: [i4sim path planning off and a smooth H crossing](2026-11-19-i4sim-h-crossing-motion.md), [showcase scenes](../../labs/i4sim/blueprints/README.md#showcase-scenes)

The H crossing showcase scene still carried the four stations of the reference's `H` layout. The scene runs without workload, so the stations never take part; they only cover the bottom of the view and the corners of the hall.

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

- [h-crossing.json](../../labs/i4sim/blueprints/showcase/h-crossing.json) lists Kuka, Staubli, Viper and Manuel. Their bodies stand below the hall, their processing places in the four corners, under the movers.
- The scenario sets `"workload": false`, so no product ever visits them. The other four showcase scenes have no stations.
- The H scene was the only place that loaded a station outside the floor, which the blueprint format allows up to twice the floor size.

## Goals and non-goals

- Goal: the H crossing shows only the hall and its four movers, in the application and in its GIF.
- Goal: the blueprint format keeps accepting stations outside the floor, now pinned by its own test.
- Non-goal: changing the bundled reference layouts or the blueprint format.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Remove or hide | Remove the stations from the scene file (`"producers": []`) | User's request ("kunnen die weg?"); the other showcase scenes do the same |
| Stations outside the floor | Keep accepting them; add a blueprint test | The rule exists for the reference's `H` layout, and an external blueprint may still use it |

## Design

`h-crossing.json` gets an empty `producers` list; floor, movers, forbidden zones and scenario stay as they were. The comment in `UnitReader::add_producer` now names the reference's layout instead of the showcase file.

## Affected areas

- [h-crossing.json](../../labs/i4sim/blueprints/showcase/h-crossing.json), [blueprint.cpp](../../labs/i4sim/src/blueprint/blueprint.cpp) (comment only).
- [simulation_tests.cpp](../../labs/i4sim/tests/simulation_tests.cpp): new blueprint test.
- [i4sim-showcase-h-crossing-clip.gif](../media/i4sim-showcase-h-crossing-clip.gif), [i4sim README](../../labs/i4sim/README.md), [blueprints README](../../labs/i4sim/blueprints/README.md).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| The H scene still swaps its corners without contacts, with path planning off | Existing `kinetra_i4sim` H cases, golden and capture cycle tests | Targeted |
| A station below the floor loads; one beyond twice the floor is rejected | `kinetra_i4sim: a station may stand outside the floor up to twice its size, with its processing place inside` | Targeted |
| The clip shows no stations | Re-render with `capture_media.sh`, look at frames | Manual |

## Review

The diff was read in full: the JSON still parses and keeps its formatting, the movers and the cycle are unchanged (the clip's frames match the old ones apart from the stations), and no other file references the removed stations.

## Verification

- `cmake --build --preset release` for the i4sim targets, then `ctest --preset release -R '^kinetra_i4sim'`: 128/128 passed.
- `ctest --preset graphics -R '^kinetra_capture|^kinetra_i4sim'` (includes the i4sim presentation tests and the capture cycle tests): 151/151 passed.
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.
- `tools/capture/capture_media.sh build-graphics i4sim-showcase-h-crossing-clip`, frames 30 and 120 viewed.

## Pull request

**fix(i4sim): remove the unused stations from the H crossing scene**

The H crossing showcase scene carried the four stations of the reference's `H` layout, although it runs without workload. They covered the bottom of the view and the hall's corners. The scene file now has no stations, like the other showcase scenes, and its GIF is re-rendered.

- `h-crossing.json`: `"producers": []`.
- New blueprint test: a station may stand outside the floor up to twice its size, so that rule keeps a test now that no bundled scene uses it.
- Docs: i4sim and blueprints READMEs, design document.

Verification: i4sim suites 128/128 (release), i4sim and capture suites with graphics 151/151, docs check clean.
