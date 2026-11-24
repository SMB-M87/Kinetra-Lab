# The labs in GIFs, and back to the configuration page

Status: accepted · Branch: `claude/roadmap-readme-media-demo-p1ywc6`, Kinetra `claude/roadmap-readme-media-demo-p1ywc6` · Related: [roadmap](../roadmap.md), [media guide](../media/README.md), [capture tool](../../tools/capture/README.md), [shared-UI GIFs](2026-11-23-shared-ui-gifs.md), [Demo scenario menu](2026-11-23-demo-scenario-menu.md)

This change finishes the roadmap item *README media as GIFs*. Lab's README gets a section per lab, with GIFs that show what each lab does and supports. The launcher screenshots leave the README, because Kinetra's README already shows the launcher. Demo and Dotio each get a debug-menu row that returns to their page of the launcher menu, as i4sim's BLUEPRINTS returns to its catalog. Doing that needs a new request in Kinetra, `SessionRequest::configurations`, and Kinetra's `ui-launcher` GIF now ends with that return.

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

- The README's gallery (`README.md`, *Screenshots and demo*) is one table of clips that are not explained per lab. Demo's Gravity, Maze and Doorway scenarios appear only as stills in the media guide, and i4sim's loadscreen appears only as a still. The roadmap item asks for a README about the labs themselves, *with as many GIFs as it takes to show how each lab works and what it supports* ([roadmap](../roadmap.md#next)).
- The README's last gallery row shows three launcher screenshots (`launcher.png`, `launcher-difficulty.png`, `launcher-loadscreen.png`). Kinetra's README shows the same menu as `ui-launcher.gif`, Demo's scenario page included.
- i4sim's debug menu ends with BLUEPRINTS, which returns to the i4sim catalog (`labs/i4sim/src/controller.cpp:477`). Demo and Dotio have no such row. Their configuration page belongs to the launcher, and `SessionRequest::menu` returns to the application list, because `LaunchMenu::update` leaves the configuration page once a configuration is chosen (`external/Kinetra/src/runtime/launcher.cpp:163-168`). To change scenario, the user has to open the settings panel, click LAUNCHER, and click DEMO again.

## Goals and non-goals

