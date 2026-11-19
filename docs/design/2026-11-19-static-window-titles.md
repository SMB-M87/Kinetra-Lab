# Static window titles

Status: implemented · Branch: `feat/static-window-titles` · Related: [Kinetra design](../../external/Kinetra/docs/design/2026-11-19-static-window-titles.md), [launcher and overlays](../launcher-overlays.md)

The window title of every Lab application is a status line: "i4sim | 1680x960+ | RUNNING", or Demo and Dotio titles of about a hundred characters with counters and key hints. The user wants the title to name only what runs: "Launcher" in the menu, "Demo", "Dotio" or "i4sim" in a session. The engine half (menu title, `Session::title()` contract, title on the first frame) is in the Kinetra design document.

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

- Session titles carry state: `sandbox::window_title` in [sandbox_renderer.cpp](../../labs/demo/src/sandbox_renderer.cpp) (agents, pause, chunks, camera, paths, avoidance, last action, key hints), `dotio::window_title` in [presentation.cpp](../../labs/dotio/src/presentation.cpp) (difficulty, pause, health, mana, kills, modes) and `FactorySession::title()` in [session.cpp](../../labs/i4sim/src/session.cpp) (blueprint name, PAUSED or RUNNING).
- The direct executables open with "Kinetra" (Demo) and "Dotio on Kinetra" (Dotio); the launcher opens with "Kinetra" and switches to "Kinetra | Start menu".
- About twenty graphics-test assertions read state from `title()`: pause state in [launcher/tests/runtime_tests.cpp](../../launcher/tests/runtime_tests.cpp) and [labs/i4sim/tests/presentation_tests.cpp](../../labs/i4sim/tests/presentation_tests.cpp), the Dotio difficulty, the i4sim loadscreen and catalog error, and Demo's camera mode and last action.

## Goals and non-goals

- Goal: the title is "Launcher", "Demo", "Dotio" or "i4sim" from the first frame on, in the launcher and in the direct executables.
- Goal: the graphics tests keep checking the same state.
- Non-goal: showing the dropped details in the window. The user chose to drop them; most are visible already (i4sim HUD, Dotio bars, pause overlays).

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Title text | "Launcher" in the menu; "Demo", "Dotio", "i4sim" in a session | User's request |
| Title-only details | Dropped, not moved to a HUD | User's choice |
| How tests observe state | `lab::Session::status()`: the former title text without the application name and key hints | Keeps every state assertion with a one-word change. Rejected: typed accessors per session (the sessions are file-local classes behind `lab::Session`, so tests would need new public types); dropping the assertions (weakens the tests) |
| Key hints in the title ("CTRL settings", "WASD/arrows move …") | Removed with their test | They were only ever in the title; the settings overlay lists the controls |

## Design

```mermaid
classDiagram
    class `kinetra::Session` {
        +title() string
    }
    class `lab::Session` {
        +close_on_escape() false
        +status() string*
    }
    `kinetra::Session` <|-- `lab::Session`
    `lab::Session` <|-- DemoSession : title "Demo", status sandbox::status_text
    `lab::Session` <|-- DotioSession : title "Dotio", status dotio::status_text
    `lab::Session` <|-- FactorySession : title "i4sim", status blueprint and pause
```

*Takeaway:* the host reads only `title()`; `status()` exists for tests and diagnostics and never reaches the screen.

- `sandbox::window_title` and `dotio::window_title` become `status_text` without the "Kinetra | <app> |" prefix and the key hints.
- The direct executables construct their `Runtime` with the application's name, and the launcher with `kinetra::launcher_window_title`.

## Affected areas

