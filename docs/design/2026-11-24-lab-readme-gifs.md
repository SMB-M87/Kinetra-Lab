# The labs in GIFs, and back to the configuration page

Status: implemented · Branch: `claude/roadmap-readme-media-demo-p1ywc6`, Kinetra `claude/roadmap-readme-media-demo-p1ywc6` ([Kinetra#53](https://github.com/SMB-M87/Kinetra/pull/53)) · Related: [roadmap](../roadmap.md), [media guide](../media/README.md), [capture tool](../../tools/capture/README.md), [shared-UI GIFs](2026-11-23-shared-ui-gifs.md), [Demo scenario menu](2026-11-23-demo-scenario-menu.md)

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
| ADR | None: `configurations` adds to [ADR 0014](../../external/Kinetra/docs/decisions/0014-session-navigation-requests.md) without changing its decision (one request, read by the host after every advance), and a value is easy to remove | The ADR criteria ask for a decision that is hard to reverse |
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
| `show_configurations` opens the page with the configuration selected; a pointer left over another row keeps the selection until it moves; an application without configurations shows the list with it selected; unknown indices throw and change nothing | New case in Kinetra's `launcher_tests.cpp` (headless) | Targeted in Kinetra |
| `SessionHost::selection()` follows `start()` and `return_to_menu()`, and stays unchanged when a factory throws | The SessionHost case in `launcher_tests.cpp` | Targeted in Kinetra |
| `run_launcher` destroys the session on `configurations`, draws the menu in that frame, and the next Enter starts the same configuration again | New graphics case in Kinetra's `runtime_tests.cpp` with a scripted application that has configurations | Full (graphics) |
| `run_session` ends on `configurations` | The existing request case, extended with the new value | Full (graphics) |
| Demo's SCENARIOS row is last and sets the flag; tuned values, speed and the ghost camera stay as they were | New case in `kinetra_demo_tests` | Targeted |
| Dotio's DIFFICULTY row is last and sets the flag; tuned balance, speed and the ghost camera stay | New case in `kinetra_dotio_tests` | Targeted |
| A Demo session and a Dotio session answer `configurations` after the row's action | Graphics cases in the launcher's `runtime_tests.cpp`, next to the existing LAUNCHER and EXIT checks | Full (graphics) |
| The scene list matches the media guide and the README; the new clips render | `capture_tests.cpp` scene-list case; `kinetra_capture_smoke` | Targeted, Full |
| With Lab's registrations, SCENARIOS returns to Demo's page with DOORWAY selected | `kinetra_capture_smoke` plays `ui-launcher`, whose script throws otherwise | Full (graphics in CI) |
| The GIFs show what their rows say | A contact sheet per new or refreshed GIF and PNG | Manual |

## Review

The agent reviewed its own work: `git diff origin/main...HEAD` in both repositories, the copies check, and `/code-review` at medium depth.

| Finding | Resolution |
| --- | --- |
| After `show_configurations` the menu's last pointer position was stale, so its first update hovered whatever row lay under a pointer the session had left there, replacing the configuration that ran | Kinetra 96c3ca7: the next update records the pointer without hovering; a test case rests the pointer on another row, then moves it |
| The loadscreen clip clicked absolute catalog rows without paging, never checked what loaded, and its hover loop underflowed for a catalog without floors | 9c0fcb5: `catalog_row` throws beyond the first page, `require_blueprint` checks the session's status after each click, and the clip throws for an empty catalog |
| The SCENARIOS and DIFFICULTY tests checked values that sat at their defaults, so a reset would have passed | 9c0fcb5: both first move speed, a tuning value and the ghost camera away from their defaults |
| The smoke test's header counted four clips; Kinetra's `run_session` case still said "menu or exit" | 9c0fcb5 and Kinetra 96c3ca7 |
| `show_configurations` and `SessionHost::start` each checked the configuration their own way | Kinetra c9ef1fd: one `known_configuration` |
| Copies: Kinetra's architecture diagram still said `alt menu or exit`; the capture README spoke of a "README gallery" | Kinetra ce10e8d; 171b84e |
| `ui-launcher` was checked only by looking at it | 171b84e: the smoke test plays it, and its script throws unless DOORWAY is selected on Demo's page |

The copies check (`check_copies.py`) ran with the old wordings `none, menu, exit`, `menu or exit`, `both requests`, `launcher-loadscreen`, the three scenario stills, `screenshots-and-demo`, `Screenshots and demo`, `README gallery`, `shared-UI clips`, `GHOST CAMERA and RESET PARAMETERS`, `eleven clips`, `5.1 MB` and `three launcher screenshots`. Every hit was judged: ADR 0014 keeps its decision text, and the remaining "shared-UI clips" name Kinetra's clips on purpose. Older design documents now link the new anchor and name the removed stills as text.

### Agent review, first round

*Fix first* on both pull requests, Kinetra at 96c3ca7 and Lab at 411474d, at level high.

| Finding | Resolution |
| --- | --- |
| Should fix, Kinetra: the `menu_key` contract (`launcher.h`, `launcher-overlays.md`) still named `SessionRequest::menu` as the only request that leaves a run when the key is unbound | Kinetra 82ca284: both name the session's own `menu` and `configurations` requests |
| Should fix, Lab: `docs/architecture.md` let a session leave only through its settings overlay, with no `configurations` branch in the session-flow diagram | The diagram's `else menu or configurations` branch, the takeaway and the sentence after it name the debug-menu rows; the *Add an application* bullet of `launcher/README.md` names `configurations` too |
| Nit, Lab: the Maze clip promised an end state that nothing checked | `demo_scenario_clip` throws when the click set no goal or the clip ends before the player arrives, through the new `Stage::demo()`; a 200-frame clip now stops with "The Maze clip ended before the player reached its goal" |
| Nit, Kinetra, pre-existing: after a plain `menu` request the application list still hovers the row under a pointer the session left there | Not in this change: *Open points* |
| Lesson: search each existing member of a set that a change extends | Kinetra c4630fa, in Kinetra's review lessons |

Lab `main` moved its pointer to b86a2ed (Kinetra-Lab#114, Kinetra#52) during the round, so this branch conflicted. As Lab's [lesson from #78](../review-lessons.md#submodule-and-merge-order) says, Kinetra `main` was merged into the Kinetra branch (5a30722), Lab `main` into this branch (12c3a32), and the pointer resolved to 5a30722. Then `ctest --preset graphics -LE soak` passed 347 of 347, and every capture of this change rendered byte for byte the same.

The copies check ran again with `SessionRequest::menu`, `settings overlay's request`, `LAUNCHER gives` and `request() == menu`. The remaining hits name `menu` for LAUNCHER alone, or stand next to their own `configurations` line.

## Verification

| Check | Command | Result |
| --- | --- | --- |
| Kinetra launcher and runtime, also after the review round | In `external/Kinetra`: `cmake --preset graphics -DKINETRA_WARNINGS=ON`, `cmake --build --preset graphics --target kinetra_launcher_tests kinetra_engine_runtime_tests`, `ctest --preset graphics -R 'kinetra_launcher\|kinetra_engine_graphics'` | 34 of 34 passed, no warnings. The new `run_launcher` case and the resting-pointer check each fail without their change |
| Lab, everything | `cmake --preset graphics -DKINETRA_LAB_WARNINGS=ON`, `cmake --build --preset graphics`, `ctest --preset graphics -LE soak` | 347 of 347 passed, 0 warnings, before the review fixes |
| Lab, after the review | `ctest --preset graphics -R '^kinetra_demo:\|^kinetra_dotio:\|^kinetra_capture\|^kinetra_graphics\|^kinetra_menu' -LE soak` | 136 of 136 passed, the smoke test with `ui-launcher` included; again after the agent review's first round |
| Maze clip check | `kinetra_capture <dir> demo-maze-clip` at 300 frames, and once at 200 | Passes at 300; at 200 it stops with "The Maze clip ended before the player reached its goal" |
| Format | clang-format 23.1.5 `--dry-run --Werror` on every changed C++ file in both repositories | Clean |
| Docs | `python3 external/Kinetra/tools/check_docs.py`; in Kinetra `python3 tools/check_docs.py` | All links resolve |
| Determinism | `capture_media.sh build-graphics` for every refreshed and new capture, twice | Byte-identical files |
| Looked at | Contact sheets of every new or refreshed clip; crops of the debug menus in `demo-debug.png` and `dotio-debug.png` | Each shows what its row in the media guide says. Gravity's statistics line reads FPS 0 for its first half second, as the application does after a start |
| Sizes | `capture_media.sh` | `demo-gravity-clip` 348 KB, `demo-maze-clip` 148 KB, `demo-doorway-clip` 256 KB, `i4sim-loadscreen-clip` 304 KB, `ui-launcher` 216 KB |
| Full | The pull requests' CI | Pending |

## Pull request

**feat(demo,dotio): the labs in GIFs, and SCENARIOS and DIFFICULTY back to the configuration page**

- **Summary:** Lab's README has a section per lab with its clips. New clips show Demo's Gravity, Maze and Doorway scenarios and i4sim's loadscreen. The launcher stills leave the README. Demo's and Dotio's debug menus end with SCENARIOS and DIFFICULTY, which return to the launcher's configuration page through Kinetra's new `SessionRequest::configurations`.
- **Changes:**
  - The rows in `DebugPanel` and `dotio::Application`, and the sessions' `request()`, with headless and graphics tests.
  - The capture tour follows `run_launcher`. There are three Demo scenario clips, a tour clip `i4sim-loadscreen-clip`, and `ui-launcher` ending with SCENARIOS; the smoke test plays `ui-launcher`.
  - `demo-debug.png` and `dotio-debug.png` are refreshed. Four stills are removed.
  - The README is rebuilt around the labs, and the media guide, capture README, module READMEs and launcher-overlays page are updated.
  - The roadmap item is closed.
  - The pointer moves to Kinetra `claude/roadmap-readme-media-demo-p1ywc6`, at 5a30722, which merged Kinetra `main` (6051dad) after Lab `main` had moved its pointer to b86a2ed.
- **Design:** this document.
- **Verification:** the table above; CI is the Full level.
- **Review notes:** the new README and its clips; the request's name and its contract in Kinetra#53; that Kinetra's `ui-debug-menu`, `ui-debug-layers` and `ui-inspector` are not refreshed (*Open points*).
- **Dependencies and merge order:** Kinetra#53 merges first, with a merge commit. This pull request then merges with its pointer at the Kinetra branch commit it was verified with. Kinetra's `lab-pointer` workflow records the merge commit after that.
- **Follow-up:** none planned.

## Open points

- After a plain `menu` request, such as Lab's LAUNCHER, the application list still hovers the row under a pointer that the session left there; only `show_configurations` settles the pointer. Settling it on every return would be a small Kinetra change, should the user want it (agent review of Kinetra#53).
- Once refreshed, Kinetra's `ui-debug-menu`, `ui-debug-layers` and `ui-inspector` will show Demo's new SCENARIOS row. They follow with the next Kinetra change that makes them stale, as Kinetra's media guide says.
