# Include layout

Status: implemented · Branch: `refactor/include-layout` · Kinetra pull request: [SMB-M87/Kinetra#14](https://github.com/SMB-M87/Kinetra/pull/14) · Related: [Kinetra design](../../external/Kinetra/docs/design/2026-11-20-include-layout.md), [ADR 0015](../../external/Kinetra/docs/decisions/0015-include-layout.md), [directory rule](../../external/Kinetra/docs/conventions.md#directory-layout)

Lab's modules each grew their own layout: 17 headers in `labs/i4sim/src/`, 7 in `tools/capture/src/`, helper headers loose in `tests/`, and four Dotio folders for one `gmath` file. Includes carry no module name, so three `session.h` and two `zoom.h` meet, and the launcher reaches them with `../../labs/demo/include/session.h`. This change moves every Lab module to the shared [directory rule](../../external/Kinetra/docs/conventions.md#directory-layout), groups the larger ones by topic, and then aligns the namespaces with the include roots. The engine side, with the new `<kinetra/simulation|view|runtime/...>` paths, is the [Kinetra design](../../external/Kinetra/docs/design/2026-11-20-include-layout.md).

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

- **Headers in `src/`:** `labs/i4sim/src/` (17), `tools/capture/src/` (7; its include directory *is* `src/`).
- **Headers loose in `tests/`:** `labs/i4sim/tests/{event_counts,recorder,showcase}.h`, `labs/test_support/test_support.h`.
- **Folders for one file:** `labs/dotio/gmath/{include,src}/` for `gmath.h` and `gmath.cpp`, next to `labs/dotio/include/` and `labs/dotio/logic/{include,src}/`.
- **Unnamed include roots:** `#include "session.h"` resolves by include-path order. [applications.cpp:2](../../launcher/src/applications.cpp) uses `../../labs/demo/include/session.h`, [scenes.cpp:8](../../tools/capture/src/scenes.cpp) `../../../labs/i4sim/include/blueprint.h`; the conventions ask for target include directories instead.
- **Vague shared name:** `labs/common/include/lab/` with namespace `lab`.
- **Namespaces differ from module names:** Demo uses `sandbox`; Dotio `logic`, `gmath` and `dotio`.

## Goals and non-goals

- Goal: every Lab module follows the [directory rule](../../external/Kinetra/docs/conventions.md#directory-layout); no `.h` outside an include root.
- Goal: every include starts with its module (`"i4sim/..."`, `"dotio/..."`, `"kinetra_lab/..."`; Kinetra stays `<kinetra/...>`); no `../` include remains.
- Goal: namespace equals include root, in separate commits after the moves.
- Goal: the engine's new include paths are used everywhere.
- Non-goal: behavior changes. Types, files and targets are renamed only where the names below say so.
- Non-goal: the i4sim blueprint catalog (`labs/i4sim/blueprints/` is data and stays).

## Decisions

Settled with the user in three grilling rounds; choices that differ from the first recommendation are marked.

| Decision | Choice | Why |
| --- | --- | --- |
| Module layout | `include/<name>/` and `src/` per module; layers as subfolders (`presentation/`) in both | Same shape as Kinetra; one include root per module |
| Module folder inside `include/` *(asked again after the first review)* | Kept: `include/<name>/` | Without it, `presentation/session.h` of Demo, Dotio and i4sim and both `zoom.h` meet in the launcher and the capture tool, which see every module's include root; the compiler takes the first silently. User's choice after that explanation |
| Internal headers | `detail/` inside their topic | Visible public/internal split without a third root |
| Test helper headers | `tests/include/<name>/testing/`, linked to test targets only | Test code stays out of the production include root |
| Shared Lab code | `labs/shared/include/kinetra_lab/`, namespace `kinetra_lab` | Names the owner without looking like an engine header |
| Namespaces | Equal to the include root (`demo`, `dotio`, `i4sim`, `capture`, `launcher`, `kinetra_lab`), in separate commits after the moves | Moves stay reviewable |
| Demo names *(follow-up, settled after the first review)* | `demo::Application` and `demo::Scene` (were `SandboxApplication` and `Sandbox`); files `application.h`, `world/scene.h`, `input/mapping.h`, `input/commands.h`, `presentation/renderer.h` | User's choice: one name for the application everywhere |
| Target names *(follow-up)* | Logic targets `kinetra_demo_logic`, `kinetra_dotio_logic`, `kinetra_i4sim_logic`; shared `kinetra_lab` and `kinetra_lab_testing`, i4sim's helpers `kinetra_i4sim_testing`; tests `kinetra_demo_tests` (prefix `kinetra_demo`), `kinetra_demo_terrain_tests`, `kinetra_lab_tests` | User's choice: one form for every module, equal to namespace and include root |
| `kinetra_sandbox` executable *(follow-up)* | Removed; the launcher is only `kinetra` | User's choice; it was a compatibility name from the first commit |
| i4sim topics | `simulation/`, `blueprint/`, `presentation/`, each with `detail/` *(differs: recommended was public/detail only)* | User's choice: the logic folder had grown too large |
| Engine | Moves in the same round to `simulation/`, `view/`, `runtime/` *(differs: recommended was Lab only)* | User's choice; [Kinetra design](../../external/Kinetra/docs/design/2026-11-20-include-layout.md) |
| Dotio topics | `entities/`, `ai/`, `navigation/`, `math/`, `presentation/` *(differs: recommended was flat)* | User's choice; `gmath` folds into `math/` |
| Demo topics | `world/`, `input/`, `presentation/` *(differs: recommended was flat)* | User's choice; topics chosen so no folder holds one file |
| Compatibility | Hard break, Kinetra merges first | User's choice; no forwarding headers |
| Start | After Kinetra-Lab #31 and #32 are merged | They touch files that move |
| Commits | One `git mv` commit for all modules, include and CMake fixes in the same commit, then one commit per namespace *(changed while building: planned was one commit per module)* | The modules include each other and the engine, so a commit per module would not build on its own |
| `gmath` namespace | `dotio::math`, not `dotio` *(changed while building)* | Merged into `dotio`, its `Vec2` met `kinetra::Vec2` in the presentation, which uses `using namespace kinetra`; the sub-namespace matches its folder |
| `zoom.h` in Demo and Dotio | Top of the include root, not `presentation/` *(changed while building)* | The logic (`demo::Application`, `dotio::Application`) includes it; a logic header must not reach into `presentation/` |

## Design

The rule itself is in [conventions](../../external/Kinetra/docs/conventions.md#directory-layout). The target tree per module:

```text
labs/shared/                                 (was labs/common, labs/test_support)
  include/kinetra_lab/   camera.h debug.h input.h session.h settings.h
  tests/include/kinetra_lab/testing/test_support.h
  tests/shared_tests.cpp                     (was common_tests.cpp)

labs/demo/
  include/demo/
    application.h debug_panel.h zoom.h
    world/         agent.h terrain.h scene.h tuning.h
    input/         mapping.h commands.h player_control.h
    presentation/  session.h renderer.h help_overlay.h
  src/  application.cpp debug_panel.cpp main.cpp
        presentation/ renderer.cpp session.cpp

labs/dotio/                                  (logic/, gmath/ and their sub-READMEs disappear)
  include/dotio/
    application.h world.h difficulty.h renderer.h zoom.h
    entities/      arrow.h bolt.h enemy.h player.h moving_object.h
                   physics_object.h static_object.h physics_adapter.h
    ai/            state.h wander.h fuzzy.h tracker.h
    navigation/    graph.h grid.h vertex.h priority_queue.h stack.h
    math/          gmath.h portable.h
    presentation/  session.h presentation.h
  src/  world.cpp difficulty.cpp main.cpp
        entities/ ai/ navigation/ math/ presentation/   (mirroring)

labs/i4sim/
  include/i4sim/
    controller.h
    simulation/    simulation.h events.h flow_metrics.h view.h
      detail/      simulation_core.h simulation_constants.h motion_step.h routing.h
                   traffic.h shuttle.h predictive_passing.h production.h goals.h
                   fleet.h floor_plan.h geometry.h event_log.h
    blueprint/     blueprint.h scenario.h
      detail/      json_fields.h
    presentation/  session.h
      detail/      world_view.h hud.h blueprint_screen.h
  src/  controller.cpp main.cpp
        simulation/ blueprint/ presentation/            (mirroring, without detail/)
  tests/include/i4sim/testing/  event_counts.h recorder.h showcase.h
  blueprints/                                       (unchanged)

launcher/include/launcher/applications.h
tools/capture/include/capture/  busy_stretch.h dotio_player.h png.h scenes.h showcase.h stage.h
```

- Every target's `target_include_directories` points at its module's `include/`; test targets add `tests/include/`. The launcher and capture link the presentation targets and include `"demo/presentation/session.h"` and friends instead of `../../`.
- Includes inside a module also use the full form (`"i4sim/simulation/detail/routing.h"`), so moving a file never depends on the directory of its includer.
- Thin sub-READMEs in `include/`, `src/` and `tests/` folders (Demo, Dotio, Dotio's `logic/` and `gmath/`) fold into their module README, as [AGENTS.md](../../AGENTS.md#documentation-in-lab) plans; each module README gets a layout section that shows its tree.
- Namespace commits: `sandbox` → `demo`; `logic` and `gamef` → `dotio`; `gmath` → `dotio::math`; `lab` → `kinetra_lab`; `lab::test` → `kinetra_lab::testing`. Helper namespaces inside a file (`layer_color`, `tuning_step`, `menu_row`) stay.

## Affected areas

- Every header and source under `labs/`, `launcher/`, `tools/capture/`; every `CMakeLists.txt` there and the root one (`kinetra_directory_warnings` paths, `labs/` subdirectories).
- The `external/Kinetra` pointer: the Kinetra merge commit with the engine layout.
- [AGENTS.md](../../AGENTS.md): layer table, verification table, functionality index (paths only; CTest regexes stay).
- [README.md](../../README.md) repository layout, [architecture](../architecture.md), the three application architecture pages, module READMEs, [capture tool](../../tools/capture/README.md), [launcher](../../launcher/README.md).
- Not affected: blueprint JSON, media, CTest names, presets, CI workflow (it calls presets).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Behavior unchanged | Every Lab suite, unchanged apart from include lines and namespaces | Full: `cmake --workflow --preset ci` and `--preset headless` |
| Logic still builds without the runtime | `headless` preset (`KINETRA_LAB_BUILD_RUNTIME=OFF`) | Full |
| Sessions, launcher and capture | `cmake --workflow --preset graphics`, smoke runs from the README, `kinetra_capture_smoke` | Full with graphics |
| Windows build | `cmake --workflow --preset windows` from WSL | Windows |
| No header outside an include root, no `../` include | `find labs launcher tools -path '*/src/*' -name '*.h'`, `find ... -path '*/tests/*.h' -not -path '*/tests/include/*'`, `grep -rn '#include "\.\./'` all empty | Manual |
| No warning added | `KINETRA_LAB_WARNINGS` count not above the baseline | Full |
| Docs | `python3 external/Kinetra/tools/check_docs.py` | Docs |

A refactor: no new tests and no changed assertions. Each module commit builds and passes its targeted suite before the next.

## Review

The full diff was read as a reviewer would; most of it is renames, which `git diff -M` pairs with their old paths.

| Finding | Resolution |
| --- | --- |
| `dotio/ai/tracker.h` had no includes and relied on its includer's order, which `clang-format` changed after the new paths | It includes `<concepts>` and `<cstddef>` itself (folded into the move commit) |
| `showcase.h` found the blueprints through `__FILE__`, one folder deeper after the move | The new `kinetra_i4sim_testing` target passes `KINETRA_I4SIM_BLUEPRINT_SOURCE_DIR` |
| `terrain_tests.cpp` reached Demo headers through `../include/` while linking only `Kinetra::Simulation` | It links `kinetra_demo_logic` |
| Merging `gamef` into `dotio` made the presentation's private `Renderer` adapter ambiguous with `dotio::Renderer` | The adapter is `RuntimeRenderer` |
| `zoom.h` sat in `presentation/` but logic includes it | Moved to the top of each include root |
| Namespace renames changed line lengths | A separate `style` commit reformats the touched files |
| Comments and guides still named `tools/capture/src/*.h`, `labs/common` and the old helper location | Updated |

Historical design documents keep their text; only link targets moved.

## Verification

| Check | Result |
| --- | --- |
| `cmake --workflow --preset ci` | 279/279 passed (after merging main and renaming) |
| `cmake --workflow --preset headless` (`KINETRA_LAB_BUILD_RUNTIME=OFF`) | 241/241 passed (after merging main and renaming) |
| `cmake --workflow --preset graphics` | 295/295 passed (after merging main and renaming) |
| Smoke runs: `kinetra --variant demo\|dotio\|i4sim --frames 120` and `kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim --frames 120`, offscreen | All six exit 0 |
| `cmake --workflow --preset windows` | 258/258 passed after merging main (MinGW-w64 cross build; the tests run on Windows through WSL interop). The renaming commit is left to the CI `windows` job |
| Warnings under `KINETRA_LAB_WARNINGS` | 46 distinct Lab warnings, the same as `main` built with the `ci` preset |
| `clang-format --dry-run --Werror` on every changed file | Clean |
| No header in `src/`, none loose in `tests/`, no `#include "../` | All three searches empty |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |
| Kinetra `ci` 251/251, `graphics` 263/264 | The one failure is the known WSLg mouse-input test, which fails on `main` too |

## Pull request

**refactor(layout): one include root per module, topic folders and matching namespaces**

Summary: every Lab module now follows the shared [directory layout](../../external/Kinetra/docs/conventions.md#directory-layout): one `include/<name>/` and a mirroring `src/`, no header in `src/` or loose in `tests/`, topic folders in i4sim, Dotio and Demo, internal headers in `detail/`. Includes carry their module path instead of bare names and `../`, namespaces match the include roots, and the engine pointer records Kinetra's matching layout. No behavior changes.

Changes: the move (`git mv`, includes, CMake), the folded directory READMEs, three namespace commits (`kinetra_lab`, `demo`, `dotio` and `dotio::math`), `zoom.h` beside the logic that reads it, a format commit and comment fixes.

Dependencies and merge order: the Kinetra pull request merges first, with a merge commit; then the pointer on this branch moves to that merge commit, and this pull request merges.

Also in this pull request, after the first review: Demo's `sandbox_` files and classes become `demo::Application` and `demo::Scene`, every logic target is `kinetra_<app>_logic`, the shared targets are `kinetra_lab` and `kinetra_lab_testing`, and the `kinetra_sandbox` executable is gone.

## Open points

None.