- Goal: Lab's README has a section per lab with its GIFs and what they show. The launcher row leaves the README.
- Goal: new GIFs for Demo's Gravity, Maze and Doorway scenarios, which replace their stills, and for i4sim's loadscreen with the pointer, which replaces `launcher-loadscreen.png`.
- Goal: Demo's debug menu gets SCENARIOS and Dotio's gets DIFFICULTY. Each returns to its application's page of the launcher menu, with the configuration that was running selected.
- Goal: Kinetra's `SessionRequest::configurations` asks the launcher for that page. Kinetra's `ui-launcher.gif` shows Demo's scenario page on the way in and on the way back.
- Non-goal: a size budget per GIF (the user's choice), and refreshing clips that this change does not make stale.
- Non-goal: refreshing Kinetra's `ui-debug-menu`, `ui-debug-layers` and `ui-inspector`, which will show the new SCENARIOS row once refreshed. Kinetra's media guide has them follow a Lab-only change to Demo with the next Kinetra change that makes them stale.
- Non-goal: the tour video (*Tour video for YouTube*).

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| How Demo returns to its scenario page | A new `SessionRequest::configurations` in Kinetra: `run_launcher` destroys the session and shows the configuration page of the application that ran, with its configuration selected | The user's choice on 2026-11-24. The page belongs to the launcher, so only the launcher can open it. Rejected: `SessionRequest::menu`, which costs one more click on DEMO, and a scenario list in the debug menu, which does not return to the menu |
| Dotio | Gets the same row, DIFFICULTY, in this change | The user's choice on 2026-11-24 (the agent recommended Demo alone, with Dotio on the roadmap). The request serves every application with configurations |
| New Lab GIFs | `demo-gravity-clip`, `demo-maze-clip`, `demo-doorway-clip` replace the three stills; `i4sim-loadscreen-clip`, with the pointer, replaces `launcher-loadscreen.png` | The user's choice on 2026-11-24. A clip shows what a scenario demonstrates, which a still cannot |
| Size budget | None | The user's choice on 2026-11-24. The agent recommended at most 1 MB per new GIF |
| README structure | Under the tour's thumbnail, a section per lab: what it is and supports, its normal and debug clips side by side, then a table of its scenarios or showcase scenes, and a link to its guide. Build and architecture follow below | The user's choice on 2026-11-24: the README is about the labs |
| The return in Kinetra's README | `ui-launcher` continues after Doorway starts: Shift+Ctrl, SCENARIOS, and back on the scenario page with DOORWAY selected | The user's choice on 2026-11-24. The engine change is a launcher change, so `ui-launcher` is stale anyway |
| Row labels and place | SCENARIOS in Demo, DIFFICULTY in Dotio, both last in the menu, as BLUEPRINTS is in i4sim | Confirmed with the plan on 2026-11-24 |
| Direct start without a launcher | `run_session` ends the run on `configurations`, as it does on `menu`. In `kinetra_demo` and `kinetra_dotio` the new row ends the program, as LAUNCHER already does | There is no menu to return to. This is consistent with LAUNCHER |
| The launcher screenshots | The files `launcher.png`, `launcher-difficulty.png` and `launcher-demo-scenarios.png` stay and are linked from the module READMEs. `launcher-loadscreen.png` and the three Demo scenario stills leave with their scenes | Clips replace these four stills. The other three still show a page that the README no longer shows |

## Design

```mermaid
sequenceDiagram
    participant Row as SCENARIOS row (Demo logic)
    participant Session as DemoSession
    participant Loop as run_launcher
    participant Host as SessionHost
    participant Menu as LaunchMenu
    Row->>Session: scenarios_requested() becomes true
    Loop->>Session: request()
    Session-->>Loop: SessionRequest::configurations
    Loop->>Host: selection(), then return_to_menu()
    Loop->>Menu: show_configurations(selection)
    Loop->>Menu: draw(): DEMO's page, the scenario that ran selected
```

*Takeaway:* the row only sets a flag in the logic layer; the session turns it into a request, and the launcher alone knows the page.

**Kinetra.**
- `SessionRequest` gets `configurations`, between `menu` and `exit`.
- `LaunchMenu::show_configurations(LaunchSelection)` shows the configuration page of that application with that configuration selected. For an application without configurations, it shows the application list with that application selected. It throws `std::out_of_range` for an application or configuration that does not exist.
- `SessionHost::selection()` returns the application and configuration of the running session, or `std::nullopt` in the menu.
- `run_launcher` treats `configurations` like `menu` and then calls `show_configurations` with the selection of the session that ran.
- `run_session` already ends on any request.
- The capture tour's loop, which copies `run_launcher`, does the same.

**Lab.**
- Demo's `DebugPanel` and Dotio's `Application` add their row last, after RESET PARAMETERS. The row sets a flag that the session reads in `request()`: `configurations` while it is set, otherwise the settings panel's request.
- Both rows are logic, tested headless, as i4sim's `FactoryController` is.

**Captures.**
- The three Demo scenario clips start their scenario as the stills did. The Maze clip clicks a goal beyond the maze, and the clip lasts until the player has walked the route.
- `i4sim-loadscreen-clip` runs in a `Tour`, as the UI clips do: the launcher's I4SIM row, unrecorded, then the catalog with hover on its rows, SHOWCASE, a scene that starts with Space, the debug menu's BLUEPRINTS, and back on the catalog.
- `ui-launcher` continues after DOORWAY: Shift+Ctrl, SCENARIOS, and the scenario page with DOORWAY selected.
- A scene's gallery decides where its file goes, so `i4sim-loadscreen-clip` belongs to the Lab gallery.

**Stale captures.** By the media guides' tables:
- Demo's debug menu: `demo-debug` (the open menu shows the new last row). `demo-debug-clip` has the menu closed.
- Dotio's debug menu: `dotio-debug`.
- The launcher: Kinetra's `ui-launcher`.
- New: the four clips above.

## Affected areas

- Kinetra:
  - `include/kinetra/runtime/session.h`, `include/kinetra/runtime/launcher.h`, `src/runtime/launcher.cpp`, `src/runtime/session.cpp` (comment).
  - `tests/launcher_tests.cpp`, `tests/runtime_tests.cpp`.
  - `docs/engine/launcher-overlays.md`, `src/runtime/README.md`, `README.md` (shared UI row), `docs/media/README.md`, `docs/media/ui-launcher.gif`.
- Lab:
  - `labs/demo/` (`debug_panel.{h,cpp}`, `presentation/session.cpp`, tests, README).
  - `labs/dotio/` (`application.h`, `presentation/session.cpp`, tests, README).
  - `tools/capture/` (scenes, UI clips, tour loop, tests, README).
  - `docs/media/` (new clips, refreshed `demo-debug.png` and `dotio-debug.png`, four stills removed, guide).
  - `README.md`, `docs/launcher-overlays.md`, `launcher/README.md`, `labs/i4sim/README.md`, `docs/roadmap.md`, this document and the design index.
  - The `external/Kinetra` pointer.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| `show_configurations` opens the page with the configuration selected; an application without configurations shows the list with it selected; unknown indices throw and change nothing | New case in Kinetra's `launcher_tests.cpp` (headless) | Targeted in Kinetra |
| `SessionHost::selection()` follows `start()` and `return_to_menu()`, and stays unchanged when a factory throws | The SessionHost case in `launcher_tests.cpp` | Targeted in Kinetra |
| `run_launcher` destroys the session on `configurations`, draws the menu in that frame, and the next Enter starts the same configuration again | New graphics case in Kinetra's `runtime_tests.cpp` with a scripted application that has configurations | Full (graphics) |
| `run_session` ends on `configurations` | The existing request case, extended with the new value | Full (graphics) |
| Demo's SCENARIOS row is last and sets the flag; nothing else changes | New case in `kinetra_demo_tests` | Targeted |
| Dotio's DIFFICULTY row is last and sets the flag | New case in `kinetra_dotio_tests` | Targeted |
| A Demo session and a Dotio session answer `configurations` after the row's action | Graphics cases in the launcher's `runtime_tests.cpp`, next to the existing LAUNCHER and EXIT checks | Full (graphics) |
| The scene list matches the media guide and the README; the new clips render | `capture_tests.cpp` scene-list case; `kinetra_capture_smoke` | Targeted, Full |
| The GIFs show what their rows say | A contact sheet per new or refreshed GIF and PNG | Manual |

## Review

To be filled in.

## Verification

To be filled in.

## Pull request

To be filled in.

## Open points

- Once refreshed, Kinetra's `ui-debug-menu`, `ui-debug-layers` and `ui-inspector` will show Demo's new SCENARIOS row. They follow with the next Kinetra change that makes them stale, as Kinetra's media guide says.
