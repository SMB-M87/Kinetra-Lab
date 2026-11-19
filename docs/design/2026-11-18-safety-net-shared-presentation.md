# Safety net and shared presentation

Status: implemented · Branches: `test/characterization-safety-net` (step 1), `refactor/shared-presentation` (steps 2–7, with `test/lab-doctest` merged in) · Related: [quality roadmap, phase 3](2026-11-18-quality-roadmap.md#phase-3-safety-net-and-shared-presentation), [Kinetra testing](../../external/Kinetra/docs/testing.md), [launcher and overlays](../launcher-overlays.md)

Phase 3 of the quality roadmap. It pins the current simulation behavior with characterization tests, so phases 4–6 can restructure i4sim, Demo and Dotio safely. It also removes the presentation glue that the three sessions duplicate. This document refines the roadmap scope with evidence from the code and records the decisions that are still open.

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

Surveyed on 2026-11-18 on `main` at `a669bf3` (Kinetra `acd5bb2`).

| Area | Evidence |
| --- | --- |
| No whole-run safety net | i4sim tests assert contracts per feature (`labs/i4sim/tests/simulation_tests.cpp`). Nothing pins a complete seeded run, so a refactor of `Simulation` could change routes, timing or throughput without a failing test. Demo crowds are only checked for finite motion and the sweep budget (`labs/demo/tests/crowd_stability_tests.cpp:12-26`). |
| Duplicated settings overlay | `i4sim::FactorySettings` (`labs/i4sim/include/factory_settings.h`) copies `lab::Settings` (`labs/settings.h`) except for the namespace, two help lines and an ignored `draw` flag. |
| Flag argument | `Settings::draw(..., bool dotio)` (`labs/settings.h:71`) switches three help lines and must be kept in step with `sim_controls_height` by hand (Dotio sets 28 at `labs/dotio/include/application.h:43`). |
| Duplicated session glue | Settings capture and debug-menu close: Demo `session.cpp:41-51`, Dotio `session.cpp:14-31`, i4sim `session.cpp:232-243`. Pause while settings are open: three variants (Demo `:47`, Dotio `:27`, i4sim edge detection `:233-239`). WASD/arrow direction: Demo `sandbox_input.h:12-20`, Dotio `presentation.cpp:16-20`, i4sim inline `session.cpp:279-283`. Zoom `pow(1.1, wheel)` with clamps: Demo `[5 or 17, 100]`, Dotio `[2.5 or 25, 50]`, i4sim `[0.005, 10]`, each repeated in the `Settings::update` call. FPS window of 0.5 s: Demo `session.cpp:16-39`, Dotio `application.h:191-197`. |
| Glyph patch | `factory_text` (`labs/i4sim/src/session.cpp:16-27`) uppercases and draws `.`, `_` and `-` over Kinetra's `draw_text`, which renders them blank (`external/Kinetra/include/kinetra/bitmap_text.h:39-48`). It has 11 call sites. Other text, such as the debug menu's "SPEED -" rows, loses the `-` today. |
| Launcher loop | Lab runs its own loop (`launcher/src/session.cpp:21-61`) instead of `kinetra::SessionHost`/`run_launcher` (`external/Kinetra/include/kinetra/launcher.h:54-77`). `run_launcher` binds M to the menu, lets Escape close the window and never reads `exit_requested` or a return request; `kinetra::Session` has no such hook (`session.h:6-14`). |
| Launcher depends on Dotio | `logic::Difficulty` is used in `launcher/include/menu.h:2-23`, `launcher/include/session.h:9-13`, `launcher/src/session.cpp` and `launcher/src/main.cpp:27-41`. |
| Dead LAUNCHER action | Standalone Demo and Dotio run `run_session`, which only checks `exit_requested` (`external/Kinetra/src/engine/session.cpp:16`). Their LAUNCHER button sets a flag that nobody reads. i4sim maps it to exit (`labs/i4sim/src/main.cpp:74-87`). |
| Test helpers | 11 copies of `require(bool, const char *)`, in every test program. |

## Goals and non-goals

Goals:

- A whole-run characterization for i4sim and Demo crowds that fails on any behavior change, runs in seconds and works headless.
- One settings overlay, in Kinetra, with application-supplied rows, actions and help lines; no flag argument.
- One place for the session glue: zoom stepping, direction mapping, pause-while-settings-open and the FPS counter.
- The launcher on `SessionHost`, with settings-driven navigation and without a Dotio dependency.
- All Lab tests as named doctest cases, with one helper instead of 11.

Non-goals:

- No behavior change in the simulations; the characterization values stay identical through this phase.
- No restructuring of `i4sim::Simulation`, Demo `Sandbox` or Dotio logic (phases 4–6).
- No new tooling, warnings or formatting; that is phase 2.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Scope and order | Roadmap phase 3, steps 1–7 | Settled in the [roadmap](2026-11-18-quality-roadmap.md#decisions) |
| Settings overlay and glyph fix | Move to Kinetra, with an ADR | Settled in the roadmap |
| Test framework | doctest, vendored by phase 2 | Settled in the roadmap; the target name comes from phase 2 |
| Characterization form | A golden line per seeded scenario: totals plus a 64-bit FNV-1a fingerprint of every mover's exact motion after every tick | Totals alone miss route changes; an end-state hash misses them when every mover ends parked (seen during the first attempt: two different runs gave the same end-state hash) |
| Shared test helper | Header-only target `kinetra_lab_test_support` (`labs/test_support/test_support.h`) with `require` and `Fingerprint` | The two new tests need the same hash; adding copies would contradict this phase. doctest replaces `require` in step 2 |
| Sequencing with phase 2 | Step 1 adds files only. Steps 2–7 wait until phase 2's clang-format commit and doctest target are on `main` | Editing the sessions before the repository-wide reformat would conflict on almost every line |
| Pull-request split | Step 1 as its own pull request now; steps 2–7 on a new branch after phase 2 merges | Settled with the user on 2026-11-18. Phase 4 gets its safety net early, and the test-only change cannot conflict with phase 2 |
| Launcher navigation | `kinetra::Session` gains a navigation request (stay, menu, exit) that `run_launcher` honors; its M and Escape bindings become configurable; Lab removes its own loop | Settled with the user on 2026-11-18. Returning to a menu is a mechanism any launcher needs. It changes Kinetra's public API, so it gets a Kinetra ADR |
| Shared helper placement | A `kinetra_lab_common` target in `labs/common/` for Lab policy, included through target include directories | Settled with the user on 2026-11-18. Replaces the `../../settings.h` includes, as the [conventions](../../external/Kinetra/docs/conventions.md#headers-and-includes) ask |
| Mechanism or policy (step 5) | Kinetra gets `zoom()` with `ZoomLimits` and `RateCounter`; Lab keeps the WASD/arrow mapping (`lab::direction`) and pause-while-open (`lab::ModalPause`) | Agent's choice on 2026-11-18, following the roadmap rule: key bindings and pausing are application policy ([Kinetra boundaries](../../external/Kinetra/AGENTS.md#architecture-boundaries)) |
| Who registers applications | The launcher's composition root (`launcher::applications()`); applications expose only their initial zoom and, for Dotio, difficulty names and an index-based factory | Agent's choice. Registrations in each presentation library would make all three depend on `Kinetra::Launcher` |
| Test helpers | `kinetra_lab_test_support` stays separate from `kinetra_lab_common` | Agent's choice. It is test-only, built only with `BUILD_TESTING`; the design originally folded it in |

## Design

```mermaid
flowchart LR
    P2["Phase 2 on main<br/>(format, doctest, CI)"]
    S1["1 Characterization<br/>(done on this branch)"]
    S2["2 doctest migration"]
    S3["3 Settings overlay<br/>to Kinetra + ADR"]
    S4["4 Glyphs to Kinetra"]
    S5["5 Shared session glue"]
    S6["6 Launcher on SessionHost"]
    S7["7 Standalone LAUNCHER"]
    P2 --> S2
    S1 --> S2
    P2 --> S3
    S3 --> S5
    S4 --> S5
    S3 --> S6
    S6 --> S7
```

### 1. Characterization (implemented)

- **i4sim** (`labs/i4sim/tests/golden_tests.cpp`, CTest `kinetra_i4sim_golden`). Ten scenarios: MAS with default motion and with every coordination feature, `3840x2400` coordinated, the Original, RVO and Improved-with-recovery profiles on `1680x960+`, and the junction, corridor, corridor-recovery and predictive-passing fixtures with the manual goals the contract tests use. A `Recorder` ticks the simulation and hashes each mover's position and velocity bits after every tick; the summary adds ticks, completed products, contacts, producer interactions, distance and the open products. Runtime is 0.6 s in Release and 7.6 s in Debug.
- **Demo** (`labs/demo/tests/crowd_characterization_tests.cpp`, CTest `kinetra_demo_crowd_characterization`). A dense idle crowd (240 agents, seed 123, 480 frames) and a player walking east then north through newly streamed chunks, with commanded agents at frame 120 (40 agents, seed 456, 1200 frames). Every frame checks finite motion, the sweep budget, the population cap, a one-to-one agent-to-body mapping and a separate player body. Every 20 frames it checks that no two moving bodies overlap by more than 0.01 units; the largest overlap measured was 0.001. The fingerprint covers every moving body's position, velocity, angle and angular velocity plus the camera. Runtime is 3.8 s in Release and 14 s in Debug.
- Both report expected and actual lines on failure. Since step 2 each scenario is one doctest case, and `KINETRA_PRINT_GOLDEN=1` (formerly `--print`) prints the current lines for a deliberate update.
- The tests drive today's public API, including the `paused` and `workload` fields. When phase 4 encapsulates those fields, the calls in the test change; the expected lines must not.

### 2–7. Implemented

```mermaid
flowchart LR
    subgraph Kinetra
        Over["SettingsOverlay"]
        Req["SessionRequest"]
        Zoom["zoom(), RateCounter"]
        RL["run_launcher + LauncherBindings"]
    end
    subgraph Common["kinetra_lab_common"]
        Rows["lab::settings_rows"]
        Pause["lab::ModalPause"]
        Dir["lab::direction"]
        Base["lab::Session"]
    end
    Sessions["Demo, Dotio, i4sim sessions"] --> Rows --> Over
    Sessions --> Pause
    Sessions --> Dir
    Sessions --> Zoom
    Sessions -->|request()| Req
    Launcher["launcher::applications()"] --> RL
    RL --> Sessions
```

*Takeaway:* every session is now the same thin shape: Kinetra mechanisms plus Lab policy from one place, and only its own rules.

2. **doctest migration.** Every Lab test program is a doctest suite registered with `kinetra_add_doctest`, prefixed with its former CTest name (`kinetra_i4sim:<case>`). The 11 `require` copies are gone; `kinetra_lab_test_support` keeps `Fingerprint`. The golden programs print their current lines with `KINETRA_PRINT_GOLDEN=1` instead of `--print`. The conversion ran on `test/lab-doctest` and is merged into this branch.
3. **Settings overlay.** Each session owns a `kinetra::SettingsOverlay` built by `lab::settings_rows()` and returns its request. `lab::Settings`, `FactorySettings` and the `dotio` flag are gone; Dotio's overlay lives in its session, because `dotio::Application` is also used by headless tests.
4. **Glyphs.** `factory_text` is gone; i4sim calls `kinetra::draw_text`, which now draws the same pixels for `- _ .`, plus `,` and `*`, and lowercase.
5. **Shared session glue.** `kinetra::zoom` with named limits (`labs/demo/include/zoom.h`, `labs/dotio/include/zoom.h`, i4sim's session), `kinetra::RateCounter` for Demo's and Dotio's rates, `lab::direction` for movement and camera panning, `lab::ModalPause` for all three sessions.
6. **Launcher.** `kinetra::run_launcher` with `launcher::applications()` and `launcher::bindings`. Lab's loop, `launcher::Menu` and the `logic::Difficulty` dependency are gone; Dotio is created from a difficulty index.
7. **Standalone LAUNCHER.** `run_session` ends on `SessionRequest::menu`, so Demo and Dotio end like i4sim; i4sim's wrapper session is removed.

## Affected areas

- Step 1: `labs/test_support/` (new; folds into `labs/common/` with step 5), `labs/CMakeLists.txt`, `labs/i4sim/CMakeLists.txt`, `labs/demo/CMakeLists.txt`, the two new test files, `AGENTS.md` (targeted-verification targets), `labs/i4sim/tests/README.md`, `labs/demo/README.md`, `labs/demo/tests/README.md`.
- Steps 2–7: every Lab test program; `labs/settings.h`, `labs/i4sim/include/factory_settings.h`; the three `session.cpp` files and Dotio `application.h`/`presentation.cpp`; `launcher/`; Kinetra `bitmap_text.h`, a new overlay header, `launcher.h`/`session.h` if the navigation hook goes there; `docs/architecture.md`, `docs/launcher-overlays.md` and the launcher READMEs.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| i4sim runs are unchanged | `kinetra_i4sim_golden` | Targeted |
| Demo crowds keep their invariants and trajectory | `kinetra_demo_crowd_characterization` | Targeted |
| The safety net detects change | Manual mutation check, reverted: heatmap weight (16 → 12) and the predictive arrival threshold were caught; heatmap refresh period and the pair-progress threshold were not. Demo: player size caught; commanded-travel duration (20 → 19) not caught, because the scenario is shorter than the command | Recorded here |
| Overlay rows, picking and actions (step 3) | Existing `kinetra_settings` cases ported to the Kinetra overlay, plus a case per row kind | Full |
| Glyphs (step 4) | Headless `glyph(char)` test in Kinetra | Full |
| Session glue (step 5) | Headless tests of zoom, direction and pause rules; graphics tests unchanged | Full with graphics |
| Launcher (step 6–7) | `kinetra_menu`, `kinetra_graphics` and the standalone smoke runs | Full with graphics |

The golden values stay unchanged through every step of this phase.

## Review

Step 1: `/code-review` (low) on the branch diff found no issues. The self-review caught one dangling lambda capture and a weak end-state hash before the commit; both were fixed.

Steps 2–7: `/code-review` (high) on `docs/roadmap-and-cleanup...refactor/shared-presentation`:

| Finding | Resolution |
| --- | --- |
| i4sim's `ModalPause` kept the saved pause state when the session went back to the blueprint screen, so a later simulation could inherit it | Fixed: the pause state is reset with the simulation |
| `kinetra_lab_common` links only `Kinetra::Simulation`, but `lab/settings.h` needs the runtime | Fixed: the CMake comment says which headers are headless |
| The launcher's menu title is now Kinetra's "Kinetra \| Start menu", and titles refresh every 500 ms instead of every frame | Accepted behavior change of moving to `run_launcher`; recorded here |
| The three sessions repeat the same overlay frame glue | Kept for phase 4–6, which restructure each session |
| Help lines live apart from the bindings they describe | Kept; the module phases may move them next to the input mapping |
| The launcher integration test mirrors the panel with fixed line counts | Kept; a changed line count fails the test loudly |

Self-review before the commits: the registration was first put in each presentation library and moved back to the launcher (see Decisions); Dotio's overlay moved out of `dotio::Application`, which headless tests use; the zoom constants moved to small `zoom.h` headers, so the launcher does not compile the scene headers and their warnings. A first commit accidentally took staged deletions and was redone before anything was published.

## Verification

Step 1, headless Release build (`-DKINETRA_LAB_BUILD_RUNTIME=OFF`) in `build/`:

- `ctest --test-dir build --output-on-failure` before the change: 9 of 9 passed.
- `ctest --test-dir build -R '^kinetra_(i4sim_golden|demo_crowd_characterization)$' --output-on-failure`: 2 of 2 passed.
- Debug build in `build-debug/`: both programs reproduce the Release values.
- `./build.sh` (Full): Kinetra 9/9 and Lab 12/12 passed, with no warnings.

Steps 2–7, in the `refactor/shared-presentation` worktree against Kinetra `ccb3396` (`feat/overlay-glyphs-navigation`):

| Command | Result |
| --- | --- |
| `cmake --workflow --preset ci` | 117/117 passed |
| `cmake --workflow --preset headless` | 136/136 passed |
| `cmake --workflow --preset graphics` (SDL offscreen, software Mesa, WSL2) | 124/124 passed |
| Debug build with tests and graphics, `ctest` | 136/136 passed |
| `kinetra_i4sim_golden`, `kinetra_demo_crowd_characterization` | Unchanged values; the step 2 conversion compared `KINETRA_PRINT_GOLDEN=1` output byte for byte |
| Lab warnings (`KINETRA_LAB_WARNINGS`, ci build) | 67 unique locations, all in unchanged headers; none in changed files (119 when the policy started) |
| Smoke: `kinetra --variant demo\|dotio\|i4sim --frames 60`, `kinetra --frames 30`, `kinetra_demo`, `kinetra_dotio --difficulty easy`, `kinetra_i4sim --frames 60`, `kinetra_i4sim --blueprint ... --ticks 2000` | All exit 0 |
| `clang-format --dry-run --Werror`, `python3 external/Kinetra/tools/check_docs.py` | Clean |

Not run: an interactive desktop playtest outside WSL2.

## Pull request

Step 1: `test: characterize i4sim and Demo crowds before phase 3 refactors` (description: summary, changes, verification with mutation check, merge order: independent). Steps 2–7 get their own pull request.

Steps 2–7: `refactor: phase 3 shared presentation on Kinetra's overlay and launcher`. Stacked on the documentation pull request; Kinetra pull request #5 merges first, with a merge commit.

## Open points

- **Uppercasing in `draw_text`.** `factory_text` uppercases; Kinetra's `draw_text` does not. Decide in the Kinetra design document for step 4.
- **Coverage gaps of the golden scenarios.** Heatmap refresh timing, the stalled-pair cancellation of predictive passing and the expiry of commanded agent travel do not change the values. Phases 4 and 5 add targeted tests when they touch that code.
- **Floating-point portability.** The fingerprints need identical floating-point results. CI on another compiler or C library may differ. If phase 2's CI shows that, keep the totals exact and compare the fingerprint per platform only.
