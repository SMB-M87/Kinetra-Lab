# Kinetra-Lab Agent Guide

Rules for agents and developers working on Kinetra-Lab. **All rules in [Kinetra's AGENTS.md](external/Kinetra/AGENTS.md) apply here**: the development cycle, verification by impact, git, engineering rules, code and repository documentation, and skills. This file adds only what is specific to Lab. Explicit user instructions take precedence.

## Table of contents

- [Where to read what](#where-to-read-what)
- [Scope](#scope)
- [Lab boundaries](#lab-boundaries)
- [Verification in Lab](#verification-in-lab)
- [Git with the engine submodule](#git-with-the-engine-submodule)
- [Documentation in Lab](#documentation-in-lab)
- [Functionality index](#functionality-index)

[Project overview](README.md) · [Documentation index](docs/README.md)

## Where to read what

The Kinetra pages live in the `external/Kinetra` submodule. If that directory is empty, run `git submodule update --init --recursive` first.

| Task | Read |
| --- | --- |
| Every task | This file and [Kinetra's AGENTS.md](external/Kinetra/AGENTS.md) |
| Start, plan, review or hand over a change | [Workflow](external/Kinetra/docs/workflow.md); Lab [design documents](docs/design/README.md) |
| Write or review C++ or CMake | [Conventions](external/Kinetra/docs/conventions.md) |
| Add tests or pick a verification level | [Testing](external/Kinetra/docs/testing.md) and [verification in Lab](#verification-in-lab) |
| Write READMEs, diagrams or `///` comments | [Documentation guide](external/Kinetra/docs/documentation.md) |
| Understand Lab targets and session flow | [System architecture](docs/architecture.md) |
| Work on one application | Its README and architecture page (see [functionality index](#functionality-index)) |
| Know what is planned | [Quality roadmap](docs/design/2026-11-18-quality-roadmap.md) |

## Scope

- Lab owns the runnable applications (Demo, Dotio, i4sim), the bundled launcher executable `kinetra`, and their tests and media.
- The engine stays in Kinetra. A mechanism that two applications need, or that has no game rules in it, is a candidate for Kinetra. Move it there with a Kinetra design document, and an ADR when it changes a boundary.

## Lab boundaries

Each application follows the [layer rule](external/Kinetra/docs/conventions.md#layers-and-dependencies):

| Layer | Targets | May depend on |
| --- | --- | --- |
| Logic: headless rules and state | `kinetra_sandbox_application`, `kinetra_dotio_logic`, `kinetra_i4sim_application` | `Kinetra::Simulation` only |
| Presentation: session, input mapping, drawing | `kinetra_demo_presentation`, `kinetra_dotio_presentation`, `kinetra_i4sim_presentation` | Its own logic, `Kinetra::Runtime`, shared Lab helpers |
| Composition roots | `kinetra`, `kinetra_sandbox`, `kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim`, `kinetra_sessions` | Presentation libraries, `Kinetra::Launcher` |

- Open point: `kinetra_i4sim_presentation` also links `Kinetra::Launcher`, for the `LaunchMenu` of its blueprint screen. The table does not allow that edge; phase 4 either makes it the rule or removes it ([roadmap open points](docs/design/2026-11-18-quality-roadmap.md#open-points)).
- Applications never include each other's headers. Shared Lab code is application-neutral: today the Lab policy in [labs/common](labs/common/include/lab/) (target `kinetra_lab_common`) and the test helpers in [labs/test_support/test_support.h](labs/test_support/test_support.h) (target `kinetra_lab_test_support`).
- Logic must build and test with `-DKINETRA_LAB_BUILD_RUNTIME=OFF`.
- i4sim needs the header-only nlohmann JSON package (3.11+). Its blueprints live in `labs/i4sim/blueprints/`; CMake copies them to `i4sim-blueprints/` next to every executable that opens them, and the application reads them from there. Treat blueprint files as a data format: changing their fields is a contract change.

*Why:* headless logic is what makes simulation behavior testable without a window, and the dependency direction keeps applications replaceable.

## Verification in Lab

Use the levels from [Kinetra testing](external/Kinetra/docs/testing.md#verification-levels):

| Level | In Lab |
| --- | --- |
| Docs | `python3 external/Kinetra/tools/check_docs.py` |
| Targeted | `cmake --build --preset dev --target <targets>` and `ctest --preset dev -R '<regex>'`, using the table below (configure once with `cmake --preset dev`) |
| Full | `cmake --workflow --preset ci` and `cmake --workflow --preset headless`, which is what CI runs (or `./build.sh`). For runtime, presentation or launcher changes, also `cmake --workflow --preset graphics` and the smoke runs in the [README](README.md#tests-and-resources) |

| Changed area | Targets | CTest regex |
| --- | --- | --- |
| `labs/i4sim/` logic (`simulation`, `predictive_passing`, `blueprint`) | `kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim` | `^kinetra_i4sim` |
| `labs/demo/` logic (`sandbox_application`, terrain, agents) | `kinetra_sandbox_tests kinetra_terrain_tests kinetra_demo_contact_tests kinetra_demo_crowd_tests kinetra_demo_crowd_characterization_tests` | `^kinetra_(sandbox_application\|terrain\|demo_contacts\|demo_crowd)` |
| `labs/dotio/logic/`, `labs/dotio/gmath/` | `kinetra_dotio_tests` | `^kinetra_dotio:` |
| `labs/common/` | `kinetra_lab_common_tests` | `^kinetra_lab_common:` |
| `launcher/include/applications.h`, `launcher/src/applications.cpp` | `kinetra_menu_tests` | `^kinetra_menu:` |
| Any `session.cpp`, presentation, launcher loop, `main.cpp` | Full level with graphics | |
| `external/Kinetra` pointer or engine sources | Full level, plus Kinetra's own checks | |

The Demo, Dotio, i4sim and settings tests are doctest suites: CTest names each case `<suite>:<case>`, so a regex that starts with `^<suite>` selects the whole suite and a longer one selects single cases. Add `-LE soak` to skip the two 60-second Demo crowd cases.

The level is decided by the whole change; one Full-level file lifts everything. In the `dev` preset (Debug), the large-factory cases of `kinetra_i4sim` and the Demo crowd soak cases take minutes. For a quick targeted run, use the `release` preset.

**Warnings.** `KINETRA_LAB_WARNINGS` (on in `dev` and `ci`) reports the shared warning set for `labs/` and `launcher/`. Engine warnings are errors in CI.
- Lab code had 119 warnings when this was introduced. New or changed Lab code must not add any.
- When a roadmap phase makes a module warning-free, add `kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/labs/<module> RECURSE AS_ERRORS)` to the root `CMakeLists.txt`, so CI keeps it that way.

## Git with the engine submodule

- Lab records one exact Kinetra commit. A change to `external/Kinetra` is committed **in Kinetra** on its own branch, then the pointer is committed in Lab (`git add external/Kinetra`).
- Never commit a pointer to a Kinetra commit that is not on a Kinetra branch. Push the Kinetra branch and open its pull request before pushing Lab.
- The Lab pull request links the Kinetra pull request. It states that Kinetra merges first, with a merge commit rather than a squash, so the pointer stays reachable.
- The manual steps and commands are in [External dependencies](external/README.md).

## Documentation in Lab

- **Module READMEs:** `launcher/README.md`, `labs/demo/README.md`, `labs/dotio/README.md` and `labs/i4sim/README.md` are the module READMEs. They follow the [module template](external/Kinetra/docs/documentation.md#module-readme-template).
- **Thin sub-READMEs:** the navigation-only READMEs in `include/`, `src/` and `tests/` subdirectories are folded into their module README when the roadmap reaches that module.
- **Architecture pages:** the cross-application pages live in [docs/](docs/README.md).
- **Design documents:** Lab design documents go in [docs/design/](docs/design/README.md).
- **ADRs:** Lab ADRs go in `docs/decisions/`, created with its README when the first Lab ADR is written.
- **Media:** screenshots and the tour video in [docs/media](docs/media/README.md) are refreshed when visible behavior changes.

## Functionality index

| Application | Implementation | Tests | Documentation |
| --- | --- | --- | --- |
| Bundled launcher | [launcher/src](launcher/src/), [registrations](launcher/include/applications.h) | [launcher/tests](launcher/tests/) | [launcher](launcher/README.md), [launcher and overlays](docs/launcher-overlays.md) |
| Demo | [labs/demo](labs/demo/) | [labs/demo/tests](labs/demo/tests/) | [Demo](labs/demo/README.md), [architecture](docs/demo-architecture.md) |
| Dotio | [labs/dotio](labs/dotio/) | [labs/dotio/tests](labs/dotio/tests/) | [Dotio](labs/dotio/README.md), [architecture](docs/dotio-architecture.md) |
| i4sim | [labs/i4sim](labs/i4sim/) | [labs/i4sim/tests](labs/i4sim/tests/) | [i4sim](labs/i4sim/README.md), [architecture](docs/factory-simulation-architecture.md) |
| Shared Lab policy (settings rows, input, pause, session base) | [labs/common](labs/common/include/lab/) | [common tests](labs/common/tests/common_tests.cpp); the overlay itself in Kinetra | [launcher and overlays](docs/launcher-overlays.md) |
| Build | [CMakeLists.txt](CMakeLists.txt), [build.sh](build.sh) | `ctest --test-dir build` | [README](README.md#quick-start) |
