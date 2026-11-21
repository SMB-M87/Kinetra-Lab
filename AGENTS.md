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
| Logic: headless rules, state, window-free input mapping and debug-menu rows | `kinetra_demo_logic`, `kinetra_dotio_logic`, `kinetra_i4sim_logic` | `Kinetra::Simulation`, and the headless Lab policy in `kinetra_lab/input.h` and `kinetra_lab/debug.h` |
| Presentation: session, panels, drawing | `kinetra_demo_presentation`, `kinetra_dotio_presentation`, `kinetra_i4sim_presentation` | Its own logic, `Kinetra::Runtime`, shared Lab helpers, and `Kinetra::Launcher` for its menu widgets |
| Composition roots | `kinetra`, `kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim`, `kinetra_sessions`, and the developer tool `kinetra_capture` | Presentation libraries, `Kinetra::Launcher`; `kinetra_capture` also `kinetra_sessions` and zlib |

- Input mapping that needs no window belongs in the logic layer, where it is tested headless: Demo's `demo/input/mapping.h` and i4sim's `FactoryController`. So do the rows of a debug menu, as `kinetra::DebugEntry` values: Demo's `DebugPanel`, Dotio's `Application::debug_entries()` and `FactoryController::menu()`. From `kinetra_lab`, logic may include only `kinetra_lab/input.h` and `kinetra_lab/debug.h`, which use Kinetra's plain input, camera and menu types; `kinetra_lab/settings.h` and `kinetra_lab/session.h` need the runtime.
- A presentation may link `Kinetra::Launcher` to reuse its menu widgets, as i4sim does with `LaunchMenu` for its blueprint screen. It never depends on the bundled launcher in `launcher/`.
- Applications never include each other's headers. Shared Lab code is application-neutral: the Lab policy in [labs/shared](labs/shared/include/kinetra_lab/) (target `kinetra_lab`) and the test helpers in [kinetra_lab/testing/test_support.h](labs/shared/tests/include/kinetra_lab/testing/test_support.h) (target `kinetra_lab_testing`).
- Files follow the [directory layout](external/Kinetra/docs/conventions.md#directory-layout): every module has one `include/<name>/` (`demo`, `dotio`, `i4sim`, `kinetra_lab`, `launcher`, `capture`) and one `src/` that mirrors it, presentation code in `presentation/`, every header that is not an entry of its module in a topic folder (at least two files per topic), internal headers in `detail/`, test helpers in `tests/include/<name>/testing/`. Include as `"i4sim/simulation/simulation.h"`; never `../`.
- Logic must build and test with `-DKINETRA_LAB_BUILD_RUNTIME=OFF`.
- i4sim needs the header-only nlohmann JSON package (3.11+). Its blueprints live in `labs/i4sim/blueprints/`; CMake copies them to `i4sim-blueprints/` next to every executable that opens them, and the application reads them from there. Treat blueprint files as a data format: changing their fields is a contract change.

*Why:* headless logic is what makes simulation behavior testable without a window, and the dependency direction keeps applications replaceable.

## Verification in Lab

Use the levels from [Kinetra testing](external/Kinetra/docs/testing.md#verification-levels):

| Level | In Lab |
| --- | --- |
| Docs | `python3 external/Kinetra/tools/check_docs.py` |
| Targeted | `cmake --build --preset dev --target <targets>` and `ctest --preset dev -R '<regex>'`, using the table below (configure once with `cmake --preset dev`) |
| Full | Locally `cmake --workflow --preset graphics` and the smoke runs in the [README](README.md#tests-and-resources); then the pull request's CI (format, docs, `headless`, `ci`, native Windows). Verified only when both pass. Run `ci`, `headless` or `windows` locally only to reproduce a CI failure ([Kinetra testing](external/Kinetra/docs/testing.md#full)) |

| Changed area | Targets | CTest regex |
| --- | --- | --- |
| `labs/i4sim/` logic (`simulation/`, `blueprint/`, `controller`) | `kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim` | `^kinetra_i4sim` |
| `labs/demo/` logic (`application`, `debug_panel`, `world/`, `input/`) | `kinetra_demo_tests kinetra_demo_terrain_tests kinetra_demo_contact_tests kinetra_demo_crowd_tests kinetra_demo_crowd_characterization_tests` | `^kinetra_demo(:\|_terrain\|_contacts\|_crowd)` |
| `labs/dotio/` logic (everything outside `presentation/`) | `kinetra_dotio_tests` | `^kinetra_dotio:` |
| `labs/shared/` | `kinetra_lab_tests` | `^kinetra_lab:` |
| `launcher/include/launcher/applications.h`, `launcher/src/applications.cpp` | `kinetra_menu_tests` | `^kinetra_menu:` |
| `tools/capture/` | `kinetra_capture_tests kinetra_capture`; then the smoke test in the `graphics` preset (`ctest --preset graphics -R kinetra_capture_smoke`) | `^kinetra_capture` |
| Any `presentation/` folder, launcher loop, `main.cpp` | Full level with graphics | |
| `external/Kinetra` pointer or engine sources | Full level, plus Kinetra's own checks | |
| `tools/release/`, `.github/workflows/release.yml` | Docs level, plus the local cross build and `package_windows.sh` from the [release tool](tools/release/README.md#build-the-zip-locally); the workflow itself runs only after a merge on `main` or a tag | |
| Windows build (`cmake/toolchains/`, `cmake/WindowsDependencies.cmake`, Windows presets, `#ifdef _WIN32` code) | Full level; CI's `windows` job builds natively. Only a change to the WSL cross build itself (`cmake/toolchains/`, the `windows` preset) also needs `cmake --workflow --preset windows` from WSL, which CI does not run ([Windows](docs/windows.md)) | |

The Demo, Dotio, i4sim and settings tests are doctest suites: CTest names each case `<suite>:<case>`, so a regex that starts with `^<suite>` selects the whole suite and a longer one selects single cases. Add `-LE soak` to skip the five 60-second Demo crowd cases.

The level is decided by the whole change; one Full-level file lifts everything. In the `dev` preset (Debug), the large-factory cases of `kinetra_i4sim` and the Demo crowd soak cases take minutes. For a quick targeted run, use the `release` preset.

**Warnings.** `KINETRA_LAB_WARNINGS` (on in `dev` and `ci`) reports the shared warning set for `labs/` and `launcher/`. Engine warnings are errors in CI.
- Lab code had 119 warnings when this was introduced. New or changed Lab code must not add any.
- When a roadmap phase makes a module warning-free, add `kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/labs/<module> RECURSE AS_ERRORS)` to the root `CMakeLists.txt`, so CI keeps it that way.

## Git with the engine submodule

- Before starting work on a local branch, pull it first (`git pull --ff-only`; the HTTPS fallback from the global Git rules when SSH is unavailable), in Lab and in `external/Kinetra`. A local branch can lag behind its remote, for example `main` after a merged pull request or a feature branch after review commits.
- Lab records one exact Kinetra commit. A change to `external/Kinetra` is committed **in Kinetra** on its own branch, then the pointer is committed in Lab (`git add external/Kinetra`).
- Never commit a pointer to a Kinetra commit that is not on a Kinetra branch. Push the Kinetra branch and open its pull request before pushing Lab.
- The Lab pull request links the Kinetra pull request. It states that Kinetra merges first, with a merge commit rather than a squash, so the pointer stays reachable.
- Lab `main` records a commit on Kinetra `main`. Once the Kinetra pull request is merged, move the pointer on the Lab branch to the Kinetra merge commit and push it before the Lab pull request is merged. When the trees are identical (`git -C external/Kinetra diff --stat <old> <new>` is empty) the earlier verification stands; otherwise verify again. If the Lab pull request was merged first, record Kinetra `main` in a small pointer pull request right away. *Why:* a pointer to the branch commit is reachable but not on `main`, so the submodule on `main` shows as modified in Lab.
- The manual steps and commands are in [External dependencies](external/README.md).

## Documentation in Lab

- **Module READMEs:** `launcher/README.md`, `labs/demo/README.md`, `labs/dotio/README.md` and `labs/i4sim/README.md` are the module READMEs. They follow the [module template](external/Kinetra/docs/documentation.md#module-readme-template).
- **Thin sub-READMEs:** the navigation-only READMEs in `include/`, `src/` and `tests/` subdirectories are folded into their module README when the roadmap reaches that module.
- **Architecture pages:** the cross-application pages live in [docs/](docs/README.md).
- **Design documents:** Lab design documents go in [docs/design/](docs/design/README.md).
- **ADRs:** Lab ADRs go in `docs/decisions/`, created with its README when the first Lab ADR is written.
- **Media:** screenshots and clips in [docs/media](docs/media/README.md) are refreshed with `tools/capture/capture_media.sh` ([capture tool](tools/capture/README.md)) when visible behavior changes; look at every refreshed file before committing it. The tour video is rendered with `tools/capture/capture_tour.sh`; look at frames from every chapter before committing it.

## Functionality index

| Application | Implementation | Tests | Documentation |
| --- | --- | --- | --- |
| Bundled launcher | [launcher/src](launcher/src/), [registrations](launcher/include/launcher/applications.h) | [launcher/tests](launcher/tests/) | [launcher](launcher/README.md), [launcher and overlays](docs/launcher-overlays.md) |
| Demo | [labs/demo](labs/demo/) | [labs/demo/tests](labs/demo/tests/) | [Demo](labs/demo/README.md), [architecture](docs/demo-architecture.md) |
| Dotio | [labs/dotio](labs/dotio/) | [labs/dotio/tests](labs/dotio/tests/) | [Dotio](labs/dotio/README.md) |
| i4sim | [labs/i4sim](labs/i4sim/) | [labs/i4sim/tests](labs/i4sim/tests/) | [i4sim](labs/i4sim/README.md), [architecture](docs/factory-simulation-architecture.md) |
| Shared Lab policy (settings rows, debug rows, input, pause, session base) | [labs/shared](labs/shared/include/kinetra_lab/) | [shared tests](labs/shared/tests/shared_tests.cpp); the overlay itself in Kinetra | [launcher and overlays](docs/launcher-overlays.md) |
| Build | [CMakeLists.txt](CMakeLists.txt), [build.sh](build.sh) | `ctest --test-dir build` | [README](README.md#quick-start) |
| Windows release zip | [tools/release](tools/release/), [release.yml](.github/workflows/release.yml) | the workflow's DLL import check | [release tool](tools/release/README.md) |
| Media capture (screenshots and GIF clips) | [tools/capture](tools/capture/) | [capture tests](tools/capture/tests/) | [capture tool](tools/capture/README.md), [media guide](docs/media/README.md) |
