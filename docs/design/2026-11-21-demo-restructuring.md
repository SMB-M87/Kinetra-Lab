# Demo restructuring

Status: implemented · Branches: `refactor/demo-headers` (step 1, Lab #52), `refactor/demo-scene-operations` (step 2, Lab #54), `docs/demo-module` (step 3), `test/demo-contact-tests` (step 4, with Kinetra #19) · Related: [quality roadmap, phase 5](2026-11-18-quality-roadmap.md#phase-5-demo), [Demo](../../labs/demo/README.md)

Roadmap phase 5. The Demo's scene, agents and terrain live almost entirely in headers, `Scene` exposes its state as public fields that tests and the application write directly, a test suite that covers only the engine sits in Lab, and the module has no README that explains how it works. This phase moves the code into `.cpp` files, gives `Scene` operations, documents the module and moves the engine test to Kinetra, without changing behavior.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
  - [Steps](#steps)
  - [Step 1: headers](#step-1-headers)
  - [Step 2: scene operations](#step-2-scene-operations)
  - [Step 3: contracts and module README](#step-3-contracts-and-module-readme)
  - [Step 4: contact tests to Kinetra](#step-4-contact-tests-to-kinetra)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

Measured at Lab `2cd2c8a` and Kinetra `7d3e586` on 2026-11-21. The roadmap's relevance check of the same day found the renderer's parameter object, the zoom constants and the `Sandbox` → `Scene` rename already done; they are out of scope here.

- **Bodies in headers.** `labs/demo/include/demo/world/scene.h` is 489 lines, nearly all of them function bodies: the constructor (45 lines), `stream` (100), `update` (170) and the terrain-circle helper. `world/agent.h` (212) holds the wander and route logic, `world/terrain.h` (121) the chunk generator, `world/tuning.h` the validation, `input/mapping.h` the key mapping and `presentation/help_overlay.h` the help text. Every file that includes `application.h` compiles the scene.
- **`using namespace kinetra;` in headers.** At namespace scope in `scene.h:16`, which leaks into every includer, and inside functions in `agent.h:110`, `terrain.h:52`, `terrain.h:89` and `help_overlay.h:13`. The [conventions](../../external/Kinetra/docs/conventions.md) forbid the first.
- **Public mutable state.** `Scene` has 16 public data members (`scene.h:52-70`): `world`, `terrain`, `preview`, `navigation`, `moving_body_count`, `player`, `tuning`, `reciprocal_avoidance`, `avoidance_limited`, `player_path`, `player_goal`, `selected_agent`, `player_replan`, `following`, `view`. Its invariants (movers first in the world, agent body ids below `moving_body_count`, the terrain index matching the loaded terrain) hold only because callers do not break them. The terrain tests do: they clear the world, add bodies, set `player` and `moving_body_count` and rebuild `navigation` by hand (`terrain_tests.cpp:621-632`, `689-694`, `725-735`).
- **Long functions.** `Scene::update` (170 lines, one lambda of 60), `Scene::stream` (100) and the constructor (45) mix several steps each.
- **Engine test in Lab.** `labs/demo/tests/contact_tests.cpp` (6 cases) includes only `kinetra/simulation/physics.h` and links only `Kinetra::Simulation`; nothing in it is Demo policy.
- **Thin documentation.** `labs/demo/README.md` describes controls and policies but not the frame flow, the streaming algorithm or the tick; `docs/demo-architecture.md` repeats part of it. Most public functions in the Demo headers have no `///` contract.

The Demo module compiles without warnings under the `dev` preset (GCC 13; the 48 warnings of that build are all in Dotio), so it can join i4sim and keep that with `AS_ERRORS`.

## Goals and non-goals

**Goals:**
- Function bodies in `.cpp` files; the headers declare and document. One-line accessors may stay inline.
- No `using namespace` in any Demo header.
- `Scene` state private, behind read accessors and named operations; the long functions split into named steps.
- `contact_tests.cpp` in Kinetra, as an engine suite.
- `///` contracts on every Demo header and a module README after the [template](../../external/Kinetra/docs/documentation.md#module-readme-template) that absorbs `docs/demo-architecture.md`.
- Demo warnings as errors.

**Non-goals:**
- Behavior changes. The four crowd characterization lines and every other Demo test stay as they are, bit for bit.
- The help overlay (marked done by the relevance check; see [open points](#open-points)), the renderer, zoom and naming items the roadmap lists but the relevance check found done.
- Moving Demo mechanisms to Kinetra. Shared passing is phase 8.

## Decisions

Agent choices, made while the user was away and recorded for review. None of them changes behavior or a public Kinetra contract.

| Decision | Choice | Why |
| --- | --- | --- |
| Pull requests | Four, stacked in order: headers, scene operations, contracts and README, contact tests. Each later one has the previous branch as its base. The contact tests came third in the first plan; they moved last because this session could not push to Kinetra (see [open points](#open-points)), and a Lab branch may not record an unpushed Kinetra commit | One logical step per pull request, as asked. Lab CI runs on every pull request regardless of its base, so each step is checked on its own. Stacking keeps the diffs small; GitHub retargets a stacked pull request to `main` when its base merges |
| Bodies first, encapsulation second | Step 1 moves code verbatim; step 2 changes the interface and extracts functions | A pure move is reviewable by comparing old and new text; mixing it with interface changes hides both |
| Hand-built layouts in tests | A `Scene::arrange(movers, player, obstacles, navigation_area)` operation instead of public fields or a test friend | Four terrain tests need an exact layout (one wall, one player). An operation keeps the invariants in one place, validates its input, and also rebuilds the terrain index that the tests now leave stale |
| Placing a body | `Scene::moving_body(id)` returns a mutable body for ids below `moving_body_count()` and throws otherwise | Tests and tools place the player or an agent; a mutable world would also let them add or remove bodies and break the mover-first layout |
| One raw world step in a test | The streaming test that checks generated terrain resolves a contact uses `Scene::update` instead of `world.step` | The world is no longer writable from outside. With no player intent and no velocity the player's acceleration is zero, so the tick resolves the same contact; the assertion (penetration shrinks) is unchanged |
| Contact suite in Kinetra | A new suite `kinetra_contact_response_tests` (prefix `kinetra_contact_response`), not merged into `simulation_tests.cpp` | The six cases form one topic (contact response of whole bodies) next to the swept-contact suites; a separate suite keeps `simulation_tests.cpp` from growing further |
| Architecture page | `docs/demo-architecture.md` is folded into the module README and removed; links point to the README | The roadmap absorbs it; the [documentation rules](../../external/Kinetra/AGENTS.md#repository-documentation) give every concept one authoritative place |
| Warnings | `kinetra_directory_warnings(labs/demo RECURSE AS_ERRORS)` in step 1 | Demo has no warnings today; adding the line first keeps every later step warning-free |

## Design

### Steps

| Step | Branch | Content |
| --- | --- | --- |
| 1 | `refactor/demo-headers` | This document; bodies to `.cpp`; no `using namespace` in headers; Demo warnings as errors |
| 2 | `refactor/demo-scene-operations` | `Scene` accessors and operations; `update`, `stream` and the constructor split into named steps |
| 3 | `docs/demo-module` | `///` contracts; module README; `docs/demo-architecture.md` absorbed; roadmap status |
| 4 | `test/demo-contact-tests` | Kinetra pull request with the suite; Lab drops its copy and records the Kinetra branch commit |

### Step 1: headers

| Header | Moves to | Stays in the header |
| --- | --- | --- |
| `world/scene.h` | `src/world/scene.cpp`; `append_avoidance_circles` and `max_box_circles` become internal to it (nothing else uses them) | The class, `maximum_agents`, `agents()` |
| `world/agent.h` | `src/world/agent.cpp`: `update_visibility`, `retarget`, `advance_navigation`, `steer_wander`, `choose_wander_goal`, the constructor | Accessors, `invalidate_route`, the tuning constants |
| `world/terrain.h` | `src/world/terrain.cpp`: `blocked`, `update`, `generate` | Constants, accessors, constructor |
| `world/tuning.h` | `src/world/tuning.cpp`: `validate` | Types and defaults |
| `input/mapping.h` | `src/input/mapping.cpp`: `read_commands`, compiled into the logic library; the header includes `kinetra/view/input.h` instead of the runtime engine header | Declaration |
| `presentation/help_overlay.h` | `src/presentation/help_overlay.cpp` | Declaration |

`.cpp` files may use `using namespace kinetra;` inside `namespace demo`, as `renderer.cpp` already does. Headers name `kinetra::` types in full.

### Step 2: scene operations

The public fields become private members with a trailing underscore. Callers use:

| Need | Today | After |
| --- | --- | --- |
| Read the world, terrain, preview, navigation | `scene.world` etc. | `world()`, `terrain()`, `preview()`, `navigation()` (const references) |
| Read the layout | `moving_body_count`, `player` | `moving_body_count()`, `player()` |
| Read the player's route and the selection | `player_path`, `player_goal`, `selected_agent` | `player_path()` (span), `player_goal()`, `selected_agent()` |
| Read frame state | `following`, `view`, `reciprocal_avoidance`, `avoidance_limited` | Accessors of the same names |
| Tune | `tuning` | `tuning()`, mutable and const, as `Application::tuning()` already offers; `update` still validates |
| Change settings between frames | Field writes from `Application` | `set_following(bool)`, `set_reciprocal_avoidance(bool)`, `set_gravity(Vec2)`, `clear_selection()` |
| Place a moving body | `scene.world.body(id) = ...` | `moving_body(id)` |
| Lay out an exact arena | Clearing the world and setting fields by hand | `arrange(movers, player, obstacles, navigation_area)` |

`player_replan` has no accessor: only the scene reads it.

**`arrange`.** Replaces every world body with `movers` followed by `obstacles` (gravity stays), sets the player and the mover count, makes `obstacles` the loaded terrain with its index, rebuilds the navigation over `navigation_area` around them, drops the player's goal and route, and invalidates agent routes, as a terrain change in `stream` does. It throws `std::invalid_argument`, before changing anything, when `player` is not a mover index or an agent's body id is not a mover index other than the player. The next `stream` that loads terrain replaces the arrangement with streamed terrain. In today's tests the arena is followed by `walk_to` or `target_agents`, which set the same route state again, so their outcome is unchanged; only route wander or no agents run in them, so the terrain index they did not have before is never queried.

**Extracted steps.** Behavior-preserving Extract Function, each step keeping the floating-point operations in their order:

| Function | Steps |
| --- | --- |
| Constructor | `populate_initial(count, seed)`, `add_player_and_props()` |
| `stream` | `evict_distant(retained)`, `load_terrain(retained)`, `populate_chunks(active)`, `reload_physics(retained, active)` |
| `update` | `steering_settings()`, `avoidance_settings()`, `steer_agents(...)`, `steer_player(...)`, and the 60-line lambda as `local_acceleration(...)` on a small per-tick context struct |

### Step 3: contracts and module README

- `///` on every public type, function and non-trivial member in `labs/demo/include/demo/`, with units (world units, seconds, logical pixels), ownership and failure behavior.
- `labs/demo/README.md` rewritten after the module template: concepts, structure with a diagram of the real types, the frame and tick flow, the streaming algorithm, the agent lifecycle as a state diagram, avoidance, invariants, recipes, tests and open points. The content of `docs/demo-architecture.md` moves in; the page goes, and its links (`docs/README.md`, `docs/architecture.md`, `AGENTS.md`, others found by `grep`) point to the README.
- The roadmap's phase 5 row in the Status table.

As implemented: the README keeps the controls and debug-menu reference and adds concepts, a class diagram, the frame as a sequence diagram, the streaming and tick algorithms, the agent lifecycle as a state diagram, invariants, recipes and the local-golden procedure. `docs/README.md`, `docs/architecture.md` and `AGENTS.md` link the README instead of the removed page.

Demo has no navigation-only sub-READMEs left: the include layout change already removed them.

### Step 4: contact tests to Kinetra

Kinetra branch `test/contact-response-tests`: `tests/contact_response_tests.cpp` with the six cases unchanged except its header comment, registered as `kinetra_contact_response_tests`, with a row in the test map and the targeted-tests table. Lab removes `labs/demo/tests/contact_tests.cpp` and `kinetra_demo_contact_tests`, updates the `AGENTS.md` targeted table and records the Kinetra branch commit. Kinetra merges first, with a merge commit; then the Lab pointer moves to Kinetra's merge commit, as [AGENTS.md](../../AGENTS.md#git-with-the-engine-submodule) prescribes.

## Affected areas

- `labs/demo/include/demo/**`, `labs/demo/src/**` (new `src/world/`, `src/input/`), `labs/demo/CMakeLists.txt`, `labs/demo/tests/*` (scene access), root `CMakeLists.txt` (warnings).
- `tools/capture/src/tour/tour_script.cpp` reads `scene.world`; it moves to `world()`.
- Kinetra: `tests/contact_response_tests.cpp`, `CMakeLists.txt`, `tests/README.md`, `docs/testing.md`, a Kinetra design document.
- Docs: `labs/demo/README.md`, `docs/demo-architecture.md` (removed), `docs/README.md`, `docs/architecture.md`, `AGENTS.md`, the roadmap, this index.
- No public Kinetra header, no blueprint, no CLI argument changes.

## Test plan

| Behavior | Proof | Level |
| --- | --- | --- |
| Crowd behavior unchanged by every step | `kinetra_demo_crowd_characterization` reproduces its four recorded lines, including the trajectory fingerprints | Targeted per commit |
| Scene, agents, terrain, application and debug menu unchanged | `kinetra_demo_terrain`, `kinetra_demo`, `kinetra_demo_crowd` (soak, at the end of steps 1 and 2) | Targeted per commit |
| `arrange` keeps the invariants | New cases: it rejects a player or agent body id outside the movers, before changing anything; after it, the world holds the movers then the obstacles and navigation plans around them | Targeted |
| `moving_body` refuses terrain | New case: an id at or above `moving_body_count()` throws `std::out_of_range` | Targeted |
| Contact response in Kinetra | The six cases pass as `kinetra_contact_response:*` in Kinetra; Lab no longer registers them | Kinetra targeted; Lab Full |
| Presentation, launcher and capture tool still build and draw | `cmake --workflow --preset graphics`, smoke runs of `kinetra`, `kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim` with `--frames 120` under Xvfb | Full, last commit of each step |
| Docs | `python3 external/Kinetra/tools/check_docs.py` | Docs |

## Review

**Step 1.** Self-review of `git diff main...refactor/demo-headers`: every moved body compared with its old text; only indentation, `inline` and the `Class::` prefix changed. `mapping.h` now includes `kinetra/view/input.h`; `kinetra_lab/input.h`, which `mapping.cpp` uses, includes the runtime's `engine.h`, but that header has no SDL or OpenGL dependency, so the headless build still links. No findings.

**Step 2.** Self-review of `git diff refactor/demo-headers...refactor/demo-scene-operations`. Each extracted function was compared with the code it replaces for the order of its floating-point operations and side effects; the lambda's captures became `Tick` fields or members with the same values. Checked that `arrange` keeps its "before changing anything" promise: it rebuilds the navigation before it touches the world, and the navigation keeps its old snapshot when it rejects an area. No findings.

## Verification

**Golden lines depend on the toolchain.** On this machine (Ubuntu 24.04, GCC 13.3) unchanged `main` does not reproduce the recorded crowd characterization lines, nor the i4sim RVO golden line, in Debug or Release; these are the same five cases the `windows` presets exclude for that reason. CI's pinned Arch toolchain checks the recorded lines. Locally, each step is checked against the lines that `main` produces here: the step must reproduce them exactly.

**Step 1** (`refactor/demo-headers`, at `a0c2438`):

| Check | Command | Result |
| --- | --- | --- |
| Targeted | `cmake --build --preset dev --target kinetra_demo_tests kinetra_demo_terrain_tests kinetra_demo_crowd_tests kinetra_demo_crowd_characterization_tests kinetra_demo kinetra kinetra_capture`; `ctest --test-dir build-dev -R '^kinetra_demo' -E characterization -LE soak` | passed, 57 cases, no warnings |
| Characterization, Debug | the four lines against `main`'s Debug lines on this machine | identical |
| Headless logic | `cmake --preset headless`, build `kinetra_demo_tests kinetra_demo_terrain_tests` | passed |
| Full | `cmake --workflow --preset graphics` | 295 of 300 passed; the 5 failures are the toolchain-dependent golden cases above |
| Golden, Release | the five lines against `main`'s Release lines on this machine | identical |
| Smoke | `kinetra` (demo, dotio, i4sim, menu), `kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim`, each `--frames 120`, under Xvfb with software Mesa | passed |

**Step 2** (`refactor/demo-scene-operations`, at `33fe902`): targeted `dev` build without warnings and 60 Demo cases passed, the three new ones included; the characterization lines are identical to `main`'s in Debug. `cmake --workflow --preset graphics` passed 298 of 303, failing only the same five toolchain-dependent golden cases, whose Release lines are identical to `main`'s; the seven smoke runs passed.

**Step 3** (`docs/demo-module`): `python3 external/Kinetra/tools/check_docs.py` passed; the header edits are comment-only, apart from `PointerAction` and `ActionResult`, which are split over lines (same enumerators, same order) to carry a comment per value, confirmed by a `dev` build of the Demo targets and the capture tool without warnings. Every path, type and test name in the README was checked with `grep`.

**Step 4** (local `test/demo-contact-tests`, Kinetra `test/contact-response-tests` at `88a4916`, rebased on Kinetra `main`): in Kinetra, `kinetra_contact_response_tests` builds without warnings and passes its 6 cases; `diff` against Lab's file shows only the header comment. In Lab, the `dev` build of the Demo targets has no warnings, `kinetra_demo_contacts` is no longer registered, the 54 remaining non-soak Demo cases pass and the characterization lines are identical to `main`'s. `cmake --workflow --preset graphics` on the pointer `88a4916` passed 308 of 313, failing only the five toolchain-dependent golden cases that fail on `main` here too; the seven smoke runs passed. After SMB-M87/Kinetra#19 merged, the pointer moved to its merge commit `8896a19` (parents `d2d4a54` and `88a4916`); `git diff --stat 88a4916 8896a19` is empty, so this verification stands.

`ctest --preset dev -LE soak` ignores the label filter with CMake 3.28; `ctest --test-dir build-dev -LE soak` applies it.

## Pull request

**Step 1.** Title: `refactor(demo): move function bodies out of the headers`

Summary: roadmap phase 5, step 1 of 4. The Demo's scene, agent, terrain, tuning, input mapping and help overlay keep only declarations in their headers; the bodies move verbatim to `.cpp` files. No header has a `using namespace` any more. The Demo module joins i4sim with warnings as errors. Includes the phase's design document. No behavior change.

Changes: `labs/demo/src/world/{scene,agent,terrain,tuning}.cpp`, `src/input/mapping.cpp` (logic library), `src/presentation/help_overlay.cpp`; trimmed header includes; two test files include the steering and avoidance headers they use; `kinetra_directory_warnings(labs/demo RECURSE AS_ERRORS)`.

Dependencies and merge order: none; steps 2 to 4 are stacked on this branch.

**Step 4.** Title: `test(demo): move the engine-only contact tests to Kinetra`

Summary: roadmap phase 5, step 4 of 4. `labs/demo/tests/contact_tests.cpp` tested only `World::step`; its six cases move unchanged to Kinetra as `kinetra_contact_response_tests` (Kinetra [#19](https://github.com/SMB-M87/Kinetra/pull/19), branch `test/contact-response-tests`). Lab drops its copy and the `kinetra_demo_contact_tests` target, updates the targeted table in `AGENTS.md` and the README, and records the Kinetra branch commit.

Dependencies and merge order: stacked on step 3. The Kinetra pull request merges first, with a merge commit; then the pointer moves to Kinetra's merge commit before this one merges.

**Step 3.** Title: `docs(demo): contracts and module README`

Summary: roadmap phase 5, step 3 of 4. `///` contracts on every Demo header (comment-only) and a module README after the template, which absorbs `docs/demo-architecture.md`; the roadmap's phase 5 row. No code change.

Dependencies and merge order: stacked on step 2; merge #52 and #54 first.

**Step 2.** Title: `refactor(demo): give Scene operations instead of public fields`

Summary: roadmap phase 5, step 2 of 4. `Scene`'s 16 public fields become private. Callers read through accessors and change state through named operations; tests that built a layout by clearing the world and setting fields now use `arrange`, which keeps the scene's invariants. The constructor, `stream` and `update` are split into named steps (`update` from 170 lines to 12). No behavior change: the crowd characterization lines are identical to `main`'s.

Dependencies and merge order: stacked on step 1; merge step 1 first.

## Open points

- **Help overlay is reachable only from tests.** `FrameCommands::toggle_help` is set by no input mapping, and the renderer never calls `demo::draw_help`; only `application_tests.cpp` and `presentation_tests.cpp` use them. The help lines a user sees come from the settings overlay. The relevance check of 2026-11-21 marked the help overlay as done, so this phase leaves it alone. Decision for the user: remove `draw_help`, `toggle_help` and `PresentationOptions::help`, or draw the overlay from the renderer.
- **Characterization lines and toolchains.** Resolved on 2026-11-22: the lines held only on CI's pinned toolchain because glibc before 2.41 and Windows' C runtime round `atan2f` and `asinf` differently; with Kinetra's `atan2` and `asin` they hold everywhere CI tests ([Linux beyond Arch](2026-11-22-linux-beyond-arch.md)).
