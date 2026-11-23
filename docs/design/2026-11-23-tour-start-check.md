# The tour's start check names what runs

Status: accepted · Branch: `fix/tour-start-check` · Related: the round-2 agent review of [Kinetra-Lab#97](https://github.com/SMB-M87/Kinetra-Lab/pull/97)

The tour checks after its launcher chapter that Demo Terrain started, so the smoke test fails with a reason when a change to the launcher or Demo's scenario page moves a click. The check reaches its own message only when Demo runs another scenario; in the menu and in another application it fails earlier with a message about something else. This change makes all three cases fail with the tour's own message, saying what runs instead. It was a nit of #97's round-2 review, left for a later change because it is code.

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

`play_tour` (`tools/capture/src/tour/tour_script.cpp:482`) checks the start with `demo_application(tour).scene().scenario() != demo::Scenario::terrain`. `demo_application` (`tour_script.cpp:81`) casts `tour.session()` to `demo::Session &`:

- When the launcher started another application, the reference `dynamic_cast` throws `std::bad_cast`, and `kinetra_capture` prints `kinetra_capture: std::bad_cast`.
- When the launcher menu still shows, `Tour::session()` (`tools/capture/src/tour/tour.cpp:136`) throws `std::logic_error` "The tour asked for its session while the launcher menu shows", which reads as a mistake in the tour's code, not as a start that failed.
- Only when Demo runs another scenario does the tour throw "The tour did not start Demo Terrain".

The smoke test (`tools/capture/tests/smoke.cmake:33`) requires the line `tour verified Demo Terrain` and still fails in all three cases, but its report shows the unclear message. The `///` summary of `play_tour` (`tools/capture/include/capture/tour/tour.h:194`) does not say that it throws.

## Goals and non-goals

- Goal: in the menu, in another application and in another Demo scenario, the tour stops with `std::runtime_error` "The tour did not start Demo Terrain", followed by what runs instead.
- Goal: a window-free test proves the three messages and that Demo Terrain passes.
- Goal: the summary of `play_tour` says that it throws when Demo Terrain did not start.
- Non-goal: the reference casts of the later chapters (`demo_application`, `dotio_application`). After the start check, the script itself starts each application, and a wrong session there is a mistake in the script; the check that guards against a changed launcher is this one.
- Non-goal: any change to what the tour shows, to the smoke test or to the applications.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Where the check lives | A free function `require_demo_terrain(kinetra_lab::Session *running)` in `tour_script.cpp`, declared in `tour.h` next to `play_tour`; `play_tour` passes `nullptr` while `tour.in_menu()` | A `Tour` needs a `Runtime` and so a window; a function of the running session can be tested window-free in `kinetra_capture_tests`. The script, not `Tour`, knows that the tour starts in Demo Terrain |
| What the message says | "The tour did not start Demo Terrain: " and then "the launcher menu still shows", "<title> runs instead" with the session's `title()`, or "Demo runs <scenario label> instead" | The smoke test prints the tool's message; naming what runs tells the reader which click moved. `title()` is the application's name, the one place every session has it |
| Sessions in the test | Real Demo sessions from `demo::create_session` for the two scenario cases, and a stand-in `kinetra_lab::Session` for another application | A Demo session needs no window, and the real one proves the cast the tour relies on. Dotio's world is a singleton and i4sim reads its blueprints from disk, so a stand-in with a title is the window-free way to be "not Demo" |

None of these is load-bearing for the user: the task names the behavior, the cases and the summary.

## Design

```cpp
void require_demo_terrain(kinetra_lab::Session *running) {
    const std::string failed = "The tour did not start Demo Terrain: ";
    if (!running)
        throw std::runtime_error(failed + "the launcher menu still shows");
    const auto *demo = dynamic_cast<const demo::Session *>(running);
    if (!demo)
        throw std::runtime_error(failed + running->title() + " runs instead");
    if (const auto scenario = demo->application().scene().scenario(); scenario != demo::Scenario::terrain)
        throw std::runtime_error(failed + "Demo runs " + std::string(demo::scenario_label(scenario)) + " instead");
}
```

`play_tour` calls `require_demo_terrain(tour.in_menu() ? nullptr : &tour.session())` right after `launcher_chapter` and before `terrain_started`. `main.cpp` already prints any `std::exception` as `kinetra_capture: <message>` and returns a failure, so the smoke test's report carries the message unchanged.

## Affected areas

- `tools/capture/include/capture/tour/tour.h`: `require_demo_terrain` with its `///` summary; the summary of `play_tour`.
- `tools/capture/src/tour/tour_script.cpp`: the function, and `play_tour` calls it.
- `tools/capture/tests/capture_tests.cpp`: one test case and the stand-in session; the file's header comment.
- `tools/capture/CMakeLists.txt`: the comment on `kinetra_capture_tests`, which said that no session is created.
- `tools/capture/README.md`: the file table, the tour's invariant on what it reads, and the test table.
- This document and the design index.

## Test plan

- New case in `kinetra_capture_tests`: `require_demo_terrain` with `nullptr` throws `std::runtime_error` naming the menu; with a stand-in session titled `Dotio`, one naming Dotio; with a Demo session on Gravity, one naming GRAVITY; with a Demo session on Terrain, nothing. Every message starts with "The tour did not start Demo Terrain".
- Reproduction first: the function is first extracted with the old behavior (`std::logic_error` in the menu, a reference cast), and the new case is run against it to see the menu and the other application fail; then the fix.
- Level: `tools/capture/` targeted, from Lab's table: `cmake --build --preset dev --target kinetra_capture_tests kinetra_capture` and `ctest --preset dev -R '^kinetra_capture'`; then `ctest --preset graphics -R kinetra_capture_smoke`, which plays the tour's start and must still print `tour verified Demo Terrain`. Full level through the pull request's CI.

## Review

To be filled in.

## Verification

To be filled in.

## Pull request

To be filled in.

## Open points

None.
