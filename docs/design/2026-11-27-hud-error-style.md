# HUD error style

Status: draft · Branch: `ccr-41f5211f-186dv3` (Lab), `feat/hud-error-text` (Kinetra) · Related: [Kinetra's HUD text and errors](../../external/Kinetra/docs/engine/launcher-overlays.md#hud-text-and-errors)

The user found i4sim's error messages too small and hard to read, and asked that an error in any application use the font and size of the statistics, in red, directly above them. Kinetra now holds that HUD style, and a panel for screens without HUD lines to spare; i4sim uses both, and Demo and Dotio take their statistics style from it.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Verification](#verification)
- [Review](#review)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

- i4sim drew its message at scale 0.8 under the toolbar (`labs/i4sim/src/presentation/hud.cpp`, `draw_text(runtime, content.message, {12, 98}, 0.8f, {1, 0.4f, 0.3f})`), while its statistics use scale 1.5 at the bottom. At 0.8 the 5 × 7 font is blurred over fractional pixels and hard to read.
- Every application picked its own HUD numbers: i4sim and Demo wrote `1.5f`, `{1, 1, 0}` and a 20-pixel line as literals, and Dotio its yellow. Nothing kept an error in one application like an error in another.
- The blueprint menu showed a load error through `LaunchMenu::draw(runtime, status)`: the first 80 characters at scale 1 or less, in orange. A load error starts with the blueprint's path, so the JSON error behind it was cut off. The load-error screen without a catalog used scale 1 and a pink tint.

## Goals and non-goals

Goals:

- One HUD style in Kinetra: the statistics' scale, line height, margin and yellow, and a red for errors.
- In i4sim, a message in that style, directly above the statistics, wrapped when it does not fit on one line.
- On the blueprint menu, a load error readable in full, in a panel that can use the whole window and closes with an X like the mover inspector's.
- Demo and Dotio draw their statistics from the same constants, so a future error there reads the same.

Non-goals:

- No error messages for Demo or Dotio: neither has one today.
- No change to what a message says or how long it stays (`FactoryController`).
- No change to the size of Demo's or Dotio's statistics: Dotio keeps its window-dependent scale.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| A message longer than a line | Wrap upward over up to four lines; the inspection and description move up | The user's choice; four lines hold a path with its parse error at a common width, and the full text stays on stderr and in the status |
| Where the shared style lives | Kinetra (`kinetra/runtime/hud_text.h`), not `labs/shared` | The user's choice: a HUD style has no game rules, and the launch menu, in Kinetra, needs it too |
| The loadscreens | Both take the style: the menu's error and the load-error screen | The user's choice |
| An error on the blueprint menu, where one HUD line fits below the menu | An error panel over the menu, at the bottom, as high as it needs up to the whole window, with an X | The user's choice, after seeing the one-line version cut the parse error off. The panel takes the pointer where it covers the menu; keys still reach the menu |
| Where the message sits when STATISTICS is off | Still on line 3 | The user asked for "above the statistics placeholder": the message does not jump when the statistics toggle |

## Design

**Kinetra.** [hud_text.h](../../external/Kinetra/include/kinetra/runtime/hud_text.h) is header-only: `text_scale` 1.5, `line_height` 20, `margin` 10, `statistics_color` yellow and `error_color` red; `line_top` and `lines_below` count lines from the bottom; `wrap` breaks at the last space that fits and cuts a longer word; `error_lines` caps the lines and ends a cut message with `...`; `draw_line` and `draw_error` draw. [error_view.h](../../external/Kinetra/include/kinetra/runtime/error_view.h) adds `error_panel::bounds`, `close_bounds`, `lines`, `update` and `draw`: the panel is as wide as the window less the margins, sits at the bottom, and uses the inspector's title bar (24 pixels) and close button (18 pixels). `LaunchMenu::update(input, viewport, error)` asks the panel first: over it the menu sees no pointer and no click; a click on the X sets `error_closed()`. `LaunchMenu::draw(runtime, error)` draws the panel last.

**i4sim.** `draw_hud` draws the statistics on lines 1 and 2, the message from line 3 up (at most four lines), then the inspection and, one line higher, the description; `hud_lines(message_lines)` computes those lines. `BlueprintScreen::update` passes the message to the menu and returns `ScreenAction::dismiss` for a click on the X, on which the session clears the message. The load-error screen without a catalog draws its two headings at the HUD scale and the error on its bottom lines.

**Demo and Dotio** read `hud::text_scale`, `hud::margin`, `hud::statistics_color` and `hud::draw_line` instead of literals, with the same pixels as before.

## Affected areas

- Kinetra: `include/kinetra/runtime/hud_text.h` and `error_view.h` (new), `src/runtime/error_view.cpp` (new), `launcher.h` and `launcher.cpp` (`LaunchMenu`), `CMakeLists.txt`, the tests in `presentation_tests.cpp` and `launcher_tests.cpp`, the [launcher contract](../../external/Kinetra/docs/engine/launcher-overlays.md), the runtime and tests READMEs, `README.md`, `AGENTS.md` (functionality index) and `docs/testing.md`.
- Lab: i4sim's `hud.cpp`, `hud.h`, `blueprint_screen.cpp`, `blueprint_screen.h`, `session.cpp` and `presentation_tests.cpp`; Demo's `renderer.cpp`; Dotio's `presentation.cpp`; i4sim's README and tests README; [launcher and overlays](../launcher-overlays.md).
- Media: no capture shows an error. The statistics of Demo, Dotio and i4sim and the launch menu without an error draw the same pixels, so no capture is stale.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| HUD lines count from the bottom; columns fit between the margins | `kinetra_presentation:HUD lines count up from the bottom of the viewport`, `HUD columns fit the width between the margins and are at least one` | Full (public header) |
| Wrapping at spaces, cutting long words, the cut mark | `kinetra_presentation:error text wraps at spaces and cuts words longer than a line`, `an error longer than its lines ends with the cut mark` | Full |
| The panel's size, position and cut at the window's height | `kinetra_launcher:the error panel grows with its message up to the whole window` | Full |
| The panel takes the pointer over the menu, its X closes, keys still work | `kinetra_launcher:an error panel over the menu takes the pointer and its close button reports the click` | Full |
| i4sim's HUD lines with and without a message | `kinetra_i4sim_graphics:the HUD message sits directly above the statistics and pushes the text above it up` | Full (graphics) |
| A load error on the blueprint menu and its X | `kinetra_i4sim_graphics:a load error shows in a panel over the blueprint menu that its X closes` | Full (graphics) |
| Demo, Dotio and i4sim render as before | The existing graphics suites and smoke tests | Full (graphics) |
| How it looks | Frames rendered from a temporary case: the HUD at 1280 × 720 and 420 × 480, the menu with a long parse error, the screen without a catalog | Looked at, not committed |

## Verification

To be filled in.

## Review

To be filled in.

## Pull request

To be filled in.

## Open points

- Demo and Dotio show no errors yet. When one does, it uses `hud::draw_error` on the line above its statistics.