- [labs/common/include/lab/session.h](../../labs/common/include/lab/session.h): pure virtual `status()`.
- Demo: [session.cpp](../../labs/demo/src/session.cpp), [sandbox_renderer.h](../../labs/demo/include/sandbox_renderer.h), [sandbox_renderer.cpp](../../labs/demo/src/sandbox_renderer.cpp), [main.cpp](../../labs/demo/src/main.cpp).
- Dotio: [session.cpp](../../labs/dotio/src/session.cpp), [presentation.h](../../labs/dotio/include/presentation.h), [presentation.cpp](../../labs/dotio/src/presentation.cpp), [main.cpp](../../labs/dotio/src/main.cpp).
- i4sim: [session.cpp](../../labs/i4sim/src/session.cpp).
- Launcher: [main.cpp](../../launcher/src/main.cpp).
- Tests: [launcher runtime tests](../../launcher/tests/runtime_tests.cpp), [i4sim presentation tests](../../labs/i4sim/tests/presentation_tests.cpp), [Demo presentation tests](../../labs/demo/tests/presentation_tests.cpp).
- Docs: [launcher and overlays](../launcher-overlays.md), [launcher README](../../launcher/README.md).
- `external/Kinetra` pointer.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Each launcher session titles the window with its name | `kinetra_graphics:every launch creates a clean session with its own camera` checks `title()` for Demo, Dotio and i4sim | Graphics |
| Pause, difficulty, loadscreen, camera mode and teleport are still observable | The existing assertions, now on `status()` | Graphics |
| The launcher titles the window "Launcher" or the session name from the first frame | Kinetra `kinetra_engine_graphics:run_launcher titles the window …` | Kinetra graphics |

The change touches presentation, sessions, `main.cpp` and the Kinetra pointer, so the level is Full with graphics.

## Review

Self-review of `git diff main...HEAD`:

- The i4sim title used to show a catalog or blueprint error. Checked: the loadscreen draws the error itself (`loadscreen_->draw(runtime, error_)`) and a running factory draws it under the HUD, so nothing disappears from view.
- `status()` is pure virtual, so a new Lab session cannot forget it; the test-only `LoopSession` returns an empty string.
- The launcher tests reach `status()` through `dynamic_cast<lab::Session &>`, which throws if a registration ever returns a non-Lab session. That is the intended failure.
- The removed Demo assertion checked the key hint in the title, which no longer exists; no other assertion was weakened.
- No new warnings in the changed lines (`KINETRA_LAB_WARNINGS` output of the `ci` build).

## Verification

| Command | Result |
| --- | --- |
| `cmake --workflow --preset ci` | 140/140 passed |
| `cmake --workflow --preset headless` | 133/133 passed. The first run failed in the build: the i4sim executables copied the blueprints into the same directory in parallel. Unrelated to this change and fixed on `main` by [the blueprint copy race fix](2026-11-19-blueprint-copy-race.md), which this branch merged |
| `cmake --workflow --preset graphics` | 152/152 passed |
| `kinetra --variant demo\|dotio\|i4sim --frames 120` and the menu for 60 frames (offscreen, software GL) | All exit 0 |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |
| Kinetra: `cmake --workflow --preset ci`, graphics suite | 215/215; 223/224 with one failure that also occurs on `main` in WSLg (see the Kinetra document) |

## Pull request

Title: `feat(labs): title the window with the application name only`

**Summary.** The window title is "Launcher" in the menu and "Demo", "Dotio" or "i4sim" while an application runs. The state the titles carried is dropped from the screen and kept for tests in `lab::Session::status()`.

**Changes.**
- `labs/common`: `lab::Session::status()`.
- Demo and Dotio: `window_title` becomes `status_text` without prefix and key hints; sessions and `main.cpp` use the application name.
- i4sim: `title()` is "i4sim", the loadscreen and pause state move to `status()`.
- Launcher: window opens as "Launcher".
- Tests read state from `status()` and check each session's title.
- Docs: launcher and overlays, launcher README.
- `external/Kinetra` pointer.

**Design.** [docs/design/2026-11-19-static-window-titles.md](2026-11-19-static-window-titles.md), Kinetra: [static window titles](../../external/Kinetra/docs/design/2026-11-19-static-window-titles.md).

**Verification.** `ci` 140/140, `headless` 133/133 (one copy race on the first run, since fixed on `main`), `graphics` 152/152, smoke runs of every variant and the menu, docs check. Kinetra `ci` 215/215.

**Review notes.** Title text is user-visible; the dropped details (agent count, kills, key hints) were a user decision.

**Dependencies and merge order.** Kinetra pull request first, with a merge commit; this branch records its commit.

**Follow-up.** None.

## Open points

None.
